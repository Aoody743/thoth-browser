/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <QRect>

namespace Thoth {

inline QRect sidebar_card_rect(QRect const& row)
{
    return row.adjusted(-10, 1, -2, -1);
}

}
