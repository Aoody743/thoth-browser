/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <AK/Utf16View.h>
#include <AK/Error.h>
#include <AK/NeverDestroyed.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Platform.h>
#include <AK/StringBuilder.h>
#include <AK/StringView.h>
#include <AudioServer/PlatformAudio.h>
#include <AudioServer/WASAPIHelpers.h>
#include <LibAudio/AudioDevices.h>
#include <LibCore/EventLoop.h>
#include <LibThreading/ThreadPool.h>

#include <AK/Windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

// NOTE: Not using the newer winrt that supersedes wrl as that uses exceptions for error handling
#include <wrl/client.h>

namespace Audio {

using namespace Microsoft::WRL;

class ComUninitializer {
public:
    ~ComUninitializer()
    {
        if (initialized)
            CoUninitialize();
    }
    bool initialized = false;
};

static thread_local ComUninitializer s_com_uninitializer {};

static ByteString wide_to_byte_string(wchar_t const* wide)
{
    auto utf8 = Utf16View { reinterpret_cast<char16_t const*>(wide), wcslen(wide) }.to_utf8();
    if (utf8.is_error())
        return ByteString { "Audio device" };
    return ByteString { utf8.value().bytes_as_string_view() };
}

static ErrorOr<ByteString> device_friendly_name(IMMDevice& device)
{
    ComPtr<IPropertyStore> store;
    HRESULT hr = device.OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr))
        return Error::from_windows_error(hr);

    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(PKEY_Device_FriendlyName, &value);
    if (FAILED(hr))
        return Error::from_windows_error(hr);
    VERIFY(value.vt == VT_LPWSTR);

    return wide_to_byte_string(value.pwszVal);
}

static ErrorOr<AudioDeviceEnumeration> enumerate_wasapi_audio_devices()
{
    if (!s_com_uninitializer.initialized) {
        HRESULT init_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(init_hr))
            s_com_uninitializer.initialized = true;
        else if (init_hr != RPC_E_CHANGED_MODE)
            return Error::from_windows_error(init_hr);
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr))
        return Error::from_windows_error(hr);

    AudioDeviceEnumeration enumeration;
    ComPtr<IMMDevice> default_render;
    ComPtr<IMMDevice> default_capture;
    (void)enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_render);
    (void)enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &default_capture);

    auto default_render_id = ByteString {};
    auto default_capture_id = ByteString {};
    if (default_render) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(default_render->GetId(&id))) {
            default_render_id = wide_to_byte_string(id);
            CoTaskMemFree(id);
        }
    }
    if (default_capture) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(default_capture->GetId(&id))) {
            default_capture_id = wide_to_byte_string(id);
            CoTaskMemFree(id);
        }
    }

    auto collect = [&](EDataFlow flow, Vector<AudioDeviceInfo>& into) -> ErrorOr<void> {
        ComPtr<IMMDeviceCollection> collection;
        HRESULT collection_hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection);
        if (FAILED(collection_hr))
            return {};

        UINT32 count = 0;
        (void)collection->GetCount(&count);
        for (UINT32 index = 0; index < count; ++index) {
            ComPtr<IMMDevice> device;
            if (FAILED(collection->Item(index, &device)))
                continue;

            LPWSTR id = nullptr;
            if (FAILED(device->GetId(&id)))
                continue;
            ByteString device_id = wide_to_byte_string(id);
            CoTaskMemFree(id);

            auto name = device_friendly_name(*device.Get());
            AudioDeviceInfo info;
            info.dom_device_id = device_id;
            info.label = name.is_error() ? ByteString { "Audio device" } : name.release_value();
            info.group_id = info.dom_device_id;
            info.is_default = device_id == (flow == eRender ? default_render_id : default_capture_id);
            into.append(move(info));
        }
        return {};
    };

    TRY(collect(eRender, enumeration.outputs));
    TRY(collect(eCapture, enumeration.inputs));
    return enumeration;
}

struct AudioDeviceWatch {
    Core::EventLoop* event_loop { nullptr };
    AudioDeviceListCallback on_device_list;
};

// The watch lasts for the rest of the process.
static NeverDestroyed<AudioDeviceWatch> s_audio_device_watch;

class WASAPIDeviceNotificationClient final : public IMMNotificationClient {
public:
    static ComPtr<WASAPIDeviceNotificationClient> create()
    {
        return ComPtr<WASAPIDeviceNotificationClient>(new (nothrow) WASAPIDeviceNotificationClient());
    }

    // The OS calls these on its own thread; bounce back to the watching event loop and re-list there.
    STDMETHOD_(ULONG, AddRef)() override
    {
        return m_ref_count.fetch_add(1) + 1;
    }
    STDMETHOD_(ULONG, Release)() override
    {
        auto remaining = m_ref_count.fetch_sub(1) - 1;
        if (remaining == 0)
            delete this;
        return remaining;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** out) override
    {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IMMNotificationClient))) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHOD(OnDeviceAdded)(LPCWSTR) override { return S_OK; }
    STDMETHOD(OnDeviceRemoved)(LPCWSTR) override { return S_OK; }
    STDMETHOD(OnDeviceStateChanged)(LPCWSTR, DWORD) override { report_change(); return S_OK; }
    STDMETHOD(OnDefaultDeviceChanged)(EDataFlow, ERole, LPCWSTR) override { report_change(); return S_OK; }
    STDMETHOD(OnPropertyValueChanged)(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    WASAPIDeviceNotificationClient() = default;

    void report_change()
    {
        if (!s_audio_device_watch->event_loop)
            return;
        s_audio_device_watch->event_loop->deferred_invoke([] {
            Threading::ThreadPool::the().submit([] {
                auto enumeration = enumerate_wasapi_audio_devices();
                s_audio_device_watch->event_loop->deferred_invoke([enumeration = move(enumeration)]() mutable {
                    s_audio_device_watch->on_device_list(move(enumeration));
                });
            });
        });
    }

    Atomic<int> m_ref_count { 0 };
};

void watch_platform_audio_devices(AudioDeviceListCallback on_device_list)
{
    // Without an event loop there is nobody to tell of changes, so this is a one-off listing.
    if (!Core::EventLoop::is_running()) {
        on_device_list(enumerate_wasapi_audio_devices());
        return;
    }

    VERIFY(s_audio_device_watch->event_loop == nullptr);
    s_audio_device_watch->event_loop = &Core::EventLoop::current();
    s_audio_device_watch->on_device_list = move(on_device_list);

    Threading::ThreadPool::the().submit([] {
        auto enumerator = ComPtr<IMMDeviceEnumerator> {};
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
            auto client = WASAPIDeviceNotificationClient::create();
            // The client keeps itself alive: the OS holds a reference until the process ends.
            if (FAILED(enumerator->RegisterEndpointNotificationCallback(client.Get())))
                warnln("Unable to watch WASAPI devices for changes");
        }
        auto enumeration = enumerate_wasapi_audio_devices();
        s_audio_device_watch->event_loop->deferred_invoke([enumeration = move(enumeration)]() mutable {
            s_audio_device_watch->on_device_list(move(enumeration));
        });
    });
}

}
