/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <LibWeb/MediaCapture/MediaStreamTrack.h>
#include <cmath>
#include <limits>

namespace Web::MediaCapture {

// Camera dimensions are discrete native modes, with continuous downsampling.
// Keep required limits separate from ideals: an unavailable ideal is not an error.
struct CameraNumericConstraint {
    double minimum { 0 };
    double maximum { std::numeric_limits<double>::infinity() };
    Optional<double> ideal;

    template<typename Number, typename Range>
    static CameraNumericConstraint from(Optional<Variant<Number, Range>> const& value, bool required = false)
    {
        CameraNumericConstraint result;
        if (!value.has_value())
            return result;
        if (value->template has<Number>()) {
            if (required)
                result.minimum = result.maximum = value->template get<Number>();
            else
                result.ideal = value->template get<Number>();
            return result;
        }
        auto const& range = value->template get<Range>();
        if (range.min.has_value())
            result.minimum = *range.min;
        if (range.max.has_value())
            result.maximum = *range.max;
        if (range.exact.has_value()) {
            result.minimum = max(result.minimum, static_cast<double>(*range.exact));
            result.maximum = min(result.maximum, static_cast<double>(*range.exact));
        }
        if (range.ideal.has_value())
            result.ideal = *range.ideal;
        return result;
    }

    bool accepts(double value) const { return std::isfinite(value) && value > 0 && value + 0.000001 >= minimum && value - 0.000001 <= maximum; }
    double distance(double value) const
    {
        if (!ideal.has_value())
            return 0;
        return std::abs(value - *ideal) / max(std::abs(value), std::abs(*ideal));
    }
};

inline bool camera_string_matches(Variant<Utf16String, Vector<Utf16String>> const& values, Utf16String const& actual)
{
    return values.visit([&](Utf16String const& value) { return value == actual; }, [&](Vector<Utf16String> const& list) { return list.contains_slow(actual); });
}

inline Optional<double> camera_string_distance(Optional<Variant<Utf16String, Vector<Utf16String>, ConstrainDOMStringParameters>> const& constraint, Utf16String const& actual)
{
    if (!constraint.has_value())
        return 0;
    if (constraint->has<ConstrainDOMStringParameters>()) {
        auto const& parameters = constraint->get<ConstrainDOMStringParameters>();
        if (parameters.exact.has_value() && !camera_string_matches(*parameters.exact, actual))
            return {};
        return parameters.ideal.has_value() && !camera_string_matches(*parameters.ideal, actual) ? 1.0 : 0.0;
    }
    return constraint->visit([&](Utf16String const& value) { return value == actual ? 0.0 : 1.0; }, [&](Vector<Utf16String> const& values) { return values.contains_slow(actual) ? 0.0 : 1.0; }, [](ConstrainDOMStringParameters const&) { return 0.0; });
}

struct CameraSelection {
    String device_id;
    Utf16String label;
    JsonObject configuration;
    MediaTrackSettings settings;
    MediaTrackCapabilities capabilities;
    double distance { std::numeric_limits<double>::infinity() };
};

inline Optional<CameraSelection> select_camera_candidates(JsonArray const& devices, MediaTrackConstraintSet const& constraints, Vector<MediaTrackConstraintSet> const& advanced)
{
    auto width = CameraNumericConstraint::from(constraints.width);
    auto height = CameraNumericConstraint::from(constraints.height);
    auto aspect = CameraNumericConstraint::from(constraints.aspect_ratio);
    auto rate = CameraNumericConstraint::from(constraints.frame_rate);
    for (auto const& additional : advanced) {
        auto intersect = [](auto& base, auto const& extra) {
            auto required = CameraNumericConstraint::from(extra, true);
            base.minimum = max(base.minimum, required.minimum);
            base.maximum = min(base.maximum, required.maximum);
        };
        intersect(width, additional.width);
        intersect(height, additional.height);
        intersect(aspect, additional.aspect_ratio);
        intersect(rate, additional.frame_rate);
    }
    auto advanced_string_matches = [&](auto property, Utf16String const& actual) {
        for (auto const& additional : advanced) {
            auto const& constraint = additional.*property;
            if (!constraint.has_value())
                continue;
            if (constraint->template has<ConstrainDOMStringParameters>()) {
                if (!camera_string_distance(constraint, actual).has_value())
                    return false;
            } else if (camera_string_distance(constraint, actual).value_or(1) != 0) {
                return false;
            }
        }
        return true;
    };
    Optional<CameraSelection> best;
    for (auto const& value : devices.values()) {
        if (!value.is_object())
            continue;
        auto const& device = value.as_object();
        auto id = device.get_string("id"sv);
        auto label = device.get_string("label"sv);
        auto formats = device.get_array("formats"sv);
        if (!id.has_value() || !label.has_value() || !formats.has_value())
            continue;
        auto device_distance = camera_string_distance(constraints.device_id, Utf16String::from_utf8(*id));
        if (!device_distance.has_value() || !advanced_string_matches(&MediaTrackConstraintSet::device_id, Utf16String::from_utf8(*id)))
            continue;
        MediaTrackCapabilities capabilities;
        capabilities.device_id = Utf16String::from_utf8(*id);
        u32 max_width = 1, max_height = 1;
        double max_rate = 0;
        for (auto const& item : formats->values()) {
            if (!item.is_object())
                continue;
            auto const& format = item.as_object();
            auto native_width = format.get_u32("width"sv).value_or(0);
            auto native_height = format.get_u32("height"sv).value_or(0);
            auto maximum_rate = format.get_double_with_precision_loss("maxFrameRate"sv).value_or(0);
            max_width = max(max_width, native_width);
            max_height = max(max_height, native_height);
            max_rate = max(max_rate, maximum_rate);
        }
        capabilities.width = ULongRange { .max = max_width, .min = 1 };
        capabilities.height = ULongRange { .max = max_height, .min = 1 };
        capabilities.aspect_ratio = DoubleRange { .max = static_cast<double>(max_width), .min = 1.0 / max_height };
        capabilities.frame_rate = DoubleRange { .max = max_rate, .min = 0 };
        capabilities.resize_mode = { "none"_utf16, "crop-and-scale"_utf16 };
        for (auto const& item : formats->values()) {
            if (!item.is_object())
                continue;
            auto const& format = item.as_object();
            auto nw = format.get_u32("width"sv).value_or(0);
            auto nh = format.get_u32("height"sv).value_or(0);
            auto min_rate = format.get_double_with_precision_loss("minFrameRate"sv).value_or(0);
            auto max_rate = format.get_double_with_precision_loss("maxFrameRate"sv).value_or(0);
            if (!nw || !nh || max_rate <= 0)
                continue;
            // Consider native output and crop/scale output separately. Prefer
            // native delivery on equal fitness, to avoid unnecessary conversion.
            for (bool scale : { false, true }) {
                auto resize_mode = scale ? "crop-and-scale"_utf16 : "none"_utf16;
                auto resize_distance = camera_string_distance(constraints.resize_mode, resize_mode);
                if (!resize_distance.has_value() || !advanced_string_matches(&MediaTrackConstraintSet::resize_mode, resize_mode))
                    continue;
                double w = nw, h = nh;
                if (scale) {
                    auto lower_w = max(1.0, std::ceil(width.minimum));
                    auto upper_w = min(static_cast<double>(nw), std::floor(width.maximum));
                    auto lower_h = max(1.0, std::ceil(height.minimum));
                    auto upper_h = min(static_cast<double>(nh), std::floor(height.maximum));
                    if (lower_w > upper_w || lower_h > upper_h)
                        continue;
                    w = clamp(width.ideal.value_or(min(1920.0, static_cast<double>(nw))), lower_w, upper_w);
                    h = clamp(height.ideal.value_or(w * nh / nw), lower_h, upper_h);
                    auto ratio_min = max(aspect.minimum, 1.0 / nh);
                    auto ratio_max = min(aspect.maximum, static_cast<double>(nw));
                    if (ratio_min > ratio_max)
                        continue;
                    auto ratio = clamp(aspect.ideal.value_or(w / h), ratio_min, ratio_max);
                    if (!std::isfinite(ratio) || ratio <= 0)
                        continue;
                    // Intersect width limits with the height limits at this ratio.
                    auto ratio_lower = max(lower_w, lower_h * ratio);
                    auto ratio_upper = min(upper_w, upper_h * ratio);
                    if (ratio_lower > ratio_upper + 0.000001)
                        continue;
                    w = std::round(clamp(w, ratio_lower, ratio_upper));
                    h = std::round(w / ratio);
                }
                auto fps_lower = max(0.01, rate.minimum);
                auto fps_upper = min(max_rate, rate.maximum);
                if (fps_lower > fps_upper)
                    continue;
                auto fps = clamp(rate.ideal.value_or(30), fps_lower, fps_upper);
                if (!width.accepts(w) || !height.accepts(h) || !aspect.accepts(w / h) || !rate.accepts(fps))
                    continue;
                auto distance = *device_distance + *resize_distance + width.distance(w) + height.distance(h) + aspect.distance(w / h) + rate.distance(fps);
                // Defaults favour 1080p30, while explicit ideals take precedence.
                auto preference = std::abs(w - 1920) / max(w, 1920.0) + std::abs(h - 1080) / max(h, 1080.0);
                distance += preference * 0.00001 + (scale ? 0.000001 : 0);
                if (best.has_value() && best->distance <= distance)
                    continue;
                CameraSelection selection;
                selection.device_id = *id;
                selection.label = Utf16String::from_utf8(*label);
                selection.distance = distance;
                selection.capabilities = capabilities;
                selection.settings.device_id = Utf16String::from_utf8(*id);
                selection.settings.width = static_cast<u32>(w);
                selection.settings.height = static_cast<u32>(h);
                selection.settings.aspect_ratio = w / h;
                selection.settings.frame_rate = fps;
                selection.settings.resize_mode = resize_mode;
                selection.configuration.set("nativeWidth"sv, nw);
                selection.configuration.set("nativeHeight"sv, nh);
                selection.configuration.set("width"sv, static_cast<u32>(w));
                selection.configuration.set("height"sv, static_cast<u32>(h));
                selection.configuration.set("frameRate"sv, max(fps, min_rate));
                best = move(selection);
            }
        }
    }
    return best;
}

inline Optional<CameraSelection> select_camera(JsonArray const& devices, MediaTrackConstraints const& constraints)
{
    Vector<MediaTrackConstraintSet> accepted;
    auto selected = select_camera_candidates(devices, constraints, accepted);
    if (!selected.has_value())
        return {};
    if (!constraints.advanced.has_value())
        return selected;
    for (auto const& additional : *constraints.advanced) {
        accepted.append(additional);
        auto candidate = select_camera_candidates(devices, constraints, accepted);
        if (candidate.has_value())
            selected = move(candidate);
        else
            accepted.take_last();
    }
    return selected;
}

}
