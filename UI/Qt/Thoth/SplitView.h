/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <QPointer>
#include <QSplitter>

namespace Thoth {

// Keep each page's original stack slot alive while lending the page to the second pane.
class SplitView final : public QSplitter {
public:
    explicit SplitView(QWidget* primary, QWidget* parent = nullptr);
    void open(QWidget* page, QWidget* home);
    void clear_secondary();
    QWidget* secondary() const { return m_secondary; }

protected:
    QSplitterHandle* createHandle() override;

private:
    QPointer<QWidget> m_secondary;
    QPointer<QWidget> m_home;
};

}
