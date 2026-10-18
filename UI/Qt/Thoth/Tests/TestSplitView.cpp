/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../SplitView.h"
#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    auto* stack = new QStackedWidget;
    auto* home = new QWidget;
    auto* home_layout = new QVBoxLayout(home);
    auto* right = new QLabel("Page B");
    home_layout->addWidget(right);
    auto* left = new QLabel("Page A");
    stack->addWidget(left);
    stack->addWidget(home);
    Thoth::SplitView split(stack);
    split.resize(1000, 700);
    split.show();
    split.open(right, home);
    app.processEvents();
    if (split.count() != 2 || !left->isVisible() || !right->isVisible()
        || right->width() <= 0 || left->width() <= 0 || stack->count() != 2)
        return 1;
    split.setSizes({ 300, 700 });
    app.processEvents();
    if (right->width() <= left->width())
        return 2;
    split.clear_secondary();
    if (right->parentWidget() != home || split.secondary() || split.count() != 1)
        return 3;
    stack->setCurrentWidget(home);
    app.processEvents();
    if (!right->isVisible())
        return 4;
    stack->setCurrentWidget(left);
    for (int i = 0; i < 10; ++i) {
        split.open(right, home);
        split.clear_secondary();
    }
    if (home_layout->count() != 1 || right->parentWidget() != home)
        return 5;
    qInfo() << "PASS: two panes, resize, restore, repeated toggles, stable stack slots";
    return 0;
}
