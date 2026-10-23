/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../PageSleepInput.h"
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
    using Thoth::input_may_change_page;
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier, QString(QChar(0x1b)));
    QKeyEvent workspace(QEvent::KeyPress, Qt::Key_1, Qt::ControlModifier | Qt::AltModifier, "1");
    QKeyEvent search(QEvent::KeyPress, Qt::Key_K, Qt::ControlModifier, "k");
    QKeyEvent text(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    QKeyEvent paste(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier, "v");
    QKeyEvent cut(QEvent::KeyPress, Qt::Key_X, Qt::MetaModifier, "x");
    QKeyEvent deletion(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    QKeyEvent arrow(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QEvent scroll(QEvent::Wheel), ime(QEvent::InputMethod), drop(QEvent::Drop), click(QEvent::MouseButtonPress);
    check(!input_may_change_page(escape) && !input_may_change_page(workspace) && !input_may_change_page(search), "browser navigation must not prevent later sleep");
    check(input_may_change_page(text) && input_may_change_page(paste) && input_may_change_page(cut) && input_may_change_page(deletion), "typing, clipboard edits and deletion protect page state");
    check(!input_may_change_page(arrow) && !input_may_change_page(scroll), "navigation and scrolling alone are safe");
    check(input_may_change_page(ime) && input_may_change_page(drop) && input_may_change_page(click), "composition, drops and page clicks are conservatively protected");
    qInfo() << "PASS: sleep input protection distinguishes edits from navigation shortcuts";
}
