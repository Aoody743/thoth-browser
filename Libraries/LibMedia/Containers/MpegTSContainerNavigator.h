/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibMedia/Containers/ContainerNavigator.h>
#include <LibMedia/TimeRanges.h>

namespace Media {

// MPEG-TS carries no index and no container-level sample table, so there is nothing to map byte positions
// onto timestamps with except the stream's own rate. Transport streams are written at a constant rate by
// design, which makes that mapping good enough for buffered-range reporting and for deciding when enough
// data has arrived to start playback.
//
// Every track in a transport stream is interleaved across the same byte range, so all of them report the
// same buffered ranges.
class MpegTSContainerNavigator final : public ContainerNavigator {
public:
    MpegTSContainerNavigator(Vector<u64> track_identifiers, size_t first_sample_position, u32 bytes_per_second, AK::Duration start_time, AK::Duration duration)
        : m_track_identifiers(move(track_identifiers))
        , m_first_sample_position(first_sample_position)
        , m_bytes_per_second(bytes_per_second)
        , m_start_time(start_time)
        , m_duration(duration)
    {
    }

    virtual Vector<BufferedRangesScan> buffered_time_ranges_by_track(Vector<MediaStream::ByteRange> const& byte_ranges) const override
    {
        Vector<BufferedRangesScan> scans;
        if (byte_ranges.is_empty())
            return scans;

        // The same ranges apply to every track, since they are interleaved across the whole stream.
        BufferedRangesScan scan;
        for (auto const& byte_range : byte_ranges) {
            if (byte_range.end <= m_first_sample_position)
                continue;

            auto data_start = (byte_range.start > m_first_sample_position) ? byte_range.start - m_first_sample_position : 0;
            auto data_end = byte_range.end - m_first_sample_position;

            auto time_start = m_start_time + AK::Duration::from_time_units(AK::clamp_to<i64>(data_start), 1, m_bytes_per_second);
            auto time_end = m_start_time + AK::Duration::from_time_units(AK::clamp_to<i64>(data_end), 1, m_bytes_per_second);
            scan.time_ranges.add_range(max(AK::Duration::zero(), time_start), time_end);
        }
        scan.last_byte_range_has_samples = byte_ranges.last().end > m_first_sample_position;

        for (auto track_identifier : m_track_identifiers) {
            auto track_scan = scan;
            track_scan.track_identifier = track_identifier;
            scans.append(move(track_scan));
        }
        return scans;
    }

    // Seeking is left to the demuxer, which seeks through FFmpeg and so needs no help here.
    virtual DecoderErrorOr<SeekResult> seek_to_timestamp(u64, AK::Duration) const override
    {
        return Empty {};
    }

    AK::Duration duration() const { return m_duration; }

private:
    Vector<u64> m_track_identifiers;
    size_t m_first_sample_position { 0 };
    u32 m_bytes_per_second { 0 };
    AK::Duration m_start_time;
    AK::Duration m_duration;
};

}
