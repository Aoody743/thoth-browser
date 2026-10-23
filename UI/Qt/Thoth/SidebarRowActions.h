/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#pragma once

#include <QHBoxLayout>
#include <QToolButton>
#include <QWidget>

namespace Thoth {

// Use one real button for the glyph, pointer target and accessibility action.
// A separate item delegate must never paint a second close control.
class SidebarRowActions final : public QWidget {
public:
    SidebarRowActions(QWidget* parent, QIcon const& close_icon, QString const& close_label)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
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
        m_layout->addWidget(m_close);
    }

    QToolButton* close_button() const { return m_close; }
    void add_audio_button(QToolButton* button)
    {
        button->setFixedSize(22, 22);
        button->setIconSize({ 14, 14 });
        button->setStyleSheet("QToolButton { padding: 0; }");
        m_layout->insertWidget(m_layout->count() - 1, button);
    }

private:
    QHBoxLayout* m_layout {};
    QToolButton* m_close {};
};

}
