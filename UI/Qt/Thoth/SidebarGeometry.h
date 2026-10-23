/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <QRect>
#include <QTreeWidget>

namespace Thoth {

inline QRect sidebar_card_rect(QRect const& row)
{
    return row.adjusted(-10, 1, -2, -1);
}

// Painting and pointer hit testing must use the same viewport coordinates,
// including the delegate's inset and the indented position of a tree row.
inline QRect sidebar_close_button_rect(QRect const& row)
{
    auto card = sidebar_card_rect(row);
    return { card.right() - 28, card.center().y() - 11, 22, 22 };
}

inline QRect sidebar_close_button_rect(QTreeWidget const& tree, QTreeWidgetItem const& item)
{
    // visualItemRect() includes the audio column, which the close glyph's
    // delegate never paints. Use the same column-zero rectangle as paint().
    return sidebar_close_button_rect(tree.visualRect(tree.indexFromItem(&item, 0)));
}

}
