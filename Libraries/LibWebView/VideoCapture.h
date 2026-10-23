/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once
#include <AK/RefCounted.h>
#include <LibGfx/ShareableBitmap.h>
namespace WebView {
// Owned by the browser page, never by untrusted renderer code.
class VideoCapture : public RefCounted<VideoCapture> {
public:
    virtual ~VideoCapture() = default;
    virtual Gfx::ShareableBitmap frame() = 0;
};
}
