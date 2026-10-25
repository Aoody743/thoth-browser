/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Atomic.h>
#include <AK/AtomicRefCounted.h>
#include <AK/Format.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Platform.h>
#include <AK/ScopeGuard.h>
#include <AK/Vector.h>
#include <AudioServer/PlatformAudio.h>
#include <AudioServer/RecordStreamWASAPI.h>
#include <AudioServer/WASAPIHelpers.h>
#include <LibAudio/ChannelMap.h>
#include <LibAudio/SampleSpecification.h>
#include <LibCore/Promise.h>
#include <LibThreading/Thread.h>
#include <string>

#include <AK/Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

// NOTE: Not using the newer winrt that supersedes wrl as that uses exceptions for error handling
#include <wrl/client.h>

namespace Audio {

using namespace Microsoft::WRL;

#define TRY_HR(expression)                                                            ({                                                                                    AK_IGNORE_DIAGNOSTIC("-Wshadow", HRESULT&& _temporary_hr = (expression));         if (FAILED(_temporary_hr)) [[unlikely]] {                                             promise->reject(Error::from_windows_error(_temporary_hr));                        return promise;                                                               }                                                                             })

struct RecordStreamWASAPI::CaptureState : public AtomicRefCounted<RecordStreamWASAPI::CaptureState> {
    CaptureState() = default;
    ~CaptureState()
    {
        exit_requested.store(true);
        if (buffer_event)
            CloseHandle(buffer_event);
        if (com_initialized.load())
            CoUninitialize();
    }

    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> audio_client;
    ComPtr<IAudioCaptureClient> capture_client;

    HANDLE buffer_event { nullptr };

    RecordStream::RecordCallback record_callback;
    Atomic<bool> exit_requested { false };
    Atomic<bool> com_initialized { false };
};

static ErrorOr<SampleSpecification> capture_specification_for(WAVEFORMATEX const* format)
{
    // WASAPI shared mode mixes to IEEE float 32; anything else would need conversion this
    // backend does not implement, so refuse it and let the caller fall back.
    auto is_float = false;
    u32 sample_rate = format->nSamplesPerSec;
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float = format->wBitsPerSample == 32;
    } else if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto const* extensible = reinterpret_cast<WAVEFORMATEXTENSIBLE const*>(format);
        is_float = IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) && extensible->Format.wBitsPerSample == 32;
    }
    if (!is_float || format->nChannels == 0)
        return Error::from_string_literal("Capture mix format is not 32-bit float");

    auto channel_map = TRY(convert_bitmask_to_channel_map(reinterpret_cast<WAVEFORMATEXTENSIBLE const*>(format)->dwChannelMask));
    return SampleSpecification { sample_rate, channel_map };
}

static void capture_thread_loop(RecordStreamWASAPI::CaptureState& state, SampleSpecification const& specification)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
        state.com_initialized.store(true);
    if (FAILED(hr)) {
        dbgln("RecordStreamWASAPI: CoInitializeEx failed: {}", Error::from_windows_error(hr));
        return;
    }

    hr = state.audio_client->Start();
    if (FAILED(hr)) {
        dbgln("RecordStreamWASAPI: Start failed: {}", Error::from_windows_error(hr));
        return;
    }

    while (!state.exit_requested.load()) {
        auto wait_result = WaitForSingleObject(state.buffer_event, 2000);
        if (state.exit_requested.load())
            break;
        if (wait_result != WAIT_OBJECT_0)
            continue;

        for (;;) {
            UINT32 packet_size = 0;
            if (FAILED(state.capture_client->GetNextPacketSize(&packet_size)) || packet_size == 0)
                break;

            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(state.capture_client->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                break;

            // Silent packets carry no data; deliver zeros so the consumer keeps a continuous timeline.
            if (frames > 0) {
                size_t byte_count = static_cast<size_t>(frames) * specification.channel_count() * sizeof(float);
                if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !data) {
                    Vector<BYTE> silence;
                    if (!silence.try_resize(byte_count).is_error()) {
                        memset(silence.data(), 0, byte_count);
                        state.record_callback({ silence.data(), silence.size() }, specification);
                    }
                } else {
                    state.record_callback({ data, byte_count }, specification);
                }
            }

            if (FAILED(state.capture_client->ReleaseBuffer(frames)))
                break;
        }
    }

    state.audio_client->Stop();
}

NonnullRefPtr<RecordStream::CreatePromise> RecordStreamWASAPI::create(SampleSpecification const&, u32, StringView device_id, RecordCallback&& record_callback)
{
    auto promise = CreatePromise::construct();

    auto state = adopt_ref(*new CaptureState());

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
        state->com_initialized.store(true);
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    if (device_id.is_empty()) {
        hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &state->device);
    } else {
        // The device id is a UTF-8 copy of the WASAPI symbolic link; convert it back.
        auto wide_length = MultiByteToWideChar(CP_UTF8, 0, device_id.characters_without_null_termination(),
            static_cast<int>(device_id.length()), nullptr, 0);
        std::wstring wide_device_id(static_cast<size_t>(wide_length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, device_id.characters_without_null_termination(),
            static_cast<int>(device_id.length()), wide_device_id.data(), wide_length);
        hr = enumerator->GetDevice(wide_device_id.c_str(), &state->device);
    }
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    hr = state->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&state->audio_client);
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    WAVEFORMATEX* mix_format = nullptr;
    hr = state->audio_client->GetMixFormat(&mix_format);
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }
    ScopeGuard free_format = [&] { CoTaskMemFree(mix_format); };

    auto specification = capture_specification_for(mix_format);
    if (specification.is_error()) {
        promise->reject(specification.release_error());
        return promise;
    }

    // A one-second shared-mode buffer; capture drains it on every event, so latency stays low.
    hr = state->audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        10'000'000LL, 0, mix_format, nullptr);
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    state->buffer_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!state->buffer_event) {
        promise->reject(Error::from_windows_error(GetLastError()));
        return promise;
    }

    hr = state->audio_client->SetEventHandle(state->buffer_event);
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    hr = state->audio_client->GetService(IID_PPV_ARGS(&state->capture_client));
    if (FAILED(hr)) {
        promise->reject(Error::from_windows_error(hr));
        return promise;
    }

    state->record_callback = move(record_callback);

    auto specification_for_thread = specification.release_value();
    auto thread_state = NonnullRefPtr<CaptureState> { *state };
    auto capture_thread = Threading::Thread::construct("WASAPI Capture"sv, [&thread_state, specification_for_thread] {
        capture_thread_loop(*thread_state, specification_for_thread);
        return 0;
    });
    capture_thread->start();
    capture_thread->detach();

    promise->resolve(adopt_ref(*new (nothrow) RecordStreamWASAPI(move(state), specification_for_thread)));
    return promise;

#undef TRY_HR
}

RecordStreamWASAPI::RecordStreamWASAPI(NonnullRefPtr<CaptureState> state, SampleSpecification specification)
    : m_state(move(state))
    , m_sample_specification(specification)
{
}

RecordStreamWASAPI::~RecordStreamWASAPI() = default;

}

namespace Audio {

// PlatformAudio dispatch point; the fallback in PlatformAudio.cpp is compiled out when
// AUDIOSERVER_CAPTURE_BACKEND is defined.
NonnullRefPtr<RecordStream::CreatePromise> create_platform_record_stream(SampleSpecification const& requested_specification, u32 fragment_size_bytes, StringView device_id, RecordStream::RecordCallback callback)
{
    return RecordStreamWASAPI::create(requested_specification, fragment_size_bytes, device_id, move(callback));
}

}
