/*
 * Copyright (c) 2026, Thoth Browser contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/Thoth/CameraCapture.h>

#include <QJsonObject>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

namespace {

struct CameraFrameState {
    std::mutex mutex;
    QImage image;
    QSize output_size;
    QSize native_size;
    std::chrono::steady_clock::time_point last_frame_at;
    std::atomic<bool> accepting_frames { true };
};

NSArray<AVCaptureDevice*>* video_devices()
{
    NSMutableArray<AVCaptureDeviceType>* types = [NSMutableArray arrayWithObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
    if (@available(macOS 14.0, *)) {
        [types addObject:AVCaptureDeviceTypeExternal];
        [types addObject:AVCaptureDeviceTypeContinuityCamera];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [types addObject:AVCaptureDeviceTypeExternalUnknown];
#pragma clang diagnostic pop
    }
    return [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:types mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionUnspecified].devices;
}

}

@interface ThothCameraFrameDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
@public
    std::shared_ptr<CameraFrameState> frame_state;
}
@end

@implementation ThothCameraFrameDelegate
- (void)captureOutput:(AVCaptureOutput*)output didOutputSampleBuffer:(CMSampleBufferRef)sample_buffer fromConnection:(AVCaptureConnection*)connection
{
    (void)output;
    (void)connection;
    auto state = frame_state;
    if (!state || !state->accepting_frames.load())
        return;
    CVPixelBufferRef buffer = CMSampleBufferGetImageBuffer(sample_buffer);
    if (!buffer || CVPixelBufferGetPixelFormatType(buffer) != kCVPixelFormatType_32BGRA)
        return;
    auto width = CVPixelBufferGetWidth(buffer);
    auto height = CVPixelBufferGetHeight(buffer);
    auto stride = CVPixelBufferGetBytesPerRow(buffer);
    // Bound frame memory, rather than excluding native 4K/8K camera formats.
    if (!width || !height || !stride || width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max() || stride > std::numeric_limits<int>::max() || height > 256 * 1024 * 1024 / stride)
        return;
    // Never disguise an unexpected native-mode downgrade by upscaling it.
    if (width != static_cast<size_t>(state->native_size.width()) || height != static_cast<size_t>(state->native_size.height()))
        return;
    if (CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
        return;
    auto* bytes = static_cast<uchar const*>(CVPixelBufferGetBaseAddress(buffer));
    QImage image;
    if (bytes)
        image = QImage(bytes, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride), QImage::Format_ARGB32).copy();
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    if (image.isNull())
        return;
    std::lock_guard lock(state->mutex);
    if (state->accepting_frames.load()) {
        state->image = std::move(image);
        state->last_frame_at = std::chrono::steady_clock::now();
    }
}
@end

namespace Thoth {

struct CameraCapture::Impl {
    QString device_id;
    std::mutex frames_mutex;
    std::shared_ptr<CameraFrameState> frames;
    dispatch_queue_t control_queue { dispatch_queue_create("org.thoth.camera.control", DISPATCH_QUEUE_SERIAL) };
    AVCaptureSession* session { nil };
    AVCaptureDevice* locked_device { nil };
    AVCaptureVideoDataOutput* output { nil };
    ThothCameraFrameDelegate* delegate { nil };
};

CameraCapture::CameraCapture(QString device_id)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->device_id = std::move(device_id);
}

CameraCapture::~CameraCapture()
{
    stop();
}

void CameraCapture::request_access(std::function<void(bool)> callback)
{
    auto complete = ^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{
            callback(granted);
        });
    };
    auto status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (status == AVAuthorizationStatusNotDetermined)
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:complete];
    else
        complete(status == AVAuthorizationStatusAuthorized);
}

QJsonArray CameraCapture::devices()
{
    QJsonArray devices;
    @autoreleasepool {
        auto preferred_id = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo].uniqueID;
        for (AVCaptureDevice* device in video_devices()) {
            QJsonArray formats;
            for (AVCaptureDeviceFormat* format in device.formats) {
                auto dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
                if (dimensions.width <= 0 || dimensions.height <= 0 || static_cast<uint64_t>(dimensions.width) * dimensions.height > 64 * 1024 * 1024)
                    continue;
                for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
                    formats.append(QJsonObject {
                        { QStringLiteral("width"), dimensions.width },
                        { QStringLiteral("height"), dimensions.height },
                        { QStringLiteral("minFrameRate"), range.minFrameRate },
                        { QStringLiteral("maxFrameRate"), range.maxFrameRate },
                    });
                }
            }
            QJsonObject description {
                { QStringLiteral("id"), QString::fromNSString(device.uniqueID) },
                { QStringLiteral("label"), QString::fromNSString(device.localizedName) },
                { QStringLiteral("formats"), formats },
            };
            if ([device.uniqueID isEqualToString:preferred_id])
                devices.prepend(description);
            else
                devices.append(description);
        }
    }
    return devices;
}

bool CameraCapture::start(QJsonObject const& configuration, QString& error)
{
    stop();
    error.clear();
    if ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo] != AVAuthorizationStatusAuthorized) {
        error = QStringLiteral("Camera access has not been granted in macOS Privacy & Security settings.");
        return false;
    }
    auto frames = std::make_shared<CameraFrameState>();
    auto* impl = m_impl.get();
    __block QString setup_error;
    dispatch_sync(impl->control_queue, ^{
        @autoreleasepool {
            AVCaptureDevice* device = nil;
            if (impl->device_id.isEmpty()) {
                device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
            } else {
                for (AVCaptureDevice* candidate in video_devices()) {
                    if (QString::fromNSString(candidate.uniqueID) == impl->device_id) {
                        device = candidate;
                        break;
                    }
                }
            }
            if (!device) {
                setup_error = QStringLiteral("The requested camera is unavailable.");
                return;
            }
            NSError* native_error = nil;
            AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&native_error];
            if (!input) {
                setup_error = native_error ? QString::fromNSString(native_error.localizedDescription) : QStringLiteral("The camera could not be opened.");
                return;
            }
            int native_width = configuration.value("nativeWidth").toInt();
            int native_height = configuration.value("nativeHeight").toInt();
            int width = configuration.value("width").toInt();
            int height = configuration.value("height").toInt();
            double frame_rate = configuration.value("frameRate").toDouble();
            AVCaptureDeviceFormat* selected_format = nil;
            AVFrameRateRange* selected_range = nil;
            // Revalidate renderer requests against real hardware. No arbitrary
            // dimensions or unsupported frame durations cross this boundary.
            if (width > 0 && height > 0 && width <= native_width && height <= native_height && std::isfinite(frame_rate) && frame_rate > 0) {
                for (AVCaptureDeviceFormat* format in device.formats) {
                    auto dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
                    if (dimensions.width != native_width || dimensions.height != native_height || static_cast<uint64_t>(native_width) * native_height > 64 * 1024 * 1024)
                        continue;
                    for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
                        if (frame_rate >= range.minFrameRate && frame_rate <= range.maxFrameRate) {
                            selected_format = format;
                            selected_range = range;
                            break;
                        }
                    }
                    if (selected_format)
                        break;
                }
            }
            if (!selected_format) {
                setup_error = QStringLiteral("The camera does not support the requested resolution or frame rate.");
                return;
            }
            AVCaptureSession* session = [[AVCaptureSession alloc] init];
            [session beginConfiguration];
            // macOS uses a held configuration lock to preserve an explicit
            // device format; the InputPriority preset is only available on iOS.
            AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
            output.videoSettings = @{(NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
            output.alwaysDiscardsLateVideoFrames = YES;
            if (![session canAddInput:input] || ![session canAddOutput:output]) {
                [session commitConfiguration];
                setup_error = QStringLiteral("This camera does not support video frame capture.");
                return;
            }
            [session addInput:input];
            [session addOutput:output];
            if (![device lockForConfiguration:&native_error]) {
                [session commitConfiguration];
                setup_error = QString::fromNSString(native_error.localizedDescription);
                return;
            }
            device.activeFormat = selected_format;
            auto duration = CMTimeMakeWithSeconds(1.0 / frame_rate, 1000000000);
            if (CMTimeCompare(duration, selected_range.minFrameDuration) < 0)
                duration = selected_range.minFrameDuration;
            if (CMTimeCompare(duration, selected_range.maxFrameDuration) > 0)
                duration = selected_range.maxFrameDuration;
            device.activeVideoMinFrameDuration = duration;
            device.activeVideoMaxFrameDuration = duration;
            impl->locked_device = device;
            frames->output_size = QSize(width, height);
            frames->native_size = QSize(native_width, native_height);
            ThothCameraFrameDelegate* delegate = [[ThothCameraFrameDelegate alloc] init];
            delegate->frame_state = frames;
            [output setSampleBufferDelegate:delegate queue:dispatch_queue_create("org.thoth.camera.frames", DISPATCH_QUEUE_SERIAL)];
            [session commitConfiguration];
            {
                std::lock_guard lock(impl->frames_mutex);
                impl->frames = frames;
            }
            impl->session = session;
            impl->output = output;
            impl->delegate = delegate;
            dispatch_async(impl->control_queue, ^{
                [session startRunning];
            });
        }
    });
    error = setup_error;
    return error.isEmpty();
}

QImage CameraCapture::latest_frame() const
{
    std::shared_ptr<CameraFrameState> frames;
    {
        std::lock_guard lock(m_impl->frames_mutex);
        frames = m_impl->frames;
    }
    if (!frames)
        return {};
    QImage image;
    {
        std::lock_guard lock(frames->mutex);
        // A retained last image is not evidence that an unplugged/interrupted device
        // is still delivering frames. Let the caller's capture timeout end the track.
        if (std::chrono::steady_clock::now() - frames->last_frame_at > std::chrono::seconds(1))
            return {};
        image = frames->image;
    }
    if (image.size() == frames->output_size)
        return image;
    // Center crop and downsample; never manufacture a higher native resolution.
    auto scaled = image.scaled(frames->output_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return scaled.copy((scaled.width() - frames->output_size.width()) / 2, (scaled.height() - frames->output_size.height()) / 2, frames->output_size.width(), frames->output_size.height());
}

void CameraCapture::stop()
{
    std::shared_ptr<CameraFrameState> frames;
    {
        std::lock_guard lock(m_impl->frames_mutex);
        frames = std::exchange(m_impl->frames, {});
    }
    if (frames) {
        frames->accepting_frames.store(false);
        std::lock_guard lock(frames->mutex);
        frames->image = {};
    }
    AVCaptureSession* session = m_impl->session;
    AVCaptureDevice* locked_device = m_impl->locked_device;
    m_impl->locked_device = nil;
    AVCaptureVideoDataOutput* output = m_impl->output;
    ThothCameraFrameDelegate* delegate = m_impl->delegate;
    m_impl->session = nil;
    m_impl->output = nil;
    m_impl->delegate = nil;
    if (!session)
        return;
    // The block retains the native objects until capture has shut down, without
    // retaining the CameraCapture object or blocking the UI on stopRunning.
    dispatch_async(m_impl->control_queue, ^{
        [session stopRunning];
        [locked_device unlockForConfiguration];
        [output setSampleBufferDelegate:nil queue:nullptr];
        (void)delegate;
    });
}

}
