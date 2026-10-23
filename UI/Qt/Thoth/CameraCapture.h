/*
 * Copyright (c) 2026, Thoth Browser contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QImage>
#include <QJsonArray>
#include <QString>
#include <functional>
#include <memory>

namespace Thoth {

// Owned by the browser process. Device IDs must only be exposed to an origin after
// its camera permission has been checked by the caller.
class CameraCapture {
public:
    // Always delivers the result asynchronously on the application's main queue.
    static void request_access(std::function<void(bool)>);
    // Does not prompt for permission or open the camera.
    static QJsonArray devices();

    explicit CameraCapture(QString device_id = {});
    ~CameraCapture();
    CameraCapture(CameraCapture const&) = delete;
    CameraCapture& operator=(CameraCapture const&) = delete;

    // Call start/stop from the owning UI thread. latest_frame is safe from any thread.
    // Requires OS permission already granted. Configures the session and schedules
    // capture on a private queue; hardware startup errors yield no frames.
    bool start(QString& error);
    // Null until first delivery, or once the latest frame is over one second old.
    QImage latest_frame() const;
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
