/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "CameraCapture.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <atomic>
#include <new>

#include <AK/Platform.h>
#if defined(AK_OS_WINDOWS)

// NB: Media Foundation delivers camera frames to the browser process on Windows, the same job
//     AVCaptureSession does on macOS. Device ids are the VIDCAP symbolic links, which never
//     leave this process (the view hashes them per-origin before exposing them to web content).

#include <guiddef.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// NOTE: Not using the newer winrt that supersedes wrl as that uses exceptions for error handling
#include <wrl/client.h>

using namespace Microsoft::WRL;

namespace {

constexpr auto camera_frame_freshness_ms = 1'000;

class CameraFrameState {
public:
    void store(QImage frame)
    {
        QMutexLocker locker(&m_mutex);
        m_frame = std::move(frame);
        m_captured_at = QDateTime::currentMSecsSinceEpoch();
    }

    QImage load() const
    {
        QMutexLocker locker(&m_mutex);
        if (m_frame.isNull() || QDateTime::currentMSecsSinceEpoch() - m_captured_at > camera_frame_freshness_ms)
            return {};
        return m_frame;
    }

private:
    mutable QMutex m_mutex;
    QImage m_frame;
    qint64 m_captured_at { 0 };
};

QImage convert_frame_to_image(IMFSample& sample, IMFMediaType& media_type)
{
    GUID subtype {};
    UINT32 width = 0, height = 0;
    if (FAILED(media_type.GetGUID(MF_MT_SUBTYPE, &subtype)))
        return {};
    if (FAILED(MFGetAttributeSize(&media_type, MF_MT_FRAME_SIZE, &width, &height)))
        return {};

    ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample.ConvertToContiguousBuffer(&buffer)))
        return {};

    BYTE* data = nullptr;
    DWORD max_length = 0, current_length = 0;
    if (FAILED(buffer->Lock(&data, &max_length, &current_length)))
        return {};
    auto unlock = [&] { buffer->Unlock(); };

    QImage result;
    if (subtype == MFVideoFormat_RGB32) {
        QImage direct(data, width, height, 4 * width, QImage::Format_ARGB32);
        result = direct.copy();
    } else if (subtype == MFVideoFormat_NV12) {
        QImage converted(width, height, QImage::Format_ARGB32);
        auto* out = reinterpret_cast<QRgb*>(converted.bits());
        auto const* y_plane = data;
        auto const* uv_plane = data + static_cast<size_t>(width) * height;
        for (UINT32 row = 0; row < height; ++row) {
            for (UINT32 column = 0; column < width; ++column) {
                auto y = y_plane[row * width + column];
                auto uv_index = (row / 2) * width + (column / 2) * 2;
                auto u = uv_plane[uv_index] - 128;
                auto v = uv_plane[uv_index + 1] - 128;
                auto r = qBound(0, y + ((91881 * v) >> 14), 255);
                auto g = qBound(0, y - ((22554 * u + 46802 * v) >> 14), 255);
                auto b = qBound(0, y + ((116130 * u) >> 14), 255);
                out[row * width + column] = qRgb(r, g, b);
            }
        }
        result = std::move(converted);
    } else if (subtype == MFVideoFormat_MJPG) {
        QImage decoded;
        if (decoded.loadFromData(data, current_length, "JPEG"))
            result = std::move(decoded);
    }
    unlock();
    return result;
}

class CameraReaderCallback final : public IMFSourceReaderCallback {
public:
    explicit CameraReaderCallback(std::shared_ptr<CameraFrameState> frames)
        : m_frames(std::move(frames))
    {
    }

    QImage latest_frame() const { return m_frames->load(); }

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
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, __uuidof(IMFSourceReaderCallback))) {
            *out = static_cast<IMFSourceReaderCallback*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHOD(OnReadSample)(HRESULT status, DWORD, DWORD, LONGLONG, IMFSample* sample) override
    {
        if (SUCCEEDED(status) && sample && !m_stopped.load()) {
            if (auto image = convert_frame_to_image(*sample, *m_media_type.Get()); !image.isNull())
                m_frames->store(std::move(image));
        }
        if (!m_stopped.load() && m_reader) {
            HRESULT hr = m_reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
            if (FAILED(hr))
                m_stopped.store(true);
        }
        return S_OK;
    }

    STDMETHOD(OnFlush)(DWORD) override { return S_OK; }
    STDMETHOD(OnEvent)(DWORD, IMFMediaEvent*) override { return S_OK; }

    void set_reader(IMFSourceReader& reader, IMFMediaType& media_type)
    {
        m_reader = &reader;
        m_media_type = &media_type;
    }

    void stop() { m_stopped.store(true); }

private:
    std::shared_ptr<CameraFrameState> m_frames;
    ComPtr<IMFSourceReader> m_reader;
    ComPtr<IMFMediaType> m_media_type;
    std::atomic<int> m_ref_count { 0 };
    std::atomic<bool> m_stopped { false };
};

}

namespace Thoth {

struct CameraCapture::Impl {
    std::shared_ptr<CameraFrameState> frames { std::make_shared<CameraFrameState>() };
    ComPtr<IMFMediaSource> source;
    ComPtr<IMFSourceReader> reader;
    ComPtr<CameraReaderCallback> callback;
    int best_format_distance { INT32_MAX };
    bool started { false };
};

void CameraCapture::request_access(std::function<void(bool)> callback)
{
    // Windows has no per-app camera consent API; the system-wide privacy switch is enforced by
    // the capture stack itself (device activation fails when the switch is off).
    QTimer::singleShot(0, [callback = std::move(callback)] { callback(true); });
}

QJsonArray CameraCapture::devices()
{
    QJsonArray devices;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool com_initialized = SUCCEEDED(hr);
    if (!com_initialized && hr != RPC_E_CHANGED_MODE)
        return devices;

    IMFAttributes* attributes = nullptr;
    if (FAILED(MFCreateAttributes(&attributes, 1))) {
        if (com_initialized)
            CoUninitialize();
        return devices;
    }
    (void)attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** activations = nullptr;
    UINT32 count = 0;
    if (SUCCEEDED(MFEnumDeviceSources(attributes, &activations, &count))) {
        for (UINT32 index = 0; index < count; ++index) {
            auto* activation = activations[index];

            WCHAR* symbolic_link = nullptr;
            UINT32 link_length = 0;
            if (FAILED(activation->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symbolic_link, &link_length)))
                continue;
            QString id = QString::fromWCharArray(symbolic_link, link_length);
            CoTaskMemFree(symbolic_link);

            WCHAR* friendly_name = nullptr;
            UINT32 name_length = 0;
            QString label = QStringLiteral("Camera");
            if (SUCCEEDED(activation->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendly_name, &name_length))) {
                label = QString::fromWCharArray(friendly_name, name_length);
                CoTaskMemFree(friendly_name);
            }

            // The format list needs the source, which only one process instance can hold at a
            // time in exclusive mode; shared readers allow this brief probe.
            ComPtr<IMFMediaSource> source;
            QJsonArray formats;
            if (SUCCEEDED(activation->ActivateObject(IID_PPV_ARGS(&source)))) {
                ComPtr<IMFPresentationDescriptor> descriptor;
                if (SUCCEEDED(source->CreatePresentationDescriptor(&descriptor))) {
                    ComPtr<IMFMediaTypeHandler> handler;
                    DWORD stream_count = 0;
                    if (SUCCEEDED(descriptor->GetStreamDescriptorCount(&stream_count)) && stream_count > 0) {
                        BOOL selected = FALSE;
                        ComPtr<IMFStreamDescriptor> stream;
                        if (SUCCEEDED(descriptor->GetStreamDescriptorByIndex(0, &selected, &stream)) && SUCCEEDED(stream->GetMediaTypeHandler(&handler))) {
                            DWORD type_count = 0;
                            if (SUCCEEDED(handler->GetMediaTypeCount(&type_count))) {
                                for (DWORD type_index = 0; type_index < type_count; ++type_index) {
                                    ComPtr<IMFMediaType> media_type;
                                    if (FAILED(handler->GetMediaTypeByIndex(type_index, &media_type)))
                                        continue;
                                    UINT32 width = 0, height = 0;
                                    if (FAILED(MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &width, &height)))
                                        continue;
                                    UINT32 numerator = 0, denominator = 0;
                                    if (FAILED(MFGetAttributeRatio(media_type.Get(), MF_MT_FRAME_RATE, &numerator, &denominator)) || denominator == 0)
                                        continue;
                                    double rate = static_cast<double>(numerator) / denominator;
                                    if (rate < 1.0)
                                        continue;
                                    formats.append(QJsonObject {
                                        { QStringLiteral("width"), static_cast<int>(width) },
                                        { QStringLiteral("height"), static_cast<int>(height) },
                                        { QStringLiteral("minFrameRate"), std::min(rate, 60.0) },
                                        { QStringLiteral("maxFrameRate"), std::min(rate, 60.0) },
                                    });
                                }
                            }
                        }
                    }
                }
                source->Shutdown();
            }
            devices.append(QJsonObject {
                { QStringLiteral("id"), id },
                { QStringLiteral("label"), label },
                { QStringLiteral("formats"), formats },
            });
        }
        for (UINT32 index = 0; index < count; ++index)
            activations[index]->Release();
        CoTaskMemFree(activations);
    }
    attributes->Release();

    if (com_initialized)
        CoUninitialize();
    return devices;
}

CameraCapture::CameraCapture(QString device_id)
    : m_impl(std::make_unique<Impl>())
{
    (void)device_id; // The default device is used; per-device selection lands with multi-camera support.
}

CameraCapture::~CameraCapture()
{
    stop();
}

bool CameraCapture::start(QJsonObject const& configuration, QString& error)
{
    stop();
    error.clear();
    m_impl->best_format_distance = INT32_MAX;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool com_initialized = SUCCEEDED(hr);
    if (!com_initialized && hr != RPC_E_CHANGED_MODE) {
        error = QStringLiteral("Could not initialize COM (%1).").arg(hr);
        return false;
    }

    int width = configuration.value("width").toInt();
    int height = configuration.value("height").toInt();
    double frame_rate = configuration.value("frameRate").toDouble();
    if (width <= 0 || height <= 0) {
        width = 640;
        height = 480;
    }

    IMFAttributes* attributes = nullptr;
    if (FAILED(MFCreateAttributes(&attributes, 2))) {
        error = QStringLiteral("Could not create camera attributes.");
        return false;
    }
    (void)attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFMediaSource* source = nullptr;
    if (FAILED(MFCreateDeviceSource(attributes, &source))) {
        attributes->Release();
        error = QStringLiteral("Could not open the camera. Check Windows privacy settings for camera access.");
        return false;
    }
    m_impl->source = source;
    source->Release();
    attributes->Release();

    IMFAttributes* reader_attributes = nullptr;
    if (FAILED(MFCreateAttributes(&reader_attributes, 1)) || FAILED(reader_attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, m_impl->callback.Get()))) {
        error = QStringLiteral("Could not create the camera reader.");
        return false;
    }
    reader_attributes->Release();

    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromMediaSource(m_impl->source.Get(), reader_attributes, &reader);
    if (FAILED(hr)) {
        error = QStringLiteral("Could not create the camera reader (%1).").arg(hr);
        return false;
    }

    ComPtr<IMFMediaType> native_type;
    ComPtr<IMFMediaType> media_type;
    for (DWORD index = 0; ; ++index) {
        ComPtr<IMFMediaType> candidate;
        if (FAILED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, &candidate)))
            break;
        UINT32 candidate_width = 0, candidate_height = 0;
        if (FAILED(MFGetAttributeSize(candidate.Get(), MF_MT_FRAME_SIZE, &candidate_width, &candidate_height)))
            continue;
        GUID subtype {};
        (void)candidate->GetGUID(MF_MT_SUBTYPE, &subtype);
        bool convertible = subtype == MFVideoFormat_RGB32 || subtype == MFVideoFormat_NV12 || subtype == MFVideoFormat_MJPG;
        if (!convertible)
            continue;

        UINT32 numerator = 0, denominator = 0;
        double candidate_rate = 30.0;
        if (SUCCEEDED(MFGetAttributeRatio(candidate.Get(), MF_MT_FRAME_RATE, &numerator, &denominator)) && denominator != 0)
            candidate_rate = static_cast<double>(numerator) / denominator;

        auto distance = qAbs<int>(candidate_width * candidate_height - width * height)
            + static_cast<int>(qAbs(candidate_rate - (frame_rate > 0 ? frame_rate : 30.0)) * 1000);
        if (!native_type || distance < m_impl->best_format_distance) {
            m_impl->best_format_distance = distance;
            native_type = candidate;
            media_type = candidate;
        }
    }
    if (!media_type) {
        error = QStringLiteral("The camera offers no supported format.");
        return false;
    }
    // The reader converts natively; request RGB32 output so frame conversion is a copy.
    ComPtr<IMFMediaType> rgb_type;
    if (SUCCEEDED(MFCreateMediaType(&rgb_type))) {
        (void)rgb_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        (void)rgb_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        (void)MFSetAttributeSize(rgb_type.Get(), MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
        if (FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, rgb_type.Get())))
            (void)reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, media_type.Get());
    }

    ComPtr<IMFMediaType> current_type;
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current_type);
    m_impl->callback = ComPtr<CameraReaderCallback>(new (std::nothrow) CameraReaderCallback(m_impl->frames));
    m_impl->callback->set_reader(*reader.Get(), *current_type.Get());

    if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr))) {
        error = QStringLiteral("The camera did not deliver its first frame.");
        return false;
    }

    m_impl->reader = reader;
    m_impl->started = true;
    return true;
}

QImage CameraCapture::latest_frame() const
{
    if (!m_impl->callback)
        return {};
    return m_impl->callback->latest_frame();
}

void CameraCapture::stop()
{
    if (!m_impl)
        return;
    if (m_impl->callback)
        m_impl->callback->stop();
    if (m_impl->reader)
        m_impl->reader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    if (m_impl->source) {
        m_impl->source->Shutdown();
        m_impl->source.Reset();
    }
    m_impl->reader.Reset();
    m_impl->callback.Reset();
    m_impl->started = false;
}

}

#endif
