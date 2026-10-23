/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "../SidebarRowActions.h"
#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QMouseEvent>
#include <QFileInfo>
#include <QDir>
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
    QIcon glyph(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("../Icons/lucide/x-regular.png"));
    check(!glyph.isNull(), "bundled Lucide close icon must load");
    QTreeWidget tree;
    tree.setColumnCount(2);
    tree.setHeaderHidden(true);
    tree.header()->setStretchLastSection(false);
    tree.header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree.header()->setSectionResizeMode(1, QHeaderView::Fixed);
    auto* item = new QTreeWidgetItem(&tree, { "Page to close" });
    item->setSizeHint(0, { 0, 40 });
    auto* actions = new Thoth::SidebarRowActions(&tree, item, glyph, "Close tab");
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
            tree.clearFocus();
            button->clearFocus();
            auto outside = tree.viewport()->rect().bottomLeft();
            QMouseEvent leave_row(QEvent::MouseMove, outside, tree.viewport()->mapToGlobal(outside), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(tree.viewport(), &leave_row);
            check(!button->isVisible(), "close control must stay hidden outside the row");
            auto row_center = tree.visualItemRect(item).center();
            QMouseEvent hover_row(QEvent::MouseMove, row_center, tree.viewport()->mapToGlobal(row_center), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(tree.viewport(), &hover_row);
            app.processEvents();
            check(button->isVisible(), "hovering the page row must reveal the real close button");
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
            QApplication::sendEvent(tree.viewport(), &leave_row);
            tree.clearFocus();
            tree.setCurrentItem(item);
            tree.setFocus(Qt::TabFocusReason);
            app.processEvents();
            check(button->isVisible(), "keyboard focus must expose the selected row close action");
            tree.clearFocus();
            QApplication::sendEvent(tree.viewport(), &leave_row);
            check(!button->isVisible(), "close action must hide again after hover and focus leave");
        }
    }
    qInfo() << "PASS: hover-revealed Lucide close button and center click at all widths, with and without audio";
}
