/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SplitView.h"
#include <QFrame>
#include <QLayout>
#include <QPaintEvent>

namespace Thoth {

class InvisibleSplitHandle final : public QSplitterHandle {
public:
    using QSplitterHandle::QSplitterHandle;

protected:
    void paintEvent(QPaintEvent*) override { }
};

SplitView::SplitView(QWidget* primary, QWidget* parent)
    : QSplitter(Qt::Horizontal, parent)
{
    setFrameShape(QFrame::NoFrame);
    setContentsMargins(0, 0, 0, 0);
    setChildrenCollapsible(false);
    // Keep a one-pixel hit target for pane resizing without allowing the
    // platform style to paint a visible grip or divider decoration.
    setHandleWidth(1);
    setStyleSheet("QSplitter { background: transparent; border: none; } QSplitter::handle { background: transparent; border: none; }");
    addWidget(primary);
}

QSplitterHandle* SplitView::createHandle()
{
    return new InvisibleSplitHandle(orientation(), this);
}

void SplitView::open(QWidget* page, QWidget* home)
{
    clear_secondary();
    m_secondary = page;
    m_home = home;
    addWidget(page);
    page->show();
    setSizes({ width() / 2, width() / 2 });
}

void SplitView::clear_secondary()
{
    if (m_secondary && m_home) {
        m_home->layout()->addWidget(m_secondary);
        m_secondary->show();
    }
    m_secondary.clear();
    m_home.clear();
}

}
