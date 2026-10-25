/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// Shared between the WASAPI playback, capture and device-watch backends.

#include <AK/Error.h>
#include <AK/Vector.h>
#include <LibAudio/ChannelMap.h>

#include <AK/Windows.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

namespace Audio {

#define ENUMERATE_CHANNEL_POSITIONS(C)                            \
    C(SPEAKER_FRONT_LEFT, Channel::FrontLeft)                     \
    C(SPEAKER_FRONT_RIGHT, Channel::FrontRight)                   \
    C(SPEAKER_FRONT_CENTER, Channel::FrontCenter)                 \
    C(SPEAKER_LOW_FREQUENCY, Channel::LowFrequency)               \
    C(SPEAKER_BACK_LEFT, Channel::BackLeft)                       \
    C(SPEAKER_BACK_RIGHT, Channel::BackRight)                     \
    C(SPEAKER_FRONT_LEFT_OF_CENTER, Channel::FrontLeftOfCenter)   \
    C(SPEAKER_FRONT_RIGHT_OF_CENTER, Channel::FrontRightOfCenter) \
    C(SPEAKER_BACK_CENTER, Channel::BackCenter)                   \
    C(SPEAKER_SIDE_LEFT, Channel::SideLeft)                       \
    C(SPEAKER_SIDE_RIGHT, Channel::SideRight)                     \
    C(SPEAKER_TOP_CENTER, Channel::TopCenter)                     \
    C(SPEAKER_TOP_FRONT_LEFT, Channel::TopFrontLeft)              \
    C(SPEAKER_TOP_FRONT_CENTER, Channel::TopFrontCenter)          \
    C(SPEAKER_TOP_FRONT_RIGHT, Channel::TopFrontRight)            \
    C(SPEAKER_TOP_BACK_LEFT, Channel::TopBackLeft)                \
    C(SPEAKER_TOP_BACK_CENTER, Channel::TopBackCenter)            \
    C(SPEAKER_TOP_BACK_RIGHT, Channel::TopBackRight)




inline ErrorOr<ChannelMap> convert_bitmask_to_channel_map(u32 channel_bitmask)
{
    Vector<Channel, ChannelMap::capacity()> channels;

#define MAYBE_ADD_CHANNEL_FROM_BITMAP_FLAG(ksmedia_channel_name, audio_channel)                   if ((channel_bitmask & ksmedia_channel_name) != 0) {                                              if (channels.size() == ChannelMap::capacity()) [[unlikely]]                                       return Error::from_string_literal("Device channel layout had too many channels");         channels.unchecked_append(audio_channel);                                                 }

    ENUMERATE_CHANNEL_POSITIONS(MAYBE_ADD_CHANNEL_FROM_BITMAP_FLAG);

    if ((channel_bitmask & SPEAKER_RESERVED) != 0) [[unlikely]]
        return Error::from_string_literal("Unsupported new KSMEDIA version");

    return ChannelMap { channels };
}

}
