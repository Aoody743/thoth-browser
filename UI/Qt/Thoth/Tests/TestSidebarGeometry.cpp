/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "../SidebarRowActions.h"
#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QTreeWidget>
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
    QApplication app(argc, argv);
    QPixmap glyph(12, 12);
    glyph.fill(Qt::transparent);
    {
        QPainter painter(&glyph);
        painter.setPen(QPen(Qt::black, 2));
        painter.drawLine(3, 3, 9, 9);
        painter.drawLine(3, 9, 9, 3);
    }
    QTreeWidget tree;
    tree.setColumnCount(2);
    tree.setHeaderHidden(true);
    tree.header()->setStretchLastSection(false);
    tree.header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree.header()->setSectionResizeMode(1, QHeaderView::Fixed);
    auto* item = new QTreeWidgetItem(&tree, { "Page to close" });
    item->setSizeHint(0, { 0, 40 });
    auto* actions = new Thoth::SidebarRowActions(&tree, QIcon(glyph), "Close tab");
    tree.setItemWidget(item, 1, actions);
    int closed = 0;
    QObject::connect(actions->close_button(), &QToolButton::clicked, [&] { ++closed; });
    tree.show();
    for (bool audio : { false, true }) {
        if (audio)
            actions->add_audio_button(new QToolButton(actions));
        tree.setColumnWidth(1, audio ? 56 : 30);
        for (int width : { 280, 360, 190 }) {
            tree.resize(width, 240);
            app.processEvents();
            auto* button = actions->close_button();
            check(button->isVisible(), "close control must be visible without hover");
            check(button->accessibleName() == "Close tab", "close control must expose an accessible action");
            auto icon = button->icon().pixmap(12, 12).toImage();
            check(!icon.isNull() && qAlpha(icon.pixel(6, 6)) > 0, "close glyph must contain visible pixels");
            auto rect = QRect(button->mapTo(tree.viewport(), QPoint()), button->size());
            check(tree.viewport()->rect().contains(rect), "close button remains inside viewport after resize");
            auto column = tree.visualRect(tree.indexFromItem(item, 1));
            check(column.contains(rect.center()), "glyph and button stay in the action column");
            auto center = button->rect().center();
            auto global = button->mapToGlobal(center);
            QMouseEvent press(QEvent::MouseButtonPress, center, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, center, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            int before = closed;
            QApplication::sendEvent(button, &press);
            QApplication::sendEvent(button, &release);
            check(closed == before + 1, "clicking the real glyph center must emit exactly one close action");
        }
    }
    qInfo() << "PASS: visible native close button and center click at all widths, with and without audio";
}
