/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../SidebarGeometry.h"
#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <cstdlib>

static void check(bool condition, char const* message)
{
    if (!condition) {
        qCritical() << "FAIL:" << message;
        std::exit(1);
    }
}

class RecordingDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    mutable QMap<QModelIndex, QRect> painted_rects;
    void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override
    {
        painted_rects[index] = option.rect;
        QStyledItemDelegate::paint(painter, option, index);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
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
    // Reproduce the real two-column sidebar: visualItemRect spans the audio
    // column, while a delegate's option.rect covers only the painted column.
    QTreeWidget tree;
    tree.setStyleSheet("QTreeWidget { background: transparent; border: none; outline: none; }"
                       "QTreeView::branch { image: none; border: none; }"
                       "QTreeWidget::item { border: 1px solid transparent; border-radius: 9px; padding: 4px 5px; }");
    tree.resize(280, 240);
    tree.setColumnCount(2);
    tree.setHeaderHidden(true);
    tree.setRootIsDecorated(false);
    tree.setIndentation(14);
    tree.header()->setStretchLastSection(false);
    tree.header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree.header()->setSectionResizeMode(1, QHeaderView::Fixed);
    tree.setColumnWidth(1, 30);
    auto* delegate = new RecordingDelegate(&tree);
    tree.setItemDelegate(delegate);
    auto* item = new QTreeWidgetItem(&tree);
    item->setText(0, "Page to close");
    item->setSizeHint(0, { 0, 40 });
    tree.show();
    for (int width : { 280, 360, 180 }) {
        tree.resize(width, 240);
        app.processEvents();
        auto rendered = tree.grab();
        (void)rendered;
        auto index = tree.indexFromItem(item, 0);
        auto painted_row = delegate->painted_rects.value(index);
        auto pointer_row = tree.visualRect(index);
        auto whole_row = tree.visualItemRect(item);
        check(!painted_row.isEmpty(), "actual sidebar delegate must paint the test row");
        check(painted_row == pointer_row, "pointer coordinates must match the actual column-zero paint rectangle");
        check(whole_row.width() - painted_row.width() == 30, "the fixture must reproduce the 30px audio-column mismatch");
        auto drawn_button = Thoth::sidebar_close_button_rect(painted_row);
        auto pointer_button = Thoth::sidebar_close_button_rect(tree, *item);
        check(drawn_button == pointer_button, "painted close button must exactly match its pointer region after resizing");
        check(!pointer_button.contains(Thoth::sidebar_close_button_rect(whole_row).center()), "the former displaced target must not close a page");
    }
    qInfo() << "PASS: actual two-column tree painting and close target align at all tested widths";
}
