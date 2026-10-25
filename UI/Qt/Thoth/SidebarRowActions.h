/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#pragma once

#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QTreeWidget>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

namespace Thoth {

// Use one real button for the glyph, pointer target and accessibility action.
// A separate item delegate must never paint a second close control.
class SidebarRowActions final : public QWidget {
public:
    SidebarRowActions(QTreeWidget* tree, QTreeWidgetItem* row, QIcon const& close_icon, QString const& close_label)
        : QWidget(tree)
        , m_tree(tree)
        , m_index(tree->indexFromItem(row, 0))
    {
        setAutoFillBackground(false);
        // The widget overlays the whole row. Clicks on the row surface are forwarded to the
        // tree viewport (so selection, double-click and drags keep working), while the child
        // buttons receive their own events as usual.
        m_layout = new QHBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 2, 0);
        m_layout->setSpacing(2);
        m_layout->addStretch();
        m_close = new QToolButton(this);
        m_close->setObjectName("ThothSidebarClose");
        m_close->setFixedSize(22, 22);
        m_close->setIconSize({ 12, 12 });
        m_close->setIcon(close_icon);
        m_close->setToolTip(close_label);
        m_close->setAccessibleName(close_label);
        m_close->setAutoRaise(true);
        m_close->setFocusPolicy(Qt::StrongFocus);
        m_close->setStyleSheet("QToolButton { padding: 0; border: none; border-radius: 6px; }"
                              "QToolButton:hover, QToolButton:focus { background: palette(midlight); }"
                              "QToolButton:pressed { background: palette(mid); }");
        auto policy = m_close->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        m_close->setSizePolicy(policy);
        m_layout->addWidget(m_close);
        m_close->hide();
        tree->viewport()->setMouseTracking(true);
        tree->viewport()->installEventFilter(this);
        tree->installEventFilter(this);
        installEventFilter(this);
        m_close->installEventFilter(this);
        connect(tree, &QTreeWidget::currentItemChanged, this, [this] { update_visibility(); });
        connect(qApp, &QApplication::focusChanged, this, [this] { update_visibility(); });
        connect(tree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { refresh_pointer(); });
        // A title/loading refresh may replace this widget under a stationary pointer.
        QTimer::singleShot(0, this, [this] { refresh_pointer(); });
    }

    QToolButton* close_button() const { return m_close; }
    void add_audio_button(QToolButton* button)
    {
        button->setFixedSize(22, 22);
        button->setIconSize({ 14, 14 });
        button->setStyleSheet("QToolButton { padding: 0; }");
        m_layout->insertWidget(m_layout->count() - 1, button);
        button->installEventFilter(this);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override { forward_to_viewport(event); }
    void mouseReleaseEvent(QMouseEvent* event) override { forward_to_viewport(event); }
    void mouseDoubleClickEvent(QMouseEvent* event) override { forward_to_viewport(event); }
    void mouseMoveEvent(QMouseEvent* event) override { forward_to_viewport(event); }
    void contextMenuEvent(QContextMenuEvent* event) override
    {
        // Row context menu lives in the tree; re-dispatch at the same global position.
        auto global = event->globalPos();
        QContextMenuEvent forwarded(event->reason(), m_tree->viewport()->mapFromGlobal(global), global);
        QApplication::sendEvent(m_tree->viewport(), &forwarded);
    }

    void forward_to_viewport(QMouseEvent* event)
    {
        if (!m_tree)
            return;
        auto target = m_tree->viewport()->mapFromGlobal(event->globalPosition().toPoint());
        QMouseEvent forwarded(event->type(), target, event->globalPosition(), event->button(),
            event->buttons(), event->modifiers());
        QApplication::sendEvent(m_tree->viewport(), &forwarded);
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (!m_tree || !m_index.isValid())
            return QWidget::eventFilter(object, event);
        if (event->type() == QEvent::Resize) {
            QTimer::singleShot(0, this, [this] { refresh_pointer(); });
        } else if (event->type() == QEvent::MouseMove) {
            update_pointer(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
        } else if (event->type() == QEvent::Enter) {
            update_pointer(static_cast<QEnterEvent*>(event)->globalPosition().toPoint());
        } else if (event->type() == QEvent::Leave) {
            update_pointer(QCursor::pos());
        } else if (object == m_tree && event->type() == QEvent::FocusIn) {
            auto reason = static_cast<QFocusEvent*>(event)->reason();
            m_keyboard_focus = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason;
        } else if (object == m_tree && event->type() == QEvent::KeyPress) {
            m_keyboard_focus = true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            m_keyboard_focus = false;
        }
        update_visibility();
        return QWidget::eventFilter(object, event);
    }

private:
    void refresh_pointer()
    {
        if (!m_tree || !m_index.isValid())
            return;
        update_pointer(QCursor::pos());
        update_visibility();
    }
    void update_pointer(QPoint global)
    {
        auto index = m_tree->indexAt(m_tree->viewport()->mapFromGlobal(global));
        m_pointer_over_row = index.isValid() && index.siblingAtColumn(0) == m_index;
    }
    void update_visibility()
    {
        if (!m_tree || !m_index.isValid())
            return;
        bool focused_row = m_keyboard_focus && m_tree->hasFocus()
            && m_tree->currentIndex().siblingAtColumn(0) == m_index;
        m_close->setVisible(m_pointer_over_row || focused_row || m_close->hasFocus());
    }
    QPointer<QTreeWidget> m_tree;
    QPersistentModelIndex m_index;
    bool m_pointer_over_row { false };
    bool m_keyboard_focus { false };
    QHBoxLayout* m_layout {};
    QToolButton* m_close {};
};

}
