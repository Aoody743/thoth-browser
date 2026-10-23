/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../SidebarGeometry.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

static void check(bool condition, char const* message)
{
    if (!condition) {
        qCritical() << "FAIL:" << message;
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    for (auto row : { QRect(0, 0, 280, 34), QRect(18, 102, 262, 34), QRect(36, -8, 180, 40), QRect(0, 340, 80, 34) }) {
        auto button = Thoth::sidebar_close_button_rect(row);
        auto card = Thoth::sidebar_card_rect(row);
        check(button.size() == QSize(22, 22), "close control retains its painted size");
        check(card.contains(button), "close control remains within its row card");
        check(button.contains(button.center()), "the glyph center must close its own row");
        check(!button.contains(QPoint(row.right(), row.center().y())), "blank space beyond the painted close button must not close a page");
        check(!button.contains(QPoint(button.center().x(), row.top())), "the old full-height close strip must not consume row-edge clicks");
        auto translation = QPoint(17, 91);
        check(Thoth::sidebar_close_button_rect(row.translated(translation)) == button.translated(translation), "scrolling and nesting must move painting and hit testing together");
    }
    qInfo() << "PASS: close glyph and pointer target share the same row geometry";
}
