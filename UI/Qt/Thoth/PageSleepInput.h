/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once
#include <QEvent>
#include <QKeyEvent>

namespace Thoth {
inline bool input_may_change_page(QEvent const& event)
{
    if (event.type() == QEvent::KeyPress) {
        auto const& key = static_cast<QKeyEvent const&>(event);
        auto command = key.modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier);
        auto text = key.text();
        auto printable = !text.isEmpty() && text.front().isPrint();
        return (!command && (printable || key.key() == Qt::Key_Backspace || key.key() == Qt::Key_Delete || key.key() == Qt::Key_Return || key.key() == Qt::Key_Enter))
            || (command && (key.key() == Qt::Key_V || key.key() == Qt::Key_X));
    }
    return event.type() == QEvent::InputMethod || event.type() == QEvent::MouseButtonPress || event.type() == QEvent::Drop;
}
}
