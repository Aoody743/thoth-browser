/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <LibAudio/RecordStream.h>

namespace Audio {

class RecordStreamWASAPI final : public RecordStream {
public:
    struct CaptureState;

    static NonnullRefPtr<CreatePromise> create(SampleSpecification const& requested_specification, u32 fragment_size_bytes, StringView device_id, RecordCallback&&);

    virtual SampleSpecification const& sample_specification() const override { return m_sample_specification; }

private:
    explicit RecordStreamWASAPI(NonnullRefPtr<CaptureState>, SampleSpecification);
    ~RecordStreamWASAPI();

    NonnullRefPtr<CaptureState> m_state;
    SampleSpecification m_sample_specification;
};

}
