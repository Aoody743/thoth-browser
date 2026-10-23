/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#pragma once

#include <utility>
#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <QToolButton>

namespace Thoth {

// One fixed control, one standard click action, one short acknowledgement.
// Do not combine native tool-button frames with custom chrome frames.
class NavigationButton final : public QToolButton {
public:
    explicit NavigationButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setCheckable(false);
        setAutoRepeat(false);
        setStyleSheet("QToolButton, QToolButton:hover, QToolButton:pressed, QToolButton:checked, QToolButton:focus { background: transparent; border: none; padding: 0; }"
                      "QToolButton::menu-indicator { image: none; width: 0; }");
        m_timer.setInterval(50);
        connect(this, &QToolButton::clicked, this, [this] {
            m_click.restart();
            if (isVisible())
                m_timer.start();
            update();
        });
        connect(&m_timer, &QTimer::timeout, this, [this] {
            m_angle = (m_angle + 20) % 360;
            if (!m_loading && !acknowledging_click())
                m_timer.stop();
            update();
        });
    }

    void set_loading(bool loading)
    {
        m_loading = loading;
        if (isVisible() && (loading || acknowledging_click()))
            m_timer.start();
        else
            m_timer.stop();
        update();
    }
    void set_loading_icon(QIcon icon) { m_loading_icon = std::move(icon); }

protected:
    void showEvent(QShowEvent* event) override
    {
        QToolButton::showEvent(event);
        if (m_loading || acknowledging_click())
            m_timer.start();
    }
    void hideEvent(QHideEvent* event) override
    {
        m_timer.stop();
        QToolButton::hideEvent(event);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        // Hover does not introduce another frame, scale, border or icon state.
        if (isEnabled() && (isDown() || acknowledging_click())) {
            auto feedback = palette().color(QPalette::Highlight);
            feedback.setAlpha(38);
            painter.setPen(Qt::NoPen);
            painter.setBrush(feedback);
            painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
        }
        if (hasFocus()) {
            painter.setPen(QPen(palette().color(QPalette::Highlight), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 7, 7);
        }
        auto size = iconSize();
        painter.translate(rect().center());
        if (m_loading && !m_loading_icon.isNull())
            painter.rotate(m_angle);
        auto const& glyph = m_loading && !m_loading_icon.isNull() ? m_loading_icon : icon();
        glyph.paint(&painter, QRect(QPoint(-size.width() / 2, -size.height() / 2), size), Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    }

private:
    bool acknowledging_click() const { return m_click.isValid() && m_click.elapsed() < 200; }
    bool m_loading { false };
    int m_angle { 0 };
    QIcon m_loading_icon;
    QElapsedTimer m_click;
    QTimer m_timer;
};

}
