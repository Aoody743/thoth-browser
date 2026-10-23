/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <LibTest/TestCase.h>
#include <LibWeb/MediaCapture/CameraConstraints.h>

using namespace Web::MediaCapture;

static JsonArray cameras()
{
    return MUST(JsonValue::from_string(R"([{"id":"camera","label":"Test camera","formats":[{"width":640,"height":480,"minFrameRate":15,"maxFrameRate":30},{"width":1280,"height":720,"minFrameRate":15,"maxFrameRate":60},{"width":1920,"height":1080,"minFrameRate":15,"maxFrameRate":30}]}])"sv)).as_array();
}

static MediaTrackConstraints exact_size(u32 width, u32 height)
{
    MediaTrackConstraints constraints;
    constraints.width = ConstrainULongRange { .exact = width };
    constraints.height = ConstrainULongRange { .exact = height };
    return constraints;
}

TEST_CASE(default_uses_1080p_and_reports_native_capabilities)
{
    auto selected = select_camera(cameras(), {});
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->settings.width, 1920u);
    EXPECT_EQ(*selected->settings.height, 1080u);
    EXPECT_EQ(*selected->settings.frame_rate, 30);
    EXPECT_EQ(*selected->capabilities.width->max, 1920u);
    EXPECT_EQ(*selected->capabilities.frame_rate->max, 60);
}

TEST_CASE(exact_1080p_selects_real_native_mode)
{
    auto selected = select_camera(cameras(), exact_size(1920, 1080));
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->configuration.get_u32("nativeWidth"sv), 1920u);
    EXPECT_EQ(*selected->configuration.get_u32("width"sv), 1920u);
}

TEST_CASE(unavailable_exact_resolution_rejects_but_unavailable_ideal_falls_back)
{
    EXPECT(!select_camera(cameras(), exact_size(3840, 2160)).has_value());
    MediaTrackConstraints constraints;
    constraints.width = 3840u;
    constraints.height = 2160u;
    auto selected = select_camera(cameras(), constraints);
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->settings.width, 1920u);
    EXPECT_EQ(*selected->settings.height, 1080u);
}

TEST_CASE(arbitrary_lower_dimensions_are_downsampled)
{
    auto selected = select_camera(cameras(), exact_size(853, 479));
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->settings.width, 853u);
    EXPECT_EQ(*selected->settings.height, 479u);
    EXPECT_EQ(*selected->settings.resize_mode, "crop-and-scale"_utf16);
}

TEST_CASE(frame_rate_selects_720p60_instead_of_unsupported_1080p60)
{
    auto constraints = exact_size(1280, 720);
    constraints.frame_rate = ConstrainDoubleRange { .exact = 60 };
    auto selected = select_camera(cameras(), constraints);
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->configuration.get_u32("nativeWidth"sv), 1280u);
    constraints = exact_size(1920, 1080);
    constraints.frame_rate = ConstrainDoubleRange { .exact = 60 };
    EXPECT(!select_camera(cameras(), constraints).has_value());
}

TEST_CASE(min_max_conflicts_and_zero_reject)
{
    MediaTrackConstraints constraints;
    ConstrainULongRange width_range;
    width_range.max = 640u;
    width_range.min = 1920u;
    constraints.width = width_range;
    EXPECT(!select_camera(cameras(), constraints).has_value());
    EXPECT(!select_camera(cameras(), exact_size(0, 1080)).has_value());
    constraints = {};
    ConstrainDoubleRange aspect_range;
    aspect_range.max = 1;
    aspect_range.min = 2;
    constraints.aspect_ratio = aspect_range;
    EXPECT(!select_camera(cameras(), constraints).has_value());
}

TEST_CASE(device_ideal_is_optional_but_exact_is_required)
{
    MediaTrackConstraints constraints;
    constraints.device_id = "missing"_utf16;
    EXPECT(select_camera(cameras(), constraints).has_value());
    ConstrainDOMStringParameters required;
    required.exact = "missing"_utf16;
    constraints.device_id = required;
    EXPECT(!select_camera(cameras(), constraints).has_value());
}

TEST_CASE(no_resize_requires_a_native_resolution)
{
    auto constraints = exact_size(853, 479);
    ConstrainDOMStringParameters required;
    required.exact = "none"_utf16;
    constraints.resize_mode = required;
    EXPECT(!select_camera(cameras(), constraints).has_value());
    constraints = exact_size(640, 480);
    constraints.resize_mode = required;
    EXPECT(select_camera(cameras(), constraints).has_value());
}

TEST_CASE(aspect_ratio_crops_to_square)
{
    MediaTrackConstraints constraints;
    constraints.width = ConstrainULongRange { .exact = 1000u };
    constraints.aspect_ratio = ConstrainDoubleRange { .exact = 1 };
    auto selected = select_camera(cameras(), constraints);
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->settings.height, 1000u);
}

TEST_CASE(advanced_sets_are_applied_in_order_and_unsatisfiable_sets_are_skipped)
{
    MediaTrackConstraints constraints;
    constraints.advanced = Vector<MediaTrackConstraintSet> {};
    constraints.advanced->append(exact_size(3840, 2160));
    constraints.advanced->append(exact_size(1280, 720));
    auto selected = select_camera(cameras(), constraints);
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->settings.width, 1280u);
    EXPECT_EQ(*selected->settings.height, 720u);
}

TEST_CASE(native_8k_is_not_limited_to_4096_pixels)
{
    auto devices = MUST(JsonValue::from_string(R"([{"id":"8k","label":"8K camera","formats":[{"width":7680,"height":4320,"minFrameRate":15,"maxFrameRate":30}]}])"sv)).as_array();
    auto selected = select_camera(devices, exact_size(7680, 4320));
    EXPECT(selected.has_value());
    EXPECT_EQ(*selected->settings.width, 7680u);
    EXPECT_EQ(*selected->settings.height, 4320u);
}
