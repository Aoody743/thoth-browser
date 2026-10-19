/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "Sidebar.h"
#include "Localization.h"
#include <LibURL/InternalURLs.h>
#include <LibWebView/Application.h>
#include <LibWebView/Profile.h>
#include <QAccessible>
#include <QActionGroup>
#include <QApplication>
#include <QAbstractAnimation>
#include <QActionGroup>
#include <QBuffer>
#include <QConicalGradient>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QIconEngine>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <UI/Qt/Application.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/ChromeStyle.h>
#include <UI/Qt/Icon.h>
#include <UI/Qt/StringUtils.h>
#include <UI/Qt/Thoth/Appearance.h>
#include <UI/Qt/TabBar.h>
#include <algorithm>
#include <cmath>
#include <functional>

namespace Thoth {
static constexpr auto ITEM_MIME = "application/x-thoth-sidebar-item";
static constexpr int SIDEBAR_REVEAL_WIDTH = 20;
static constexpr int SIDEBAR_REVEAL_DELAY_MS = 240;
static constexpr int SIDEBAR_MIN_EXPANDED_WIDTH = 190;
static constexpr int SIDEBAR_MAX_EXPANDED_WIDTH = 400;
static constexpr int ID_ROLE = Qt::UserRole;
static constexpr int SECTION_ROLE = Qt::UserRole + 1;
static constexpr int PAGE_ROLE = Qt::UserRole + 2;
static constexpr int GROUP_ROLE = Qt::UserRole + 3;
static constexpr int ICONS_ROLE = Qt::UserRole + 4;
static constexpr int TITLES_ROLE = Qt::UserRole + 5;
static constexpr int ACTIVE_ROLE = Qt::UserRole + 6;
static constexpr int EXPANDED_ROLE = Qt::UserRole + 7;
static constexpr int SEPARATOR_ROLE = Qt::UserRole + 8;
static constexpr int ACTION_ROLE = Qt::UserRole + 9;
static constexpr int DROP_SLOT_ROLE = Qt::UserRole + 10;
static constexpr int LOADING_PAGES_ROLE = Qt::UserRole + 11;
static constexpr int LOADING_COLOR_ROLE = Qt::UserRole + 12;
static constexpr int PAGE_IDS_ROLE = Qt::UserRole + 13;

static bool prompt_for_name(QWidget* parent, QString title, QString label, QString initial, QString accept_text, QString& result)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setMinimumWidth(360);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setStyleSheet(
        "QDialog { background: transparent; }"
        "QFrame#ThothPromptCard { background: palette(window); border: 1px solid palette(mid); border-radius: 16px; }"
        "QLabel#ThothPromptLabel { color: palette(window-text); font-size: 13px; }"
        "QLineEdit#ThothPromptInput { min-height: 38px; padding: 0 12px; border: 1px solid palette(mid); border-radius: 10px; background: palette(base); color: palette(text); font-size: 14px; }"
        "QLineEdit#ThothPromptInput:focus { border: 2px solid palette(highlight); }"
        "QPushButton { min-height: 34px; padding: 0 16px; border-radius: 8px; }"
        "QPushButton#ThothPromptPrimary { background: palette(highlight); color: palette(highlighted-text); border: none; font-weight: 600; }"
        "QPushButton#ThothPromptPrimary:hover { background: palette(highlight); }");

    auto* outer = new QVBoxLayout(&dialog);
    outer->setContentsMargins(8, 8, 8, 8);
    auto* card = new QFrame(&dialog);
    card->setObjectName("ThothPromptCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 70));
    card->setGraphicsEffect(shadow);
    outer->addWidget(card);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(12);
    auto* heading = new QLabel(title, &dialog);
    heading->setStyleSheet("font-size: 18px; font-weight: 650; color: palette(window-text);");
    layout->addWidget(heading);
    auto* prompt = new QLabel(label, &dialog);
    prompt->setObjectName("ThothPromptLabel");
    layout->addWidget(prompt);
    auto* input = new QLineEdit(initial, &dialog);
    input->setObjectName("ThothPromptInput");
    input->selectAll();
    layout->addWidget(input);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(accept_text);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("ThothPromptPrimary");
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (!input->text().trimmed().isEmpty())
            dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(input, &QLineEdit::returnPressed, &dialog, [&] {
        if (!input->text().trimmed().isEmpty())
            dialog.accept();
    });
    input->setFocus();
    localize_widget_tree(dialog);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    result = input->text().trimmed();
    return !result.isEmpty();
}

static void paint_rounded_icon(QPainter* painter, QIcon const& icon, QRect rect)
{
    if (rect.isEmpty() || icon.isNull())
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    auto radius = rect.width() * 0.24;
    clip.addRoundedRect(QRectF(rect), radius, radius);
    painter->setClipPath(clip);
    icon.paint(painter, rect, Qt::AlignCenter);
    painter->restore();
}

static QString sidebar_menu_style_sheet(QColor const& background, QColor const& foreground, QColor const& hover, QColor const& selected, QColor const& border, QColor const& disabled, QColor const& divider)
{
    return QString(
               "QMenu { background: %1; color: %2; border: 1px solid %5; border-radius: 10px; padding: 5px; }"
               "QMenu::item { background: transparent; padding: 7px 26px 7px 12px; border-radius: 7px; min-height: 20px; }"
               "QMenu::item:selected { background: %3; color: %2; }"
               "QMenu::item:pressed { background: %4; }"
               "QMenu::item:disabled { color: %6; }"
               "QMenu::separator { height: 1px; background: %7; margin: 5px 8px; }"
               "QMenu::icon { margin-left: 5px; }"
               "QMenu::indicator { width: 14px; height: 14px; }")
        .arg(background.name(), foreground.name(), hover.name(), selected.name(), border.name(), disabled.name(), divider.name());
}

class SidebarDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(QStyleOptionViewItem const&, QModelIndex const& index) const override
    {
        if (index.data(ACTION_ROLE).toString() == "newtab")
            return { 0, index.data(SEPARATOR_ROLE).toBool() ? 44 : 34 };
        if (index.data(ID_ROLE).toString().isEmpty())
            return { 0, index.data(DROP_SLOT_ROLE).toBool() ? 14 : index.data(SEPARATOR_ROLE).toBool() ? 18 : 0 };
        return { 0, index.data(GROUP_ROLE).toStringList().size() == 2 ? 40 : 34 };
    }
    void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override
    {
        auto dark = Ladybird::ChromeStyle::is_dark(option.palette);
        auto rect = option.rect.adjusted(-10, 1, -2, -1);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setFont(option.font);
        if (index.data(ACTION_ROLE).toString() == "newtab") {
            if (index.data(SEPARATOR_ROLE).toBool()) {
                painter->setPen(dark ? QColor(255, 255, 255, 22) : QColor(56, 45, 82, 26));
                painter->drawLine(rect.left() + 8, rect.top() + 3, rect.right() - 8, rect.top() + 3);
                rect.setTop(rect.top() + 10);
            }
            auto ink = dark ? QColor("#a39bad") : QColor("#8d8598");
            painter->setPen(QPen(ink, 1.3, Qt::SolidLine, Qt::RoundCap));
            auto center = QPoint(rect.left() + 16, rect.center().y());
            painter->drawLine(center.x() - 4, center.y(), center.x() + 4, center.y());
            painter->drawLine(center.x(), center.y() - 4, center.x(), center.y() + 4);
            painter->setPen(ink);
            painter->drawText(rect.adjusted(32, 0, -8, 0), Qt::AlignVCenter, Thoth::localized_text("New Tab"));
            painter->restore();
            return;
        }
        if (index.data(ID_ROLE).toString().isEmpty()) {
            if (index.data(SEPARATOR_ROLE).toBool()) {
                painter->setPen(dark ? QColor(255, 255, 255, 22) : QColor(56, 45, 82, 26));
                painter->drawLine(rect.left() + 8, rect.center().y(), rect.right() - 8, rect.center().y());
            }
            painter->restore();
            return;
        }
        auto selected = option.state & QStyle::State_Selected;
        auto hovered = option.state & QStyle::State_MouseOver;
        auto pages = index.data(GROUP_ROLE).toStringList();
        auto page_ids = index.data(PAGE_IDS_ROLE).toStringList();
        auto loading_pages = index.data(LOADING_PAGES_ROLE).toStringList();
        auto loading_frame = parent() ? parent()->property("ThothLoadingFrame").toInt() : 0;
        auto draw_loading_ring = [&](QRectF card, qreal radius) {
            auto color = index.data(LOADING_COLOR_ROLE).value<QColor>();
            if (!color.isValid())
                color = option.palette.color(QPalette::Highlight);
            color = dark ? color.lighter(145) : color.darker(155);
            auto faded = color;
            faded.setAlpha(0);
            auto soft = color;
            soft.setAlpha(72);
            QConicalGradient gradient(card.center(), (loading_frame * 15) % 360);
            gradient.setColorAt(0.0, faded);
            gradient.setColorAt(0.68, faded);
            gradient.setColorAt(0.75, soft);
            gradient.setColorAt(0.82, color);
            gradient.setColorAt(0.92, color);
            gradient.setColorAt(0.98, soft);
            gradient.setColorAt(1.0, faded);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(soft, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawRoundedRect(card.adjusted(1, 1, -1, -1), radius, radius);
            painter->setPen(QPen(QBrush(gradient), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawRoundedRect(card.adjusted(1, 1, -1, -1), radius, radius);
            painter->setPen(Qt::NoPen);
        };
        painter->setPen(Qt::NoPen);
        if (pages.size() == 2) {
            painter->setBrush(dark ? QColor("#34323f") : QColor("#dfdce8"));
            painter->drawRoundedRect(rect, 10, 10);
            auto icons = index.data(ICONS_ROLE).toList();
            auto titles = index.data(TITLES_ROLE).toStringList();
            auto chip_font = option.font;
            chip_font.setPointSizeF(11.5);
            painter->setFont(chip_font);
            QFontMetrics metrics(chip_font);
            for (int n = 0; n < 2; ++n) {
                auto chip = QRect(rect.x() + n * rect.width() / 2 + 3, rect.y() + 3, rect.width() / 2 - 6, rect.height() - 6);
                if (selected && pages[n] == index.data(ACTIVE_ROLE).toString()) {
                    painter->setBrush(dark ? QColor("#514b60") : QColor("#f8f7fc"));
                    painter->drawRoundedRect(chip, 7, 7);
                }
                if (n < icons.size())
                    paint_rounded_icon(painter, icons[n].value<QIcon>(), QRect(chip.x() + 6, chip.center().y() - 8, 16, 16));
                auto text = chip.adjusted(26, 0, hovered && n == 1 ? -27 : -5, 0);
                painter->setPen(dark ? QColor("#eeeaf6") : QColor("#34313f"));
                painter->drawText(text, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(titles.value(n), Qt::ElideRight, text.width()));
                painter->setPen(Qt::NoPen);
                if (loading_pages.contains(page_ids.value(n)))
                    draw_loading_ring(QRectF(chip), 7);
            }
        } else {
            if (selected || hovered) {
                painter->setBrush(dark ? QColor(selected ? "#403a4c" : "#302d39") : QColor(selected ? "#dfdbe9" : "#e6e3ed"));
                painter->drawRoundedRect(rect, 8, 8);
            }
            auto folder = index.data(PAGE_ROLE).toString() == "folder";
            auto text_rect = rect.adjusted(32, 0, hovered && !folder ? -34 : -9, 0);
            if (folder) {
                auto font = option.font;
                font.setWeight(QFont::Medium);
                painter->setFont(font);
                painter->setPen(QPen(dark ? QColor("#a7a1b5") : QColor("#777082"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                auto x = rect.left() + 12.0;
                auto y = rect.center().y();
                QPainterPath chevron;
                if (index.data(EXPANDED_ROLE).toBool()) {
                    chevron.moveTo(x - 3, y - 1.5);
                    chevron.lineTo(x, y + 1.5);
                    chevron.lineTo(x + 3, y - 1.5);
                } else {
                    chevron.moveTo(x - 1.5, y - 3);
                    chevron.lineTo(x + 1.5, y);
                    chevron.lineTo(x - 1.5, y + 3);
                }
                painter->drawPath(chevron);
                text_rect.setLeft(rect.left() + 26);
            } else {
                paint_rounded_icon(painter, index.data(Qt::DecorationRole).value<QIcon>(), QRect(rect.left() + 9, rect.center().y() - 9, 18, 18));
            }
            painter->setPen(dark ? QColor("#e5e1ed") : QColor("#3b3744"));
            painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(painter->font()).elidedText(index.data().toString(), Qt::ElideRight, text_rect.width()));
            if (!folder && loading_pages.contains(page_ids.value(0)))
                draw_loading_ring(QRectF(rect), 8);
        }
        if (hovered && index.data(PAGE_ROLE).toString() != "folder") {
            auto close_rect = QRect(rect.right() - 28, rect.center().y() - 11, 22, 22);
            painter->setPen(Qt::NoPen);
            painter->setBrush(dark ? QColor("#514c5b") : QColor("#d8d3df"));
            painter->drawEllipse(close_rect);
            painter->setPen(QPen(dark ? QColor("#e8e4ed") : QColor("#514b59"), 1.35, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(close_rect.center() + QPoint(-3, -3), close_rect.center() + QPoint(3, 3));
            painter->drawLine(close_rect.center() + QPoint(3, -3), close_rect.center() + QPoint(-3, 3));
        }
        painter->restore();
    }
};

class FavouriteDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const override
    {
        auto size = index.data(Qt::SizeHintRole).toSize();
        return size.isValid() ? size : QSize(option.rect.width(), 48);
    }

    void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override
    {
        auto dark = Ladybird::ChromeStyle::is_dark(option.palette);
        auto selected = option.state & QStyle::State_Selected;
        auto hovered = option.state & QStyle::State_MouseOver;
        auto rect = option.rect.adjusted(2, 2, -2, -2);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        auto tile = dark ? QColor(selected ? "#484052" : hovered ? "#393542" : "#302d38") : QColor(selected ? "#ded9e8" : hovered ? "#e5e1ec" : "#eae7f1");
        painter->setPen(selected ? QPen(dark ? QColor("#716584") : QColor("#c5bbd4"), 1) : Qt::NoPen);
        painter->setBrush(tile);
        painter->drawRoundedRect(rect, 10, 10);
        auto icon = index.data(Qt::DecorationRole).value<QIcon>();
        auto size = QSize(26, 26);
        auto icon_rect = QRect(QPoint(rect.center().x() - size.width() / 2, rect.center().y() - size.height() / 2), size);
        paint_rounded_icon(painter, icon, icon_rect);
        painter->restore();
    }
};

static bool is_sidebar_drag(QDropEvent const& event)
{
    return event.mimeData()->hasFormat(ITEM_MIME);
}

class SidebarRevealHandle final : public QToolButton {
public:
    using QToolButton::QToolButton;
    std::function<void()> on_click;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        auto pressed = isDown();
        auto background = QColor(property("ThothSidebarBackground").toString());
        if (!background.isValid())
            background = palette().color(QPalette::Window);
        background.setAlpha(255);
        painter.fillRect(rect(), background);
        auto color = Ladybird::ChromeStyle::is_dark(palette()) ? QColor("#d8cdea") : QColor("#665c78");
        color.setAlpha(pressed ? 238 : 165);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        auto handle_width = pressed ? 6.0 : 4.0;
        auto handle_height = pressed ? 46.0 : 40.0;
        painter.drawRoundedRect(QRectF((width() - handle_width) / 2.0, (height() - handle_height) / 2.0, handle_width, handle_height), 2.0, 2.0);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            setDown(true);
            update();
            event->accept();
            if (on_click)
                on_click();
            return;
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            setDown(false);
            update();
            event->accept();
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }
};

class NewTabAddressEdit final : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
    std::function<void()> on_escape;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            event->accept();
            if (on_escape)
                on_escape();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

class NewTabPromptOverlay final : public QFrame {
public:
    using QFrame::QFrame;
    QFrame* card {};
    QLineEdit* address {};
    std::function<void()> on_dismiss;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (!card || !card->geometry().contains(event->position().toPoint())) {
            event->accept();
            if (on_dismiss)
                on_dismiss();
            return;
        }
        QFrame::mousePressEvent(event);
    }
};

struct SessionRegistry {
    QJsonObject windows;
    QHash<QString, Sidebar*> live;
    QString path;
    bool quitting { false };
    bool restoring { false };
    bool has_seen_window { false };
    QByteArray last_saved;
    SessionRegistry()
    {
        path = qstring_from_ak_string(WebView::Application::profile().paths().data) + "/thoth-sidebar.json";
        auto document = SidebarModel::read_document(path);
        windows = document["windows"].toObject();
    }
};
static SessionRegistry& registry()
{
    static SessionRegistry state;
    return state;
}

class SidebarTree final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    std::function<void(QString, QString, QString, QString)> on_drop;
    std::function<bool(QString, QString)> can_merge;
    std::function<void(QString, QString)> on_merge;
    std::function<void(bool)> on_drag_state;
    void reset_drop_target() { m_drop_target = nullptr; m_merge_target = false; }

    bool handle_viewport_drag_event(QEvent* event)
    {
        if (event->type() == QEvent::DragLeave) {
            m_drop_target = nullptr;
            m_merge_target = false;
            viewport()->update();
            return true;
        }
        if (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)
            return false;
        auto* drop_event = static_cast<QDropEvent*>(event);
        if (!is_sidebar_drag(*drop_event))
            return false;
        auto* source = qobject_cast<QWidget*>(drop_event->source());
        if (!source || source->window() != window())
            return false;
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto position = drop_event->position().toPoint();
            m_drop_target = itemAt(position);
            m_merge_target = false;
            if (m_drop_target && !m_drop_target->data(0, ID_ROLE).toString().isEmpty()
                && m_drop_target->data(0, PAGE_ROLE).toString() != "folder"
                && m_drop_target->data(0, ACTION_ROLE).toString().isEmpty()) {
                auto target_rect = visualItemRect(m_drop_target);
                auto source_id = QString::fromUtf8(drop_event->mimeData()->data(ITEM_MIME));
                auto target_id = m_drop_target->data(0, ID_ROLE).toString();
                m_merge_target = position.y() >= target_rect.top() + target_rect.height() / 4
                    && position.y() <= target_rect.bottom() - target_rect.height() / 4
                    && source_id != target_id && can_merge && can_merge(source_id, target_id);
            }
            viewport()->update();
            drop_event->setDropAction(Qt::MoveAction);
            drop_event->accept();
            return true;
        }
        if (event->type() != QEvent::Drop)
            return false;
        auto* target = itemAt(drop_event->position().toPoint());
        auto source_id = QString::fromUtf8(drop_event->mimeData()->data(ITEM_MIME));
        if (m_merge_target && target && on_merge) {
            on_merge(source_id, target->data(0, ID_ROLE).toString());
            m_drop_target = nullptr;
            m_merge_target = false;
            viewport()->update();
            drop_event->setDropAction(Qt::MoveAction);
            drop_event->accept();
            return true;
        }
        QString section = "temporary", parent, before;
        if (target) {
            section = target->data(0, SECTION_ROLE).toString();
            auto id = target->data(0, ID_ROLE).toString();
            if (target->data(0, PAGE_ROLE).toString() == "folder")
                parent = id;
            else if (!id.isEmpty()) {
                before = id;
                if (target->parent())
                    parent = target->parent()->data(0, ID_ROLE).toString();
            }
        }
        if (on_drop)
            on_drop(source_id, section, parent, before);
        m_drop_target = nullptr;
        m_merge_target = false;
        viewport()->update();
        drop_event->setDropAction(Qt::MoveAction);
        drop_event->accept();
        return true;
    }

protected:
    void startDrag(Qt::DropActions) override
    {
        auto* item = currentItem();
        if (!item || !(item->flags() & Qt::ItemIsDragEnabled) || item->data(0, ID_ROLE).toString().isEmpty())
            return;
        auto* mime = new QMimeData;
        mime->setData(ITEM_MIME, item->data(0, ID_ROLE).toString().toUtf8());
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        auto pixmap = viewport()->grab(visualItemRect(item));
        if (!pixmap.isNull()) {
            drag->setPixmap(pixmap);
            drag->setHotSpot(pixmap.rect().center());
        }
        if (on_drag_state)
            on_drag_state(true);
        drag->exec(Qt::MoveAction);
        if (on_drag_state)
            on_drag_state(false);
    }
    void dragEnterEvent(QDragEnterEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QTreeWidget::dragEnterEvent(e);
    }
    void dragMoveEvent(QDragMoveEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QTreeWidget::dragMoveEvent(e);
    }
    void dragLeaveEvent(QDragLeaveEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QTreeWidget::dragLeaveEvent(e);
    }
    void dropEvent(QDropEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QTreeWidget::dropEvent(e);
    }

    void paintEvent(QPaintEvent* e) override
    {
        QTreeWidget::paintEvent(e);
        if (!m_drop_target || (!m_drop_target->data(0, DROP_SLOT_ROLE).toBool() && !m_merge_target))
            return;
        auto rect = visualItemRect(m_drop_target).adjusted(3, 2, -3, -2);
        if (rect.isEmpty())
            return;
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        auto accent = palette().color(QPalette::Highlight);
        painter.setPen(QPen(accent, m_merge_target ? 2.0 : 1.3, m_merge_target ? Qt::SolidLine : Qt::DashLine));
        painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), m_merge_target ? 34 : 18));
        painter.drawRoundedRect(rect, 7, 7);
    }

private:
    QTreeWidgetItem* m_drop_target { nullptr };
    bool m_merge_target { false };
};
class FavouriteList final : public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<void(QString, QString)> on_drop;

    bool handle_viewport_drag_event(QEvent* event)
    {
        if (event->type() == QEvent::DragLeave) {
            m_drop_hover = false;
            if (count() == 0)
                setFixedHeight(0);
            viewport()->update();
            return true;
        }
        if (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)
            return false;
        auto* drop_event = static_cast<QDropEvent*>(event);
        if (!is_sidebar_drag(*drop_event))
            return false;
        auto* source = qobject_cast<QWidget*>(drop_event->source());
        if (!source || source->window() != window())
            return false;
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            m_drop_hover = true;
            if (count() == 0) {
                setFixedHeight(60);
                setVisible(true);
            }
            viewport()->update();
            drop_event->setDropAction(Qt::MoveAction);
            drop_event->accept();
            return true;
        }
        if (event->type() != QEvent::Drop)
            return false;
        auto* target = itemAt(drop_event->position().toPoint());
        if (on_drop)
            on_drop(QString::fromUtf8(drop_event->mimeData()->data(ITEM_MIME)), target ? target->data(ID_ROLE).toString() : QString {});
        m_drop_hover = false;
        if (count() == 0)
            setFixedHeight(0);
        viewport()->update();
        drop_event->setDropAction(Qt::MoveAction);
        drop_event->accept();
        return true;
    }

protected:
    void startDrag(Qt::DropActions) override
    {
        if (!currentItem() || !(currentItem()->flags() & Qt::ItemIsDragEnabled) || currentItem()->data(ID_ROLE).toString().isEmpty())
            return;
        auto* mime = new QMimeData;
        mime->setData(ITEM_MIME, currentItem()->data(ID_ROLE).toString().toUtf8());
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        auto pixmap = viewport()->grab(visualItemRect(currentItem()));
        if (!pixmap.isNull()) {
            drag->setPixmap(pixmap);
            drag->setHotSpot(pixmap.rect().center());
        }
        drag->exec(Qt::MoveAction);
    }
    void dragEnterEvent(QDragEnterEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QListWidget::dragEnterEvent(e);
    }
    void dragMoveEvent(QDragMoveEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QListWidget::dragMoveEvent(e);
    }
    void dropEvent(QDropEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QListWidget::dropEvent(e);
    }
    void dragLeaveEvent(QDragLeaveEvent* e) override
    {
        if (!handle_viewport_drag_event(e))
            QListWidget::dragLeaveEvent(e);
    }
    void paintEvent(QPaintEvent* e) override
    {
        QListWidget::paintEvent(e);
        if (!m_drop_hover)
            return;
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        auto bounds = viewport()->rect().adjusted(3, 3, -3, -3);
        auto accent = palette().color(QPalette::Highlight);
        painter.setPen(QPen(accent, 1.4, Qt::DashLine));
        painter.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 18));
        painter.drawRoundedRect(bounds, 9, 9);
        if (count() == 0) {
            QPainterPath star;
            auto center = QPointF(bounds.center());
            constexpr auto pi = 3.14159265358979323846;
            for (int point = 0; point < 10; ++point) {
                auto angle = -pi / 2.0 + point * pi / 5.0;
                auto radius = point % 2 == 0 ? 8.0 : 3.8;
                auto position = center + QPointF(std::cos(angle) * radius, std::sin(angle) * radius);
                if (point == 0)
                    star.moveTo(position);
                else
                    star.lineTo(position);
            }
            star.closeSubpath();
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
            painter.drawPath(star);
        }
    }

private:
    bool m_drop_hover { false };
};
class WorkspaceIconEngine final : public QIconEngine {
public:
    WorkspaceIconEngine(QString symbol, QColor color)
        : m_symbol(std::move(symbol))
        , m_color(std::move(color))
    {
    }

    QIconEngine* clone() const override
    {
        return new WorkspaceIconEngine(m_symbol, m_color);
    }

    QSize actualSize(QSize const& size, QIcon::Mode, QIcon::State) override
    {
        return size;
    }

    // Qt's default QIconEngine::pixmap()/scaledPixmap() can hand back an
    // uninitialized pixmap for engines that only implement paint(), which shows
    // up as random colored noise behind the symbol on Windows. Fill the buffer
    // ourselves so the icon always has a clean alpha background.
    QPixmap pixmap(QSize const& size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap result(size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return result;
    }

    QPixmap scaledPixmap(QSize const& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        auto pixel_width = qMax(1, qRound(size.width() * scale));
        auto pixel_height = qMax(1, qRound(size.height() * scale));
        QPixmap result(pixel_width, pixel_height);
        result.setDevicePixelRatio(scale);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        painter.scale(scale, scale);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return result;
    }

    void paint(QPainter* painter, QRect const& rect, QIcon::Mode mode, QIcon::State) override
    {
        if (!painter || rect.isEmpty())
            return;

        auto color = m_color;
        if (color.lightness() < 105)
            color = color.lighter(155);
        else if (color.lightness() > 220)
            color = color.darker(155);
        if (mode == QIcon::Disabled)
            color.setAlpha(105);

        painter->save();
        // QIcon may render an engine into a reused pixmap. Clear the icon's
        // transparent area first so stale pixels cannot appear as colored
        // speckles around the workspace glyph.
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->fillRect(rect, Qt::transparent);
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->translate(rect.topLeft());
        painter->scale(rect.width() / 20.0, rect.height() / 20.0);
        painter->setPen(QPen(color, 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);

        if (m_symbol == "book") {
            QPainterPath pages;
            pages.moveTo(4.6, 5.3);
            pages.cubicTo(6.8, 4.6, 8.7, 5.2, 10, 6.2);
            pages.cubicTo(11.3, 5.2, 13.2, 4.6, 15.4, 5.3);
            pages.lineTo(15.4, 14.5);
            pages.cubicTo(13.2, 13.8, 11.3, 14.4, 10, 15.4);
            pages.cubicTo(8.7, 14.4, 6.8, 13.8, 4.6, 14.5);
            pages.closeSubpath();
            painter->drawPath(pages);
            painter->drawLine(QPointF(10, 6.2), QPointF(10, 15.1));
            painter->drawLine(QPointF(6.2, 7.6), QPointF(8.1, 7.9));
            painter->drawLine(QPointF(11.9, 7.9), QPointF(13.8, 7.6));
        } else if (m_symbol == "diamond") {
            QPainterPath diamond;
            diamond.moveTo(10, 3.9);
            diamond.lineTo(16.1, 10);
            diamond.lineTo(10, 16.1);
            diamond.lineTo(3.9, 10);
            diamond.closeSubpath();
            painter->drawPath(diamond);
            painter->drawLine(QPointF(6.6, 6.6), QPointF(10, 16.1));
            painter->drawLine(QPointF(13.4, 6.6), QPointF(10, 16.1));
        } else if (m_symbol == "globe") {
            painter->drawEllipse(QRectF(3.9, 3.9, 12.2, 12.2));
            painter->drawEllipse(QRectF(7.2, 3.9, 5.6, 12.2));
            painter->drawArc(QRectF(4.5, 6.5, 11, 7), 0, 180 * 16);
            painter->drawArc(QRectF(4.5, 6.5, 11, 7), 180 * 16, 180 * 16);
        } else {
            painter->drawEllipse(QRectF(8.5, 8.5, 3, 3));
            painter->save();
            painter->translate(10, 10);
            painter->rotate(-30);
            painter->drawEllipse(QRectF(-7.1, -2.7, 14.2, 5.4));
            painter->restore();
        }
        painter->restore();
    }

private:
    QString m_symbol;
    QColor m_color;
};

static QIcon workspace_icon(Workspace const& space, bool)
{
    auto color = QColor(space.color);
    if (!color.isValid())
        color = QColor("#b9a5df");
    QPixmap pixmap(40, 40);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    WorkspaceIconEngine engine(space.symbol, color);
    engine.paint(&painter, QRect(0, 0, 20, 20), QIcon::Normal, QIcon::Off);
    painter.end();
    return QIcon(pixmap);
}
static QIcon workspace_color_swatch(QString const& color)
{
    QPixmap pixmap(32, 32);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    auto fill = QColor(color);
    if (!fill.isValid())
        fill = QColor("#b9a5df");
    painter.setPen(QPen(fill.darker(118), 0.8));
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(2.5, 2.5, 11, 11));
    return QIcon(pixmap);
}
class WorkspaceButton final : public QToolButton {
public:
    using QToolButton::QToolButton;
    std::function<void(QString)> on_drop;

protected:
    void set_drop_target(bool active)
    {
        if (property("dropTarget").toBool() == active)
            return;
        setProperty("dropTarget", active);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
    void dragEnterEvent(QDragEnterEvent* e) override
    {
        auto* source = qobject_cast<QWidget*>(e->source());
        if (source && source->window() == window() && e->mimeData()->hasFormat(ITEM_MIME)) {
            set_drop_target(true);
            e->setDropAction(Qt::MoveAction);
            e->acceptProposedAction();
        }
    }
    void dragMoveEvent(QDragMoveEvent* e) override
    {
        auto* source = qobject_cast<QWidget*>(e->source());
        if (source && source->window() == window() && e->mimeData()->hasFormat(ITEM_MIME)) {
            set_drop_target(true);
            e->setDropAction(Qt::MoveAction);
            e->acceptProposedAction();
        }
    }
    void dragLeaveEvent(QDragLeaveEvent* e) override
    {
        set_drop_target(false);
        QToolButton::dragLeaveEvent(e);
    }
    void dropEvent(QDropEvent* e) override
    {
        auto* source = qobject_cast<QWidget*>(e->source());
        if (!source || source->window() != window() || !e->mimeData()->hasFormat(ITEM_MIME))
            return;
        set_drop_target(false);
        if (on_drop)
            on_drop(QString::fromUtf8(e->mimeData()->data(ITEM_MIME)));
        e->setDropAction(Qt::MoveAction);
        e->acceptProposedAction();
    }
};

Sidebar::Sidebar(Ladybird::TabWidget& tabs, QStackedWidget& navigation)
    : QWidget(&tabs)
    , m_tabs(tabs)
    , m_navigation(navigation)
    , m_window_id(SidebarModel::new_id())
{
#if defined(Q_OS_WIN)
    // The URL toolbar stack is displayed by TabWidget above the full-width splitter.
    m_navigation.setParent(&tabs);
#endif
    m_private = browser().is_private() == WebView::IsPrivate::Yes;
    if (m_private)
        m_model.workspaces.first().name = "Private";
    setObjectName("ThothSidebar");
    setProperty("LadybirdWindowDragRegion", true);
#if defined(Q_OS_MACOS)
    // WebContentView is backed by a native QRhiWidget; keep the hover overlay above it.
    setAttribute(Qt::WA_NativeWindow);
#endif
    auto sidebar_font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    sidebar_font.setPointSizeF(12);
    sidebar_font.setWeight(QFont::Normal);
    setFont(sidebar_font);
    setMinimumWidth(80);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 10);
    layout->setSpacing(8);
    auto* top_widget = new QWidget(this);
    top_widget->setObjectName("ThothSidebarTopBar");
    top_widget->setProperty("LadybirdWindowDragRegion", true);
    top_widget->setFixedHeight(48);
    auto* top = new QHBoxLayout(top_widget);
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(0);
#if defined(Q_OS_MACOS)
    top->addSpacing(66);
#endif
    m_navigation_buttons = new QStackedWidget(this);
    m_navigation_buttons->setProperty("LadybirdWindowDragRegion", true);
    m_navigation_buttons->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_navigation_buttons->setFixedHeight(46);
    top->addWidget(m_navigation_buttons, 1);
    layout->addWidget(top_widget);
#if defined(Q_OS_WIN)
    // Windows uses a full-width browser toolbar above the workspace pane.
    top_widget->hide();
#endif
    m_body = new QWidget(this);
    m_body->setProperty("LadybirdWindowDragRegion", true);
    auto* body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(8);
#if !defined(Q_OS_WIN)
    body->addWidget(&m_navigation);
#endif
    auto* favourites = new FavouriteList(this);
    m_favourites = favourites;
    favourites->setObjectName("ThothFavourites");
    favourites->setItemDelegate(new FavouriteDelegate(favourites));
    favourites->setViewMode(QListView::IconMode);
    favourites->setFlow(QListView::LeftToRight);
    favourites->setResizeMode(QListView::Adjust);
    favourites->setMovement(QListView::Static);
    favourites->setWrapping(true);
    favourites->setGridSize({ 72, 60 });
    favourites->setIconSize({ 26, 26 });
    favourites->setSpacing(4);
    favourites->setTextElideMode(Qt::ElideRight);
    favourites->setDragEnabled(true);
    favourites->setAcceptDrops(true);
    favourites->setDragDropMode(QAbstractItemView::DragDrop);
    favourites->setDefaultDropAction(Qt::MoveAction);
    favourites->setDragDropOverwriteMode(false);
    favourites->setAutoScroll(true);
    favourites->setDropIndicatorShown(true);
    favourites->setSelectionMode(QAbstractItemView::SingleSelection);
    favourites->setMouseTracking(true);
    favourites->setFrameShape(QFrame::NoFrame);
    favourites->setContextMenuPolicy(Qt::CustomContextMenu);
    favourites->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->addWidget(favourites);
    m_space_header = new QToolButton(this);
    m_space_header->setObjectName("ThothSpaceHeader");
    m_space_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_space_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_space_header->setFixedHeight(36);
    m_space_header->setIconSize({ 20, 20 });
    m_space_header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_space_header, &QToolButton::clicked, this, [this] { workspace_menu(m_model.active_workspace, m_space_header->mapToGlobal(QPoint(0, m_space_header->height()))); });
    connect(m_space_header, &QWidget::customContextMenuRequested, this, [this](QPoint p) { workspace_menu(m_model.active_workspace, m_space_header->mapToGlobal(p)); });
    body->addWidget(m_space_header);
    connect(favourites, &QListWidget::itemClicked, this, [this](auto* i) {
        auto id = i->data(ID_ROLE).toString();
        if (id.isEmpty()) {
            if (auto* current = m_model.item(m_model.active); current && current->pages.size() == 1)
                drop_item(current->id, "favourite", {});
        } else
            activate(id);
    });
    connect(favourites, &QWidget::customContextMenuRequested, this, [this](QPoint p) { auto* i = m_favourites->itemAt(p); context_menu(i ? i->data(ID_ROLE).toString() : QString {}, m_favourites->viewport()->mapToGlobal(p)); });
    favourites->on_drop = [this](QString id, QString before) { drop_item(id, "favourite", {}, before); };
    auto* tree = new SidebarTree(this);
    m_tree = tree;
    tree->setObjectName("ThothPageTree");
    tree->setColumnCount(1);
    tree->setAccessibleName("Pages and Folders");
    tree->setItemDelegate(new SidebarDelegate(tree));
    tree->setHeaderHidden(true);
    tree->setIndentation(14);
    tree->setAnimated(false);
    tree->setRootIsDecorated(false);
    tree->setIconSize({ 18, 18 });
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setMouseTracking(true);
    tree->viewport()->setMouseTracking(true);
    tree->setDragEnabled(true);
    tree->setAcceptDrops(true);
    tree->setDragDropMode(QAbstractItemView::DragDrop);
    tree->setDefaultDropAction(Qt::MoveAction);
    tree->setDragDropOverwriteMode(false);
    tree->setAutoScroll(true);
    tree->setDropIndicatorShown(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->viewport()->setAcceptDrops(true);
    tree->viewport()->installEventFilter(this);
    favourites->viewport()->setAcceptDrops(true);
    favourites->viewport()->installEventFilter(this);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    body->addWidget(tree, 1);
    tree->on_drop = [this](QString id, QString section, QString parent, QString before) { drop_item(id, section, parent, before); };
    tree->can_merge = [this](QString source, QString target) {
        auto* first = m_model.item(source);
        auto* second = m_model.item(target);
        return first && second && !first->folder && !second->folder
            && first->pages.size() == 1 && second->pages.size() == 1
            && m_model.is_visible(*first) && m_model.is_visible(*second)
            && (first->workspace.isEmpty() || second->workspace.isEmpty() || first->workspace == second->workspace);
    };
    tree->on_merge = [this](QString source, QString target) {
        if (m_model.merge(target, source)) {
            activate(target);
            rebuild();
            save();
        }
    };
    tree->on_drag_state = [this](bool dragging) {
        m_dragging_sidebar_item = dragging;
        if (m_favourites->count() == 0) {
            m_favourites->setVisible(dragging);
            m_favourites->setFixedHeight(dragging ? 60 : 0);
            m_favourites->updateGeometry();
        }
        if (!dragging) {
            m_last_render.clear();
            rebuild();
        }
    };
    connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* node, int) {
        if (node->data(0, ACTION_ROLE).toString() == "newtab") {
            new_page();
            return;
        }
        auto id = node->data(0, ID_ROLE).toString();
        auto* entry = m_model.item(id);
        if (!entry)
            return;
        if (entry->folder) {
            node->setExpanded(!node->isExpanded());
            return;
        }
        auto pane = node->data(0, PAGE_ROLE).toString();
        if (entry->pages.size() == 2) {
            auto rect = m_tree->visualItemRect(node);
            pane = entry->pages[QCursor::pos().x() - m_tree->viewport()->mapToGlobal(rect.topLeft()).x() < rect.width() / 2 ? 0 : 1];
        }
        activate(id, pane);
    });
    connect(tree, &QTreeWidget::itemExpanded, this, [this](auto* node) { if (auto* i = m_model.item(node->data(0, ID_ROLE).toString()); i && i->folder) { i->expanded = true; node->setData(0, EXPANDED_ROLE, true); save(); } });
    connect(tree, &QTreeWidget::itemCollapsed, this, [this](auto* node) { if (auto* i = m_model.item(node->data(0, ID_ROLE).toString()); i && i->folder) { i->expanded = false; node->setData(0, EXPANDED_ROLE, false); save(); } });
    connect(tree, &QWidget::customContextMenuRequested, this, [this](QPoint p) {
        auto* node = m_tree->itemAt(p);
        auto id = node ? node->data(0, ID_ROLE).toString() : QString {};
        auto* entry = m_model.item(id);
        QString pane;
        if (node && entry && entry->pages.size() == 2) {
            auto rect = m_tree->visualItemRect(node);
            pane = entry->pages[p.x() - rect.x() < rect.width() / 2 ? 0 : 1];
        }
        context_menu(id, m_tree->viewport()->mapToGlobal(p), pane);
    });
    auto* bottom = new QHBoxLayout;
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->setSpacing(8);
    auto* dock_scroll = new QScrollArea(this);
    dock_scroll->setObjectName("ThothWorkspaceDock");
    dock_scroll->setWidgetResizable(true);
    dock_scroll->setFrameShape(QFrame::NoFrame);
    dock_scroll->setFixedHeight(40);
    dock_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dock_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* dock = new QWidget(dock_scroll);
    dock->setProperty("LadybirdWindowDragRegion", true);
    m_space_dock = new QHBoxLayout(dock);
    m_space_dock->setContentsMargins(0, 0, 0, 0);
    m_space_dock->setSpacing(3);
    dock_scroll->setWidget(dock);
    dock_scroll->viewport()->installEventFilter(this);
    bottom->addWidget(dock_scroll, 1);
    m_browser_menu_button = new QToolButton(this);
    m_browser_menu_button->setObjectName("ThothBrowserMenu");
    m_browser_menu_button->setIcon(Ladybird::create_chrome_icon(Ladybird::ChromeIcon::Menu, palette()));
    m_browser_menu_button->setIconSize({ 20, 20 });
    m_browser_menu_button->setFixedSize(36, 36);
    m_browser_menu_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_browser_menu_button->setToolTip("Browser, pages, and workspaces");
    m_browser_menu_button->setAccessibleName("Browser and sidebar actions");
    bottom->addWidget(m_browser_menu_button);
    auto* footer_divider = new QFrame(m_body);
    footer_divider->setObjectName("ThothWorkspaceDivider");
    footer_divider->setFrameShape(QFrame::HLine);
    footer_divider->setFixedHeight(1);
    body->addWidget(footer_divider);
    body->addLayout(bottom);
    layout->addWidget(m_body, 1);
    connect(tabs.split_view(), &QSplitter::splitterMoved, this, [this] {
        auto* item = m_model.item(m_model.active);
        auto sizes = m_tabs.split_view()->sizes();
        if (item && item->pages.size() == 2 && sizes.size() == 2 && sizes[0] + sizes[1] > 0) {
            item->ratio = std::clamp(static_cast<double>(sizes[0]) / (sizes[0] + sizes[1]), 0.1, 0.9);
            save();
        }
    });
    tabs.split_view()->installEventFilter(this);
    tabs.installEventFilter(this);
    browser().installEventFilter(this);
    m_loading_animation_timer = new QTimer(this);
    m_loading_animation_timer->setInterval(32);
    connect(m_loading_animation_timer, &QTimer::timeout, this, [this] {
        m_loading_animation_frame = (m_loading_animation_frame + 1) % 24;
        m_tree->setProperty("ThothLoadingFrame", m_loading_animation_frame);
        m_tree->viewport()->update();
    });
    auto* timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, [this] { if (m_ready && !m_closing) refresh(); });
    timer->start();
    if (!m_private) {
        auto& store = registry();
        if (store.live.isEmpty() && store.has_seen_window && !store.restoring)
            store.windows = {};
        store.has_seen_window = true;
        store.live.insert(m_window_id, this);
    }
    install_hover_event_filters(this);
    QTimer::singleShot(0, this, [this] { m_ready = true; rebuild(); save(); });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        QTimer::singleShot(0, this, [this] {
            auto const& appearance = WebView::Application::settings().appearance();
            Thoth::apply_browser_ui_appearance(qstring_from_ak_string(appearance.browser_ui_font.view()), qstring_from_ak_string(appearance.browser_color_palette.view()), qstring_from_ak_string(appearance.browser_icon_weight.view()), qstring_from_ak_string(appearance.color_scheme.view()));
            apply_style();
            m_last_render.clear();
            rebuild();
        });
    });
    apply_style();
    rebuild();
}
Sidebar::~Sidebar()
{
    if (m_host)
        m_host->removeEventFilter(this);
    m_tabs.removeEventFilter(this);
    browser().removeEventFilter(this);
    if (m_reveal_handle) {
        m_reveal_handle->removeEventFilter(this);
        delete m_reveal_handle;
    }
    if (!m_private)
        registry().live.remove(m_window_id);
}
Ladybird::BrowserWindow& Sidebar::browser() const { return *static_cast<Ladybird::BrowserWindow*>(m_tabs.window()); }
void Sidebar::set_browser_menu(QMenu& menu)
{
    m_browser_menu = &menu;
    if (m_browser_menu_button) {
        m_browser_menu_button->setMenu(&menu);
        m_browser_menu_button->setPopupMode(QToolButton::InstantPopup);
    }

    style_browser_menu();

    if (menu.findChild<QMenu*>("ThothBrowserAppearanceMenu"))
        return;

    menu.addSeparator();
    auto* appearance = menu.addMenu("Appearance");
    appearance->setObjectName("ThothBrowserAppearanceMenu");
    auto* choices = new QActionGroup(appearance);
    choices->setExclusive(true);
    for (auto const& [label, scheme] : { QPair { QStringLiteral("Follow System"), QStringLiteral("system") }, QPair { QStringLiteral("Light"), QStringLiteral("light") }, QPair { QStringLiteral("Dark"), QStringLiteral("dark") } }) {
        auto* action = appearance->addAction(label);
        action->setCheckable(true);
        action->setData(scheme);
        choices->addAction(action);
        connect(action, &QAction::triggered, this, [scheme] {
            auto settings = WebView::Application::settings().appearance();
            settings.color_scheme = ak_byte_string_from_qstring(scheme);
            WebView::Application::settings().set_appearance(std::move(settings));
        });
    }
    connect(appearance, &QMenu::aboutToShow, appearance, [choices] {
        auto current = qstring_from_ak_string(WebView::Application::settings().appearance().color_scheme.view());
        for (auto* action : choices->actions())
            action->setChecked(action->data().toString() == current);
    });

    menu.addSeparator();
    auto* organize = menu.addMenu("Organize Pages");
    organize->setObjectName("ThothOrganizeMenu");
    auto* new_folder = organize->addAction("New Folder…");
    connect(new_folder, &QAction::triggered, this, [this] {
        QString title;
        if (!prompt_for_name(this, "New Folder", "Folder name", {}, "Create", title))
            return;
        m_model.add_folder(title.trimmed());
        rebuild();
        save();
    });
    auto* reopen_closed = organize->addAction("Reopen Closed Tab");
    reopen_closed->setEnabled(can_reopen());
    connect(reopen_closed, &QAction::triggered, this, [this] { reopen(); });
    connect(organize, &QMenu::aboutToShow, organize, [this, reopen_closed] { reopen_closed->setEnabled(can_reopen()); });

    auto* workspaces = menu.addMenu("Workspaces");
    workspaces->setObjectName("ThothWorkspaceMenu");
    auto* new_workspace = workspaces->addAction("New Workspace…");
    connect(new_workspace, &QAction::triggered, this, [this] { create_workspace(); });
    Thoth::localize_menu(menu);
}
void Sidebar::style_browser_menu()
{
    if (!m_browser_menu_button || !m_browser_menu)
        return;

    auto dark = Ladybird::ChromeStyle::is_dark(palette());
    auto accent = QColor(browser_palette_accent(qApp->property("ThothColorPalette").toString(), dark));
    if (!accent.isValid())
        accent = Ladybird::ChromeStyle::chrome_accent(palette());
    auto background = dark ? QColor("#211f28") : QColor("#f5f4f8");
    auto hover = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.14 : 0.09);
    auto selected = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.28 : 0.18);
    auto border = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.62 : 0.48);
    m_browser_menu->setPalette(palette());
    m_browser_menu->setStyleSheet(sidebar_menu_style_sheet(background, dark ? QColor("#e9e5ee") : QColor("#35313d"), hover, selected, border, dark ? QColor("#a49bac") : QColor("#777181"), dark ? QColor("#393541") : QColor("#d9d5e2")));
}
QString Sidebar::page_id(Ladybird::Tab* tab) const
{
    for (auto it = m_live.begin(); it != m_live.end(); ++it) {
        if (it.value() == tab)
            return it.key();
    }
    return {};
}
void Sidebar::attach_navigation(Ladybird::Tab* tab)
{
    if (!tab->sidebar_navigation_buttons())
        return;
    tab->sidebar_navigation_buttons()->setProperty("LadybirdWindowDragRegion", true);
    install_hover_event_filters(tab->sidebar_navigation_buttons());
#if !defined(Q_OS_WIN)
    m_navigation_buttons->addWidget(tab->sidebar_navigation_buttons());
    if (m_tabs.tab(m_tabs.current_index()) == tab)
        m_navigation_buttons->setCurrentWidget(tab->sidebar_navigation_buttons());
#endif
}
void Sidebar::page_added(Ladybird::Tab* tab)
{
    auto id = m_next_page_id;
    m_next_page_id.clear();
    if (id.isEmpty())
        id = m_model.add_page();
    m_live[id] = tab;
    QTimer::singleShot(0, this, [this] { refresh(); });
}
void Sidebar::sync_pages()
{
    for (auto it = m_live.begin(); it != m_live.end(); ++it) {
        if (!it.value())
            continue;
        if (auto* p = m_model.page(it.key())) {
            auto url = qstring_from_ak_string(it.value()->view().url().serialize());
            if (!url.isEmpty())
                p->url = url;
            p->title = it.value()->title();
            auto previous_icon = m_icons.value(it.key());
            m_icons[it.key()] = it.value()->tab_icon();
            if (previous_icon.cacheKey() != m_icons[it.key()].cacheKey() && !it.value()->favicon().isNull()) {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                it.value()->favicon().pixmap(32, 32).save(&buffer, "PNG");
                p->favicon = QString::fromLatin1(bytes.toBase64());
            }
        }
    }
}
void Sidebar::page_removed(Ladybird::Tab* tab)
{
#if !defined(Q_OS_WIN)
    if (auto* navigation = tab->sidebar_navigation_buttons()) {
        m_navigation_buttons->removeWidget(navigation);
        navigation->setParent(tab);
        navigation->hide();
    }
#endif

    auto id = page_id(tab);
    if (id.isEmpty())
        return;
    sync_pages();
    m_live.remove(id);
    if (m_mutating)
        return;
    m_removing_page = true;
    auto* entry = m_model.item_for_page(id);
    if (!entry)
        return;
    auto item_id = entry->id;
    auto pending = m_pending_close.take(id);
    auto pane = m_pane_close.take(id);
    if (pane) {
        m_model.close_page(id);
    } else if (!pending.isEmpty()) {
        if (!m_pending_close.values().contains(pending)) {
            if (entry->section == "temporary")
                m_model.close(item_id);
            else
                m_model.active.clear();
        }
    } else if (entry->section == "temporary") {
        m_model.close_page(id);
    }
    rebuild();
    save();
}
QIcon Sidebar::icon(QString const& id) const
{
    auto result = m_icons.value(id);
    static QIcon light, dark;
    auto& fallback = Ladybird::ChromeStyle::is_dark(palette()) ? dark : light;
    if (fallback.isNull())
        fallback = Ladybird::create_chrome_icon(Ladybird::ChromeIcon::Globe, palette());
    return result.isNull() ? fallback : result;
}
QString Sidebar::label(SidebarItem const& item) const
{
    if (item.folder)
        return item.title;
    QStringList titles;
    for (auto const& pid : item.pages) {
        auto* p = const_cast<SidebarModel&>(m_model).page(pid);
        titles.append(Thoth::localized_text(p && !p->title.isEmpty() ? p->title : QStringLiteral("New Tab")));
    }
    return titles.join("  /  ");
}
void Sidebar::rebuild()
{
    auto json = QJsonDocument(m_model.to_json()).toJson(QJsonDocument::Compact);
    QByteArray icons;
    for (auto const& p : m_model.pages) {
        icons += QByteArray::number(icon(p.id).cacheKey());
        auto* tab = m_live.value(p.id).data();
        icons += tab && tab->is_loading() ? '1' : '0';
    }
    if (m_last_render == json + icons)
        return;
    m_last_render = json + icons;
    auto scroll = m_tree->verticalScrollBar()->value();
    QSignalBlocker blocker(m_tree);
    QSignalBlocker favourite_blocker(m_favourites);
    static_cast<SidebarTree*>(m_tree)->reset_drop_target();
    m_tree->clear();
    m_favourites->clear();
    auto* pinned = new QTreeWidgetItem(m_tree);
    pinned->setData(0, SECTION_ROLE, "pinned");
    pinned->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    pinned->setData(0, DROP_SLOT_ROLE, true);
    pinned->setExpanded(true);
    auto* new_tab = new QTreeWidgetItem(m_tree, { "New Tab" });
    new_tab->setData(0, ACTION_ROLE, "newtab");
    new_tab->setData(0, SECTION_ROLE, "temporary");
    new_tab->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
    auto* temporary = new QTreeWidgetItem(m_tree);
    temporary->setData(0, SECTION_ROLE, "temporary");
    temporary->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    temporary->setData(0, DROP_SLOT_ROLE, true);
    temporary->setExpanded(true);
    QHash<QString, QTreeWidgetItem*> nodes;
    for (auto const& i : m_model.items) {
        if (!m_model.is_visible(i))
            continue;
        if (i.section == "favourite") {
            auto* favourite = new QListWidgetItem(icon(i.pages.first()), QString {}, m_favourites);
            favourite->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
            favourite->setData(ID_ROLE, i.id);
            favourite->setToolTip(label(i));
            favourite->setSizeHint({ 68, 54 });
            favourite->setTextAlignment(Qt::AlignCenter);
            if (i.id == m_model.active)
                favourite->setSelected(true);
            continue;
        }
        auto* node = new QTreeWidgetItem;
        node->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        nodes[i.id] = node;
        node->setText(0, label(i));
        node->setToolTip(0, label(i));
        node->setData(0, ID_ROLE, i.id);
        node->setData(0, PAGE_IDS_ROLE, i.pages);
        node->setData(0, SECTION_ROLE, i.section);
            node->setSizeHint(0, { 0, 40 });
        auto font = node->font(0);
        font.setWeight(i.id == m_model.active ? QFont::DemiBold : i.folder ? QFont::Medium : QFont::Normal);
        node->setFont(0, font);
        if (i.folder) {
            node->setData(0, EXPANDED_ROLE, i.expanded);
            node->setData(0, PAGE_ROLE, "folder");
        } else
            node->setIcon(0, icon(i.pages.first()));
        QStringList loading_pages;
        for (auto const& page : i.pages) {
            auto* tab = m_live.value(page).data();
            if (tab && tab->is_loading())
                loading_pages.append(page);
        }
        node->setData(0, LOADING_PAGES_ROLE, loading_pages);
        if (!loading_pages.isEmpty()) {
            auto workspace_id = i.workspace.isEmpty() ? m_model.active_workspace : i.workspace;
            if (auto* workspace = m_model.workspace(workspace_id))
                node->setData(0, LOADING_COLOR_ROLE, QColor(workspace->color));
        }
    }
    for (auto const& i : m_model.items) {
        if (!nodes.contains(i.id))
            continue;
        auto* node = nodes[i.id];
        auto* parent = nodes.value(i.parent, i.section == "temporary" ? temporary : pinned);
        parent->addChild(node);
        node->setExpanded(i.folder ? i.expanded : i.pages.size() == 2);
        if (i.pages.size() == 2) {
            node->setData(0, GROUP_ROLE, i.pages);
            node->setData(0, TITLES_ROLE, QStringList { m_model.page(i.pages[0])->title, m_model.page(i.pages[1])->title });
            node->setData(0, ICONS_ROLE, QVariantList { icon(i.pages[0]), icon(i.pages[1]) });
            node->setData(0, ACTIVE_ROLE, i.active);
            node->setSizeHint(0, { 0, 44 });
        }
        if (i.id == m_model.active)
            node->setSelected(true);
    }
    pinned->setData(0, DROP_SLOT_ROLE, pinned->childCount() == 0);
    temporary->setData(0, DROP_SLOT_ROLE, temporary->childCount() == 0);
    new_tab->setData(0, SEPARATOR_ROLE, pinned->childCount() > 0);
    rebuild_workspaces();
    auto tile_width = std::max(44, (width() - 28) / 3);
    m_favourites->setGridSize({ tile_width, 60 });
    for (int n = 0; n < m_favourites->count(); ++n)
        m_favourites->item(n)->setSizeHint({ tile_width - 4, 56 });
    auto has_favourites = m_favourites->count() > 0;
    m_favourites->setVisible(has_favourites || m_dragging_sidebar_item);
    m_favourites->setFixedHeight(has_favourites ? std::clamp(((m_favourites->count() + 2) / 3) * 60, 60, 180) : m_dragging_sidebar_item ? 60 : 0);
    m_tree->doItemsLayout();
    QAccessibleTableModelChangeEvent accessibility_update(m_tree, QAccessibleTableModelChangeEvent::ModelReset);
    QAccessible::updateAccessibility(&accessibility_update);
    m_tree->verticalScrollBar()->setValue(scroll);
    m_body->setVisible(!m_model.collapsed || m_hover_revealed);
    bool any_visible_page_loading = false;
    for (auto const& item : m_model.items) {
        if (!m_model.is_visible(item) || item.folder)
            continue;
        for (auto const& page : item.pages) {
            auto* tab = m_live.value(page).data();
            if (tab && tab->is_loading()) {
                any_visible_page_loading = true;
                break;
            }
        }
        if (any_visible_page_loading)
            break;
    }
    if (any_visible_page_loading) {
        if (!m_loading_animation_timer->isActive())
            m_loading_animation_timer->start();
    } else {
        m_loading_animation_timer->stop();
    }
}
Ladybird::Tab* Sidebar::ensure_page(QString const& id)
{
    if (auto* tab = m_live.value(id).data())
        return tab;
    auto* p = m_model.page(id);
    if (!p)
        return nullptr;
    auto url = p->url;
    m_next_page_id = id;
    auto& tab = browser().create_new_tab(Web::HTML::ActivateTab::No, Ladybird::BrowserWindow::TabLocation::end());
    auto page_url = url == "about:newtab" ? QStringLiteral("about:blank") : url;
    tab.navigate(ak_url_from_qstring(page_url).value_or(URL::about_blank()));
    return &tab;
}
void Sidebar::activate(QString const& id, QString page)
{
    auto* item = m_model.item(id);
    if (!item || item->folder || m_mutating)
        return;
    m_mutating = true;
    if (item->section != "favourite" && item->workspace != m_model.active_workspace)
        m_model.switch_workspace(item->workspace);
    auto pids = item->pages;
    auto ratio = item->ratio;
    m_pending_close.clear();
    m_pane_close.clear();
    if (!pids.contains(page))
        page = item->active;
    // Creation can reallocate the model, so retain IDs rather than references.
    auto* left = ensure_page(pids.first());
    auto* right = pids.size() == 2 ? ensure_page(pids.last()) : nullptr;
    m_model.active = id;
    m_model.item(id)->active = page;
    if (auto* space = m_model.workspace(m_model.active_workspace))
        space->active = id;
    m_tabs.show_pages(left, right, m_live.value(page), ratio);
    if (auto* active = m_live.value(page).data(); active && active->sidebar_navigation_buttons())
        m_navigation_buttons->setCurrentWidget(active->sidebar_navigation_buttons());
    m_mutating = false;
    rebuild();
    save();
}
void Sidebar::active_changed(Ladybird::Tab* tab)
{
    if (m_mutating || m_removing_page || !tab || !m_pending_close.isEmpty())
        return;
    auto pid = page_id(tab);
    auto* entry = m_model.item_for_page(pid);
    if (!entry)
        return;
    if (entry->id != m_model.active) {
        activate(entry->id, pid);
        return;
    }
    entry->active = pid;
    if (auto* space = m_model.workspace(m_model.active_workspace))
        space->active = entry->id;
    m_tabs.mark_active_page(tab);
    if (tab->sidebar_navigation_buttons())
        m_navigation_buttons->setCurrentWidget(tab->sidebar_navigation_buttons());
    rebuild();
    save();
}
void Sidebar::page_loading_changed(Ladybird::Tab* tab)
{
    if (page_id(tab).isEmpty())
        return;
    m_last_render.clear();
    rebuild();
}
void Sidebar::refresh()
{
    if (m_mutating || m_closing)
        return;
    sync_pages();
    rebuild();
    save();
}

void Sidebar::refresh_language()
{
    m_last_render.clear();
    rebuild();
    rebuild_workspaces();
    if (m_browser_menu)
        Thoth::localize_menu(*m_browser_menu);
}

void Sidebar::choose_split()
{
    auto pid = page_id(m_tabs.tab(m_tabs.current_index()));
    auto* item = m_model.item_for_page(pid);
    if (!item)
        return;
    auto id = item->id;
    if (item->pages.size() == 2) {
        context_menu(id, mapToGlobal(QPoint(width(), 60)));
        return;
    }
    QMenu menu(this);
    auto* blank = menu.addAction("Split with New Page");
    QHash<QAction*, QString> candidates;
    menu.addSeparator();
    for (auto const& candidate : m_model.items) {
        if (!candidate.folder && m_model.is_visible(candidate) && candidate.id != id && candidate.pages.size() == 1)
            candidates[menu.addAction(icon(candidate.pages.first()), label(candidate))] = candidate.id;
    }
    Thoth::localize_menu(menu);
    auto* selected = menu.exec(mapToGlobal(QPoint(width(), 60)));
    QString second;
    if (selected == blank) {
        auto& tab = browser().new_tab_from_url(URL::about_blank(), Web::HTML::ActivateTab::No, Ladybird::BrowserWindow::TabLocation::end());
        if (auto* added = m_model.item_for_page(page_id(&tab)))
            second = added->id;
    } else
        second = candidates.value(selected);
    if (m_model.merge(id, second))
        activate(id);
}
void Sidebar::close_item(QString const& id, QString pane)
{
    sync_pages();
    auto* item = m_model.item(id);
    if (!item || item->folder)
        return;
    auto pages = pane.isEmpty() ? item->pages : QStringList { pane };
    bool any_live = false;
    for (auto const& pid : pages) {
        if (!m_live.value(pid))
            continue;
        any_live = true;
        m_pending_close[pid] = id;
        m_pane_close[pid] = !pane.isEmpty();
    }
    if (!any_live) {
        if (item->section == "temporary")
            m_model.close(id);
        rebuild();
        save();
        return;
    }
    for (auto const& pid : pages) {
        if (auto* tab = m_live.value(pid).data())
            tab->request_close();
    }
    // A cancelled beforeunload leaves the remaining group intact; it can reactivate an unloaded pane.
    if (m_tabs.count() == 0 && keeps_window_open()) {
        auto active = m_model.active;
        auto* candidate = m_model.item(active);
        if (!candidate || candidate->pages.isEmpty() || !m_live.value(candidate->active)) {
            auto& tab = browser().new_tab_from_url(URL::about_blank(), Web::HTML::ActivateTab::Yes, Ladybird::BrowserWindow::TabLocation::end());
            active_changed(&tab);
        }
    }
}
void Sidebar::ensure_nonempty()
{
    if (m_mutating || m_closing)
        return;
    for (auto const& entry : m_model.items) {
        if (!entry.folder && m_model.is_visible(entry)) {
            for (auto const& pid : entry.pages) {
                if (m_live.value(pid))
                    return;
            }
        }
    }
    browser().new_tab_from_url(URL::about_blank(), Web::HTML::ActivateTab::Yes, Ladybird::BrowserWindow::TabLocation::end());
}
void Sidebar::restore_active() { activate(m_model.active); }
void Sidebar::close_current()
{
    auto* entry = m_model.item_for_page(page_id(m_tabs.tab(m_tabs.current_index())));
    if (entry)
        close_item(entry->id);
}
bool Sidebar::reopen()
{
    auto id = m_model.reopen();
    if (id.isEmpty())
        return false;
    activate(id);
    return true;
}
void Sidebar::cycle(int direction)
{
    QStringList ids;
    for (auto const& i : m_model.items) {
        if (!i.folder && m_model.is_visible(i))
            ids.append(i.id);
    }
    if (ids.isEmpty())
        return;
    auto index = ids.indexOf(m_model.active);
    activate(ids[(index + direction + ids.size()) % ids.size()]);
}
void Sidebar::drop_item(QString const& id, QString section, QString parent, QString before)
{
    if (m_model.move(id, section, parent, before)) {
        rebuild();
        save();
    }
}
void Sidebar::context_menu(QString const& id, QPoint point, QString pane)
{
    QMenu menu(this);
    auto* entry = m_model.item(id);
    if (entry && !entry->folder) {
        auto* open = menu.addAction("Open");
        connect(open, &QAction::triggered, this, [this, id, pane] { activate(id, pane); });
        if (entry->pages.size() == 1) {
            auto* favourite = menu.addAction("Move to Favourites");
            connect(favourite, &QAction::triggered, this, [this, id] { drop_item(id, "favourite", {}); });
        }
        auto* pinned = menu.addAction("Pin Page");
        connect(pinned, &QAction::triggered, this, [this, id] { drop_item(id, "pinned", {}); });
        auto* temporary = menu.addAction("Unpin to Temporary");
        connect(temporary, &QAction::triggered, this, [this, id] { drop_item(id, "temporary", {}); });
        auto* folders = menu.addMenu("Move to Folder");
        for (auto const& folder : m_model.items) {
            if (folder.folder && folder.workspace == m_model.active_workspace) {
                auto* action = folders->addAction(folder.title);
                connect(action, &QAction::triggered, this, [this, id, parent = folder.id] { drop_item(id, "pinned", parent); });
            }
        }
        if (entry->pages.size() == 2) {
            auto* swap = menu.addAction("Swap Left and Right");
            connect(swap, &QAction::triggered, this, [this, id] { auto* i = m_model.item(id); if (i) { i->pages.swapItemsAt(0, 1); i->ratio = 1.0 - i->ratio; activate(id); } });
            auto* ungroup = menu.addAction("Separate Pages");
            connect(ungroup, &QAction::triggered, this, [this, id] { if (m_model.ungroup(id)) activate(id); });
            auto* close_panes = menu.addMenu("Close Pane");
            for (int n = 0; n < 2; ++n) {
                auto* action = close_panes->addAction(n == 0 ? "Close Left Pane" : "Close Right Pane");
                connect(action, &QAction::triggered, this, [this, id, pid = entry->pages[n]] { close_item(id, pid); });
            }
            if (entry->pages.contains(pane)) {
                auto* close_pane = menu.addAction("Close This Pane");
                connect(close_pane, &QAction::triggered, this, [this, id, pane] { close_item(id, pane); });
            }
        } else {
            auto* split = menu.addAction("Create Split View…");
            connect(split, &QAction::triggered, this, [this, id] { activate(id); choose_split(); });
        }
        menu.addSeparator();
        auto* close = menu.addAction(entry->section == "temporary" ? "Close" : "Unload Page (Keep Pinned)");
        connect(close, &QAction::triggered, this, [this, id] { close_item(id); });
    }
    if (entry && entry->folder) {
        auto* rename = menu.addAction("Rename Folder…");
        connect(rename, &QAction::triggered, this, [this, id] {
            auto* i = m_model.item(id);
            if (!i)
                return;
            QString title;
            if (prompt_for_name(this, "Rename Folder", "Folder name", i->title, "Save", title)) {
                m_model.item(id)->title = title.trimmed();
                rebuild();
                save();
            }
        });
        auto* remove = menu.addAction("Remove Folder, Keep Pages");
        connect(remove, &QAction::triggered, this, [this, id] {
            auto* folder = m_model.item(id);
            if (!folder)
                return;
            auto parent = folder->parent;
            QStringList children;
            for (auto const& i : m_model.items) {
                if (i.parent == id)
                    children.append(i.id);
            }
            for (auto const& child : children)
                m_model.move(child, "pinned", parent, id);
            m_model.close(id, false);
            rebuild();
            save();
        });
    }
    if (entry) {
        auto* move_space = menu.addMenu("Move to Workspace");
        for (auto const& space : m_model.workspaces) {
            if (space.id == entry->workspace)
                continue;
            auto* action = move_space->addAction(workspace_icon(space, Ladybird::ChromeStyle::is_dark(palette())), space.name);
            connect(action, &QAction::triggered, this, [this, id, destination = space.id] { if (m_model.move_to_workspace(id,destination)) { switch_workspace(m_model.active_workspace); rebuild(); save(); } });
        }
    }
    menu.addSeparator();
    auto* add_folder = menu.addAction("New Folder…");
    connect(add_folder, &QAction::triggered, this, [this, id] {
        QString title;
        if (prompt_for_name(this, "New Folder", "Folder name", {}, "Create", title)) {
            auto* i = m_model.item(id);
            m_model.add_folder(title.trimmed(), i && i->folder ? id : QString {});
            rebuild();
            save();
        }
    });
    auto* undo = menu.addAction("Reopen Closed Tab");
    undo->setEnabled(can_reopen());
    connect(undo, &QAction::triggered, this, [this] { reopen(); });
    Thoth::localize_menu(menu);
    menu.exec(point);
}
void Sidebar::set_outer_splitter(QSplitter* splitter)
{
    m_outer = splitter;
    m_host = splitter->parentWidget();
    // The sidebar computes its palette before the outer splitter is created.
    // Copy the already-resolved color now so the divider does not fall back to
    // the application window color on first paint.
    splitter->setProperty("ThothSidebarBackground", property("ThothSidebarBackground"));
    if (auto* handle = splitter->handle(1))
        handle->update();
    // Set the direction again after the splitter is parented and populated;
    // Qt may otherwise re-inherit the window's direction during re-layout.
    splitter->setLayoutDirection(Qt::LeftToRight);
    setLayoutDirection(Qt::LeftToRight);
    splitter->setCollapsible(0, true);
    splitter->setCollapsible(1, false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ m_model.width, 1100 });
    m_collapse_animation = new QVariantAnimation(this);
    m_collapse_animation->setDuration(260);
    m_collapse_animation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_collapse_animation, &QVariantAnimation::valueChanged, this, [this](QVariant const& value) {
        if (!m_outer)
            return;
        auto sidebar_width = value.toInt();
        auto content_width = std::max(0, m_outer->width() - sidebar_width - m_outer->handleWidth());
        m_outer->setSizes({ sidebar_width, content_width });
    });
    connect(m_collapse_animation, &QVariantAnimation::finished, this, [this] {
        if (!m_outer || !m_host)
            return;
        if (!m_model.collapsed || (m_hover_revealed && m_chrome_visible)) {
            setMinimumWidth(SIDEBAR_MIN_EXPANDED_WIDTH);
            setMaximumWidth(SIDEBAR_MAX_EXPANDED_WIDTH);
            m_body->show();
            setVisible(m_chrome_visible);
            update_compact_header();
            if (m_model.collapsed) {
                m_hover_outside_ms = 0;
                m_hover_timer->start();
            }
            return;
        }
        update_compact_header();
        m_body->hide();
        setMinimumWidth(0);
        setMaximumWidth(0);
        hide();
        m_outer->setSizes({ 0, m_outer->width() });
        update_collapsed_overlay_geometry();
        if (m_reveal_handle)
            m_reveal_handle->setVisible(m_chrome_visible);
        if (m_reveal_handle && m_chrome_visible)
            m_reveal_handle->raise();
    });
    connect(splitter, &QSplitter::splitterMoved, this, [this] {
        if (!m_model.collapsed && (!m_collapse_animation || m_collapse_animation->state() != QAbstractAnimation::Running)) {
            if (QGuiApplication::mouseButtons().testFlag(Qt::LeftButton) && width() <= SIDEBAR_MIN_EXPANDED_WIDTH) {
                m_model.width = SIDEBAR_MIN_EXPANDED_WIDTH;
                m_model.collapsed = true;
                m_ignore_hover_until_leave = true;
                m_last_render.clear();
                apply_collapsed_presentation(false, true);
                save();
                return;
            }
            m_model.width = std::clamp(width(), SIDEBAR_MIN_EXPANDED_WIDTH, SIDEBAR_MAX_EXPANDED_WIDTH);
            m_last_render.clear();
            rebuild();
            save();
        }
    });
    if (m_host) {
        m_host->installEventFilter(this);
        // Keep the hover target outside QSplitter. Direct children of a
        // splitter are interpreted as panes, which can create a phantom
        // right-side pane after collapsing and reopening the sidebar.
        m_reveal_handle = new SidebarRevealHandle(m_host);
        m_reveal_handle->setObjectName("ThothSidebarReveal");
#if defined(Q_OS_MACOS)
        m_reveal_handle->setAttribute(Qt::WA_NativeWindow);
        m_reveal_handle->setAttribute(Qt::WA_TranslucentBackground);
#endif
        m_reveal_handle->setAttribute(Qt::WA_Hover);
        m_reveal_handle->setAccessibleName("Show Sidebar");
        m_reveal_handle->setToolTip("Show Sidebar · ⌘⇧L");
        m_reveal_handle->setFixedWidth(SIDEBAR_REVEAL_WIDTH);
        m_reveal_handle->setMouseTracking(true);
        m_reveal_handle->setCursor(Qt::PointingHandCursor);
        auto* reveal_handle = static_cast<SidebarRevealHandle*>(m_reveal_handle.data());
        reveal_handle->on_click = [this] {
            if (!m_model.collapsed)
                return;
            m_model.collapsed = false;
            m_ignore_hover_until_leave = false;
            apply_collapsed_presentation(false, true);
            save();
        };
        connect(reveal_handle, &QToolButton::clicked, this, [reveal_handle] {
            if (reveal_handle->on_click)
                reveal_handle->on_click();
        });
        m_reveal_handle->installEventFilter(this);
        m_reveal_handle->hide();
    }
    m_hover_timer = new QTimer(this);
    m_hover_timer->setInterval(80);
    connect(m_hover_timer, &QTimer::timeout, this, [this] {
        if (!m_model.collapsed || !m_chrome_visible)
            return;
        auto cursor = QCursor::pos();
        auto outer_origin = m_outer->mapToGlobal(QPoint(0, 0));
        auto reveal_rect = QRect(outer_origin, QSize(SIDEBAR_REVEAL_WIDTH, m_outer->height()));
        auto sidebar_rect = QRect(outer_origin, QSize(std::min(m_model.width, m_outer->width()), m_outer->height()));
        if (!m_hover_revealed) {
            auto cursor_in_reveal_area = m_reveal_handle && m_reveal_handle->isVisible() && reveal_rect.contains(cursor);
            if (m_ignore_hover_until_leave) {
                if (!cursor_in_reveal_area)
                    m_ignore_hover_until_leave = false;
                m_hover_outside_ms = 0;
                return;
            }
            if (!cursor_in_reveal_area) {
                m_hover_outside_ms = 0;
                return;
            }
            m_hover_outside_ms += m_hover_timer->interval();
            if (m_hover_outside_ms >= SIDEBAR_REVEAL_DELAY_MS)
                reveal_collapsed_sidebar();
            return;
        }
        if (sidebar_rect.contains(cursor)) {
            m_hover_outside_ms = 0;
            return;
        }
        m_hover_outside_ms += m_hover_timer->interval();
        if (m_hover_outside_ms >= SIDEBAR_REVEAL_DELAY_MS)
            hide_collapsed_sidebar();
    });
    apply_style();
}
void Sidebar::update_compact_header()
{
    auto* header = layout()->itemAt(0)->widget()->layout();
    auto compact = m_model.collapsed && !m_hover_revealed;
    header->setContentsMargins(0, compact ? 34 : 0, 0, 0);
#if defined(Q_OS_MACOS)
    header->itemAt(0)->spacerItem()->changeSize(compact ? 0 : 66, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
#endif
    m_navigation_buttons->setVisible(!compact);
    header->invalidate();
}
void Sidebar::update_collapsed_overlay_geometry()
{
    if (!m_outer || !m_host)
        return;
    if (m_reveal_handle) {
        auto origin = m_outer->mapTo(m_host, QPoint {});
        m_reveal_handle->setGeometry(QRect(origin, QSize(SIDEBAR_REVEAL_WIDTH, m_outer->height())));
    }
}
void Sidebar::apply_collapsed_presentation(bool reveal, bool animate)
{
    if (!m_outer || !m_host)
        return;
    m_outer->setLayoutDirection(Qt::LeftToRight);
    if (m_collapse_animation)
        m_collapse_animation->stop();
    auto sizes = m_outer->sizes();
    auto current_width = sizes.isEmpty() ? (isVisible() ? width() : 0) : sizes.first();
    if (!m_model.collapsed) {
        if (m_hover_timer)
            m_hover_timer->stop();
        m_ignore_hover_until_leave = false;
        m_hover_revealed = false;
        if (m_reveal_handle)
            m_reveal_handle->hide();
        if (m_outer->indexOf(this) < 0) {
            hide();
            m_outer->insertWidget(0, this);
        }
        setMinimumWidth(0);
        setMaximumWidth(SIDEBAR_MAX_EXPANDED_WIDTH);
        update_compact_header();
        m_body->show();
        setVisible(m_chrome_visible);
        auto sidebar_width = std::clamp(m_model.width, SIDEBAR_MIN_EXPANDED_WIDTH, SIDEBAR_MAX_EXPANDED_WIDTH);
        if (animate && m_chrome_visible && current_width < sidebar_width && m_collapse_animation) {
            m_outer->setSizes({ current_width, std::max(0, m_outer->width() - current_width - m_outer->handleWidth()) });
            m_collapse_animation->setStartValue(current_width);
            m_collapse_animation->setEndValue(sidebar_width);
            m_collapse_animation->start();
            return;
        }
        setMinimumWidth(SIDEBAR_MIN_EXPANDED_WIDTH);
        m_outer->setSizes({ m_model.width, std::max(200, m_outer->width() - m_model.width - m_outer->handleWidth()) });
        return;
    }

    m_hover_revealed = reveal;
    update_collapsed_overlay_geometry();
    if (reveal && m_chrome_visible) {
        // Keep the sidebar inside QSplitter while collapsed. Reparenting a
        // native macOS widget out of and back into QSplitter can leave its
        // native surface at the old pane position and make hover detection
        // oscillate between the stale and current geometries.
        setMinimumWidth(0);
        setMaximumWidth(SIDEBAR_MAX_EXPANDED_WIDTH);
        show();
        m_body->show();
        update_compact_header();
        if (m_reveal_handle)
            m_reveal_handle->hide();
        auto available_sidebar_width = std::max(SIDEBAR_MIN_EXPANDED_WIDTH, m_outer->width() - m_outer->handleWidth() - 200);
        auto sidebar_width = std::min(m_model.width, available_sidebar_width);
        if (animate && current_width < sidebar_width && m_collapse_animation) {
            m_collapse_animation->setStartValue(current_width);
            m_collapse_animation->setEndValue(sidebar_width);
            m_hover_outside_ms = 0;
            m_hover_timer->start();
            m_collapse_animation->start();
            return;
        }
        setMinimumWidth(SIDEBAR_MIN_EXPANDED_WIDTH);
        m_outer->setSizes({ sidebar_width, std::max(200, m_outer->width() - sidebar_width - m_outer->handleWidth()) });
        m_hover_outside_ms = 0;
        m_hover_timer->start();
    } else {
        if (animate && m_chrome_visible && isVisible() && current_width > 0 && m_collapse_animation) {
            setMinimumWidth(0);
            setMaximumWidth(SIDEBAR_MAX_EXPANDED_WIDTH);
            m_body->show();
            if (m_reveal_handle)
                m_reveal_handle->hide();
            m_collapse_animation->setStartValue(current_width);
            m_collapse_animation->setEndValue(0);
            m_collapse_animation->start();
            return;
        }
        update_compact_header();
        m_body->hide();
        setMinimumWidth(0);
        setMaximumWidth(0);
        hide();
        m_outer->setSizes({ 0, m_outer->width() });
        if (m_reveal_handle)
            m_reveal_handle->setVisible(m_chrome_visible);
        if (m_reveal_handle && m_chrome_visible)
            m_reveal_handle->raise();
        m_hover_outside_ms = 0;
        if (m_chrome_visible)
            m_hover_timer->start();
        else
            m_hover_timer->stop();
    }
}
void Sidebar::reveal_collapsed_sidebar()
{
    if (!m_model.collapsed || !m_chrome_visible || m_hover_revealed || m_ignore_hover_until_leave)
        return;
    apply_collapsed_presentation(true, true);
}
void Sidebar::hide_collapsed_sidebar()
{
    if (!m_model.collapsed || !m_hover_revealed)
        return;
    apply_collapsed_presentation(false, true);
}
void Sidebar::toggle_collapsed()
{
    m_model.collapsed = !m_model.collapsed;
    m_ignore_hover_until_leave = m_model.collapsed;
    apply_collapsed_presentation(false, true);
    save();
}
void Sidebar::expand()
{
    if (m_model.collapsed)
        toggle_collapsed();
}
void Sidebar::set_chrome_visible(bool visible)
{
    m_chrome_visible = visible;
    if (m_model.collapsed)
        apply_collapsed_presentation(false);
    else
        setVisible(visible);
}
void Sidebar::refresh_appearance()
{
    auto sidebar_font = QApplication::font();
    sidebar_font.setPointSizeF(12);
    sidebar_font.setWeight(QFont::Normal);
    setFont(sidebar_font);
    apply_style();
    m_last_render.clear();
    rebuild();
}
void Sidebar::apply_style()
{
    if (m_styling)
        return;
    m_styling = true;
    bool dark = Ladybird::ChromeStyle::is_dark(palette());
    auto* space = m_model.workspace(m_model.active_workspace);
    auto tint = QColor(space ? space->color : "#b9a5df");
    if (!tint.isValid())
        tint = QColor("#b9a5df");
    auto accent = QColor(browser_palette_accent(qApp->property("ThothColorPalette").toString(), dark));
    if (!accent.isValid())
        accent = Ladybird::ChromeStyle::chrome_accent(palette());
    auto background = Ladybird::ChromeStyle::mix(dark ? QColor("#211f28") : QColor("#f5f4f8"), tint, dark ? 0.16 : 0.24);
    setProperty("ThothSidebarBackground", background.name());
    if (m_reveal_handle) {
        m_reveal_handle->setProperty("ThothSidebarBackground", background.name());
        m_reveal_handle->update();
    }
    if (m_outer) {
        m_outer->setProperty("ThothSidebarBackground", background.name());
        if (auto* handle = m_outer->handle(1))
            handle->update();
    }
    auto hover = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.14 : 0.09);
    auto selected = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.28 : 0.18);
    auto selection_border = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.62 : 0.48);
    auto pressed = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.48 : 0.34);
    auto pressed_border = Ladybird::ChromeStyle::mix(background, accent, dark ? 0.78 : 0.64);
    QString bg = background.name(), fg = dark ? "#e9e5ee" : "#35313d";
    setStyleSheet(QString(
        "QWidget#ThothSidebar { background: %1; color: %2; border-right: none; }"
        "QWidget#ThothSidebar QWidget { color: %2; }"
        "QTreeWidget, QListWidget { background: transparent; border: none; outline: none; }"
        "QTreeView::branch, QTreeView::branch:selected { background: %1; image: none; border: none; }"
        "QTreeWidget::item { border: 1px solid transparent; border-radius: 9px; padding: 4px 5px; }"
        "QTreeWidget::item:hover, QListWidget::item:hover { background: %3; border-radius: 9px; }"
        "QTreeWidget::item:selected, QListWidget::item:selected { background: %4; border: 1px solid %5; border-radius: 9px; }"
        "QListWidget::item { background: transparent; border: 1px solid transparent; border-radius: 10px; margin: 3px; }"
        "QToolButton { border: none; border-radius: 8px; padding: 4px; background: transparent; }"
        "QToolButton:hover { background: %3; }"
        "QToolButton:pressed { background: %8; border: 1px solid %9; }"
        "QToolButton#ThothBrowserMenu { min-width: 36px; min-height: 36px; padding: 0; border-radius: 10px; }"
        "QToolButton#ThothBrowserMenu::menu-indicator { image: none; width: 0; }"
        "QToolButton[dropTarget=\"true\"] { background: %4; border: 1px solid %5; border-radius: 8px; }"
        "QToolButton#ThothNewTab { text-align: left; color: %6; padding-left: 5px; }"
        "QToolButton#ThothSpaceHeader { text-align: left; font-weight: 600; color: %6; padding-left: 8px; border-radius: 9px; }"
        "QToolButton[workspaceActive=\"true\"] { background: %3; border: 1px solid %5; border-radius: 8px; }"
        "QToolButton[checked=\"true\"] { background: %4; }"
        "QToolButton[workspaceActive=\"true\"]:pressed, QToolButton[checked=\"true\"]:pressed { background: %8; border: 1px solid %9; }"
        "QScrollArea#ThothWorkspaceDock, QScrollArea#ThothWorkspaceDock QWidget { background: transparent; border: none; }"
        "QWidget#LadybirdToolbarContainer, QWidget#LadybirdNavigationToolbar { background: transparent; border: none; }"
        "QFrame#ThothWorkspaceDivider { background: %7; border: none; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { background: %4; border-radius: 2px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(bg, fg, hover.name(), selected.name(), selection_border.name(), dark ? "#a49bac" : "#777181", dark ? "#393541" : "#d9d5e2", pressed.name(), pressed_border.name()));
    if (m_browser_menu_button)
        m_browser_menu_button->setIcon(Ladybird::create_chrome_icon(Ladybird::ChromeIcon::Menu, palette()));
    style_browser_menu();
    if (m_outer) {
        auto frame_style = QString("QSplitter { background: transparent; border: none; } QSplitter::handle { background: transparent; border: none; }");
        if (m_outer->styleSheet() != frame_style)
            m_outer->setStyleSheet(frame_style);
    }
    m_styling = false;
    m_workspace_render.clear();
    if (m_space_header)
        rebuild_workspaces();
}
void Sidebar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update_new_tab_prompt_geometry();
    if (m_tree && m_favourites)
        QTimer::singleShot(0, this, [this] { m_last_render.clear(); rebuild(); });
}
void Sidebar::update_new_tab_prompt_geometry()
{
    if (!m_new_tab_prompt)
        return;
    auto* content = m_tabs.split_view();
    // The prompt is a child of TabWidget, so keep all geometry in the same
    // coordinate system. This avoids a detached native window retaining its
    // previous screen position during a browser move.
    auto content_rect = QRect(content->mapTo(&m_tabs, QPoint {}), content->size());
    auto width = std::clamp(content_rect.width() - 48, 260, 540);
    auto height = 156;
    m_new_tab_prompt->setGeometry(QRect(
        content_rect.center().x() - width / 2,
        content_rect.center().y() - height / 2,
        width,
        height));
    if (m_new_tab_prompt->isVisible())
        m_new_tab_prompt->raise();
}
bool Sidebar::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutDirectionChange) {
        setLayoutDirection(Qt::LeftToRight);
        if (m_outer)
            m_outer->setLayoutDirection(Qt::LeftToRight);
    }
    if (event->type() == QEvent::PaletteChange)
        apply_style();
    if (m_model.collapsed && m_hover_revealed && event->type() == QEvent::Enter)
        m_hover_outside_ms = 0;
    return QWidget::event(event);
}
bool Sidebar::eventFilter(QObject* object, QEvent* event)
{
    if ((object == m_tabs.split_view() || object == &m_tabs || object == &browser())
        && (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::WindowStateChange)) {
        update_new_tab_prompt_geometry();
        QTimer::singleShot(0, this, [this] {
            if (m_new_tab_prompt)
                update_new_tab_prompt_geometry();
        });
    }

    auto is_left_press = event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton;
    if (is_left_press && object == m_tree->viewport()) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        auto position = mouse->position().toPoint();
        auto* item = m_tree->itemAt(position);
        auto id = item ? item->data(0, ID_ROLE).toString() : QString {};
        if (!id.isEmpty() && item->data(0, PAGE_ROLE).toString() != "folder") {
            auto row = m_tree->visualItemRect(item);
            if (position.x() >= row.right() - 34 && position.x() <= row.right() && position.y() >= row.top() && position.y() <= row.bottom()) {
                close_item(id);
                mouse->accept();
                return true;
            }
        }
        auto has_action = item && !item->data(0, ACTION_ROLE).toString().isEmpty();
        auto has_item = !id.isEmpty();
        if (!has_action && !has_item && browser().start_window_move()) {
            mouse->accept();
            return true;
        }
    }
    if (is_left_press && object == m_favourites->viewport()) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (!m_favourites->itemAt(mouse->position().toPoint()) && browser().start_window_move()) {
            mouse->accept();
            return true;
        }
    }
    auto is_drag_event = event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::DragLeave || event->type() == QEvent::Drop;
    if (is_drag_event && object == m_tree->viewport())
        return static_cast<SidebarTree*>(m_tree)->handle_viewport_drag_event(event);
    if (is_drag_event && object == m_favourites->viewport())
        return static_cast<FavouriteList*>(m_favourites)->handle_viewport_drag_event(event);

    if (object == m_reveal_handle) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter || event->type() == QEvent::MouseMove) {
            m_reveal_handle->update();
        } else if (event->type() == QEvent::Leave) {
            m_reveal_handle->update();
        }
    }
    if (m_model.collapsed && m_hover_revealed && object != m_reveal_handle && event->type() == QEvent::Enter)
        m_hover_outside_ms = 0;
    if ((object == m_host || object == m_outer) && event->type() == QEvent::Resize && m_model.collapsed) {
        update_collapsed_overlay_geometry();
        if (m_hover_revealed && m_outer) {
            raise();
        } else if (m_reveal_handle && m_chrome_visible) {
            m_reveal_handle->raise();
        }
    }

    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        auto delta = wheel->pixelDelta().isNull() ? wheel->angleDelta() : wheel->pixelDelta();
        if (std::abs(delta.x()) > std::abs(delta.y()) * 2 && delta.x() != 0) {
            if (!m_workspace_swipe_lock) {
                m_workspace_swipe += delta.x();
                if (std::abs(m_workspace_swipe) > 90) {
                    cycle_workspace(m_workspace_swipe < 0 ? 1 : -1);
                    m_workspace_swipe = 0;
                    m_workspace_swipe_lock = true;
                    QTimer::singleShot(600, this, [this] { m_workspace_swipe_lock = false; });
                }
            }
            wheel->accept();
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}
void Sidebar::install_hover_event_filters(QWidget* root)
{
    if (!root || root->property("ThothHoverEventFilterInstalled").toBool())
        return;
    root->setProperty("ThothHoverEventFilterInstalled", true);
    root->installEventFilter(this);
    for (auto* child : root->findChildren<QWidget*>()) {
        if (child->property("ThothHoverEventFilterInstalled").toBool())
            continue;
        child->setProperty("ThothHoverEventFilterInstalled", true);
        child->installEventFilter(this);
    }
}
void Sidebar::new_page()
{
    if (m_new_tab_prompt) {
        update_new_tab_prompt_geometry();
        m_new_tab_prompt->raise();
        auto* overlay = static_cast<NewTabPromptOverlay*>(m_new_tab_prompt.data());
        overlay->show();
        if (overlay->address) {
            overlay->address->clear();
            overlay->address->setFocus(Qt::OtherFocusReason);
            overlay->address->selectAll();
        }
        return;
    }

    auto dark = Ladybird::ChromeStyle::is_dark(palette());
    auto accent = QColor(browser_palette_accent(qApp->property("ThothColorPalette").toString(), dark));
    if (!accent.isValid())
        accent = Ladybird::ChromeStyle::chrome_accent(palette());
    auto card_color = dark ? QColor("#24212c") : QColor("#fffefe");
    auto outline = Ladybird::ChromeStyle::mix(card_color, accent, dark ? 0.36 : 0.24);
    auto ink = dark ? QColor("#f2eef7") : QColor("#35313c");
    auto muted = dark ? QColor("#aaa3b3") : QColor("#77717f");
    auto input_color = dark ? QColor("#302c39") : QColor("#f7f5fa");

    // Keep the prompt outside the splitter, but make it a child of TabWidget.
    // A separate Qt::Tool window can leave an old native surface behind while
    // the browser window is being moved. A native child follows its parent
    // automatically and cannot create a second floating prompt.
    auto* overlay = new NewTabPromptOverlay(&m_tabs);
    overlay->setObjectName("ThothNewTabPromptOverlay");
    overlay->setAttribute(Qt::WA_StyledBackground, true);
    overlay->setStyleSheet("QFrame#ThothNewTabPromptOverlay { background: transparent; border: none; }");
#if defined(Q_OS_MACOS)
    // WebContentView is backed by a native QRhiWidget. Keep the prompt in its
    // parent's native child stack so it remains above the page surface.
    overlay->setAttribute(Qt::WA_NativeWindow);
    overlay->setAttribute(Qt::WA_TranslucentBackground);
#endif

    auto* outer = new QVBoxLayout(overlay);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto* card = new QFrame(overlay);
    card->setObjectName("ThothNewTabPromptCard");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setFixedHeight(156);
    card->setStyleSheet(QString(
        "QFrame#ThothNewTabPromptCard { background: %1; border: 1px solid %2; border-radius: 18px; }"
        "QLabel#ThothNewTabHeading { color: %3; font-size: 11px; font-weight: 700; letter-spacing: 1.1px; background: transparent; border: none; }"
        "QLabel#ThothNewTabEscape { color: %4; font-size: 11px; background: transparent; border: none; padding: 3px 7px; border: 1px solid %2; border-radius: 6px; }"
        "QLabel#ThothNewTabGlyph { color: %5; font-size: 21px; background: transparent; border: none; }"
        "QLineEdit#ThothNewTabAddress { color: %3; background: %6; border: 1px solid %2; border-radius: 11px; padding: 0 14px; selection-background-color: %5; font-size: 15px; }"
        "QLineEdit#ThothNewTabAddress:focus { border: 1px solid %5; }"
        "QLabel#ThothNewTabHint { color: %4; background: transparent; border: none; font-size: 11px; }")
            .arg(card_color.name(), outline.name(), ink.name(), muted.name(), accent.name(), input_color.name()));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(18, 16, 18, 14);
    card_layout->setSpacing(10);
    auto* heading_row = new QHBoxLayout;
    heading_row->setContentsMargins(2, 0, 2, 0);
    auto* heading = new QLabel("NEW PAGE", card);
    heading->setObjectName("ThothNewTabHeading");
    heading_row->addWidget(heading);
    heading_row->addStretch(1);
    auto* escape_hint = new QLabel("esc", card);
    escape_hint->setObjectName("ThothNewTabEscape");
    heading_row->addWidget(escape_hint);
    card_layout->addLayout(heading_row);

    auto* input_row = new QHBoxLayout;
    input_row->setContentsMargins(10, 0, 10, 0);
    input_row->setSpacing(8);
    auto* search_glyph = new QLabel("⌕", card);
    search_glyph->setObjectName("ThothNewTabGlyph");
    input_row->addWidget(search_glyph);
    auto* address = new NewTabAddressEdit(card);
    address->setObjectName("ThothNewTabAddress");
    address->setPlaceholderText("Search or enter a web address");
    address->setClearButtonEnabled(true);
    address->setFixedHeight(48);
    input_row->addWidget(address, 1);
    card_layout->addLayout(input_row);
    auto* hint = new QLabel("Press Enter to open · Esc to dismiss", card);
    hint->setObjectName("ThothNewTabHint");
    auto* hint_row = new QHBoxLayout;
    hint_row->setContentsMargins(3, 0, 2, 0);
    hint_row->addStretch(1);
    hint_row->addWidget(hint);
    card_layout->addLayout(hint_row);
    outer->addWidget(card);
    overlay->setFixedSize(540, 156);
    overlay->card = card;
    overlay->address = address;

    auto dismiss = [overlay] {
        overlay->hide();
    };
    overlay->on_dismiss = dismiss;
    address->on_escape = dismiss;
    connect(address, &QLineEdit::returnPressed, overlay, [this, address, dismiss] {
        auto value = address->text().trimmed();
        if (value.isEmpty()) {
            address->setFocus();
            return;
        }
        dismiss();
        // Creating the tab is the commit action. Opening and dismissing the
        // prompt must not mutate the page tree or add an about:blank tab.
        m_tabs.clear_split_presentation();
        auto& tab = browser().new_tab_from_url(URL::about_blank(), Web::HTML::ActivateTab::Yes, Ladybird::BrowserWindow::TabLocation::end());
        tab.navigate_from_user_input(value);
    });

    m_new_tab_prompt = overlay;
    update_new_tab_prompt_geometry();
    overlay->show();
    overlay->raise();
    address->setFocus(Qt::OtherFocusReason);
}
void Sidebar::switch_workspace(QString const& id)
{
    if (!m_model.switch_workspace(id))
        return;
    m_pending_close.clear();
    m_pane_close.clear();
    apply_style();
    auto active = m_model.active;
    if (!active.isEmpty())
        activate(active);
    else
        new_page();
    rebuild();
    save();
}
void Sidebar::cycle_workspace(int direction)
{
    if (m_model.workspaces.size() < 2)
        return;
    int index = 0;
    for (int n = 0; n < m_model.workspaces.size(); ++n) {
        if (m_model.workspaces[n].id == m_model.active_workspace)
            index = n;
    }
    switch_workspace(m_model.workspaces[(index + direction + m_model.workspaces.size()) % m_model.workspaces.size()].id);
}
void Sidebar::select_workspace(int index)
{
    if (index >= 0 && index < m_model.workspaces.size())
        switch_workspace(m_model.workspaces[index].id);
}
void Sidebar::rebuild_workspaces()
{
    if (!m_space_header || !m_space_dock)
        return;
    QString signature = m_model.active_workspace;
    for (auto const& space : m_model.workspaces)
        signature += space.id + space.name + space.color + space.symbol;
    if (signature == m_workspace_render)
        return;
    m_workspace_render = signature;
    auto dark = Ladybird::ChromeStyle::is_dark(palette());
    if (auto* active = m_model.workspace(m_model.active_workspace)) {
        m_space_header->setText(active->name);
        m_space_header->setIcon(workspace_icon(*active, dark));
        m_space_header->setToolTip(active->name + " · Workspace settings");
        Thoth::localize_widget_tree(*m_space_header);
    }
    while (auto* item = m_space_dock->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    QToolButton* selected = nullptr;
    for (int n = 0; n < m_model.workspaces.size(); ++n) {
        auto space = m_model.workspaces[n];
        auto* button = new WorkspaceButton(m_space_dock->parentWidget());
        install_hover_event_filters(button);
        button->setFixedSize(32, 30);
        button->setIconSize({ 19, 19 });
        button->setIcon(workspace_icon(space, dark));
        button->setAccessibleName("Workspace " + space.name);
        button->setToolTip(space.name + (n < 9 ? QString(" · ⌘⌥%1").arg(n + 1) : QString {}));
        Thoth::localize_widget_tree(*button);
        button->setProperty("workspaceActive", space.id == m_model.active_workspace);
        button->setAcceptDrops(true);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QToolButton::clicked, this, [this, id = space.id] { switch_workspace(id); });
        connect(button, &QWidget::customContextMenuRequested, this, [this, button, id = space.id](QPoint p) { workspace_menu(id, button->mapToGlobal(p)); });
        button->on_drop = [this, destination = space.id](QString item) { if (m_model.move_to_workspace(item,destination)) { switch_workspace(destination); activate(item); rebuild(); save(); } };
        m_space_dock->addWidget(button);
        if (space.id == m_model.active_workspace)
            selected = button;
    }
    m_space_dock->addStretch();
    if (selected) {
        auto guard = QPointer<QToolButton>(selected);
        QTimer::singleShot(0, this, [this, guard] { if (guard) { if (auto* scroll = findChild<QScrollArea*>("ThothWorkspaceDock")) scroll->ensureWidgetVisible(guard,3,0); } });
    }
}
void Sidebar::workspace_menu(QString const& id, QPoint point)
{
    QMenu menu(this);
    auto* space = m_model.workspace(id);
    if (space) {
        auto* title = menu.addAction(space->name);
        title->setEnabled(false);
        auto* rename = menu.addAction("Rename Workspace…");
        connect(rename, &QAction::triggered, this, [this, id] {
            auto* current = m_model.workspace(id);
            if (!current)
                return;
            QString name;
            if (prompt_for_name(this, "Rename Workspace", "Workspace name", current->name, "Save", name)) {
                m_model.workspace(id)->name = name.trimmed();
                rebuild_workspaces();
                save();
            }
        });
        auto* colors = menu.addMenu("Color");
        QList<QPair<QString, QString>> palette { { "Lavender", "#b9a5df" }, { "Sage", "#a8c8ae" }, { "Rose", "#d7a7b8" }, { "Sand", "#d7bb82" }, { "Blue", "#9fbbe0" }, { "Graphite", "#aeb7c6" } };
        auto* color_group = new QActionGroup(colors);
        color_group->setExclusive(true);
        auto set_workspace_color = [this, id](QColor color) {
            auto* current = m_model.workspace(id);
            if (!current || !color.isValid())
                return;
            current->color = color.name(QColor::HexRgb);
            m_workspace_render.clear();
            apply_style();
            rebuild();
            save();
        };
        for (auto const& color : palette) {
            auto* action = colors->addAction(color.first);
            action->setIcon(workspace_color_swatch(color.second));
            action->setActionGroup(color_group);
            action->setCheckable(true);
            action->setChecked(QColor(space->color).name(QColor::HexRgb) == color.second);
            connect(action, &QAction::triggered, this, [set_workspace_color, color = color.second] { set_workspace_color(QColor(color)); });
        }
        auto* custom_color = colors->addAction("Custom…");
        custom_color->setIcon(workspace_color_swatch(space->color));
        connect(custom_color, &QAction::triggered, this, [this, id, set_workspace_color] {
            auto* current = m_model.workspace(id);
            if (!current)
                return;
            auto color = QColorDialog::getColor(QColor(current->color), this, "Choose Workspace Color");
            if (color.isValid())
                set_workspace_color(color);
        });
        auto* symbols = menu.addMenu("Icon");
        for (auto const& symbol : { QString("orbit"), QString("book"), QString("globe"), QString("diamond") }) {
            auto* action = symbols->addAction(symbol.left(1).toUpper() + symbol.mid(1));
            action->setCheckable(true);
            action->setChecked(space->symbol == symbol);
            connect(action, &QAction::triggered, this, [this, id, symbol] { if (auto* current=m_model.workspace(id)) { current->symbol=symbol; rebuild_workspaces(); save(); } });
        }
        auto* earlier = menu.addAction("Move Left");
        auto* later = menu.addAction("Move Right");
        int index = 0;
        for (int n = 0; n < m_model.workspaces.size(); ++n) {
            if (m_model.workspaces[n].id == id)
                index = n;
        }
        earlier->setEnabled(index > 0);
        later->setEnabled(index + 1 < m_model.workspaces.size());
        connect(earlier, &QAction::triggered, this, [this, index] { m_model.workspaces.swapItemsAt(index,index-1); rebuild_workspaces(); save(); });
        connect(later, &QAction::triggered, this, [this, index] { m_model.workspaces.swapItemsAt(index,index+1); rebuild_workspaces(); save(); });
        auto* remove = menu.addAction("Remove Workspace, Keep Pages");
        remove->setEnabled(m_model.workspaces.size() > 1);
        connect(remove, &QAction::triggered, this, [this, id] { if (m_model.remove_workspace(id)) switch_workspace(m_model.active_workspace); });
        menu.addSeparator();
    }
    auto* add = menu.addAction("New Workspace…");
    connect(add, &QAction::triggered, this, [this] { create_workspace(); });
    Thoth::localize_menu(menu);
    menu.exec(point);
}
void Sidebar::create_workspace()
{
    QString name;
    if (!prompt_for_name(this, "New Workspace", "Workspace name", {}, "Create", name))
        return;
    QStringList colors { "#b9a5df", "#a8c8ae", "#9fbbe0", "#d7a7b8", "#d7bb82" };
    auto id = m_model.add_workspace(name, colors[m_model.workspaces.size() % colors.size()], m_model.workspaces.size() % 2 ? "book" : "globe");
    switch_workspace(id);
}
void Sidebar::page_removal_finished()
{
    if (!m_removing_page || m_mutating)
        return;
    m_removing_page = false;
    if (!m_pending_close.isEmpty())
        return;
    QString next;
    for (auto const& entry : m_model.items) {
        if (entry.folder || !m_model.is_visible(entry))
            continue;
        bool loaded = false;
        for (auto const& pid : entry.pages) {
            if (m_live.value(pid))
                loaded = true;
        }
        if (!loaded)
            continue;
        if (entry.id == m_model.active) {
            next = entry.id;
            break;
        }
        if (next.isEmpty())
            next = entry.id;
    }
    if (!next.isEmpty())
        activate(next);
    else
        ensure_nonempty();
}

void Sidebar::save()
{
    if (!m_ready || m_mutating || m_private || m_closing || registry().restoring)
        return;
    auto& store = registry();
    store.windows[m_window_id] = m_model.to_json();
    auto document = QJsonObject { { "schema", 1 }, { "windows", store.windows } };
    auto bytes = QJsonDocument(document).toJson(QJsonDocument::Compact);
    if (store.last_saved == bytes)
        return;
    QString error;
    if (SidebarModel::save_document(store.path, document, &error))
        store.last_saved = bytes;
    else
        qWarning("Cannot save Thoth sidebar: %s", qPrintable(error));
    Ladybird::Application::the().update_reopen_recently_closed_actions();
}
void Sidebar::load_window(QJsonObject const& json)
{
    SidebarModel restored;
    if (!restored.load(json))
        return;
    m_mutating = true;
    QList<QPointer<Ladybird::Tab>> previous;
    for (auto tab : m_live)
        previous.append(tab);
    m_live.clear();
    m_icons.clear();
    m_model = std::move(restored);
    auto* startup_tab = previous.isEmpty() ? nullptr : previous.first().data();
    auto* reused_startup_tab = static_cast<Ladybird::Tab*>(nullptr);
    QString active_page;
    if (auto* active_item = m_model.item(m_model.active); active_item && !active_item->folder)
        active_page = active_item->active;
    for (auto const& page : m_model.pages) {
        QPixmap pixmap;
        if (pixmap.loadFromData(QByteArray::fromBase64(page.favicon.toLatin1()), "PNG"))
            m_icons[page.id] = QIcon(pixmap);
    }
    // Reuse the startup view for the active restored page. Keeping its browsing
    // context alive avoids closing the handle WebDriver selected during startup.
    auto pages = m_model.pages;
    for (auto const& p : pages) {
        auto* item = m_model.item_for_page(p.id);
        if (!item || !m_model.is_visible(*item) || (item->section != "temporary" && item->id != m_model.active))
            continue;
        if (startup_tab && p.id == active_page) {
            m_live[p.id] = startup_tab;
            reused_startup_tab = startup_tab;
            auto page_url = p.url == "about:newtab" ? QStringLiteral("about:blank") : p.url;
            startup_tab->navigate(ak_url_from_qstring(page_url).value_or(URL::about_blank()));
        } else
            ensure_page(p.id);
    }
    if (m_live.isEmpty()) {
        if (startup_tab && !m_model.pages.isEmpty()) {
            auto const& page = m_model.pages.first();
            m_live[page.id] = startup_tab;
            reused_startup_tab = startup_tab;
            auto page_url = page.url == "about:newtab" ? QStringLiteral("about:blank") : page.url;
            startup_tab->navigate(ak_url_from_qstring(page_url).value_or(URL::about_blank()));
        } else {
            auto& tab = browser().new_tab_from_url(URL::about_blank(), Web::HTML::ActivateTab::No, Ladybird::BrowserWindow::TabLocation::end());
            (void)tab;
        }
    }
    for (auto tab : previous) {
        if (tab && tab != reused_startup_tab) {
            auto index = m_tabs.index_of(tab);
            if (index >= 0)
                browser().definitely_close_tab(index);
        }
    }
    m_mutating = false;
    auto active = m_model.active;
    if (active.isEmpty() || !m_model.item(active) || m_model.item(active)->folder) {
        for (auto const& i : m_model.items) {
            if (!i.folder && m_model.is_visible(i)) {
                active = i.id;
                break;
            }
        }
    }
    update_compact_header();
    apply_collapsed_presentation(false);
    activate(active);
    rebuild();
}
void Sidebar::restore_session()
{
    if (m_private)
        return;
    auto& store = registry();
    auto saved = store.windows;
    if (saved.isEmpty()) {
        // One-time migration: alpha1 has flat upstream closed/recovered units, not Thoth groups.
        bool recovered_window = false;
        for (int n = 0; n < 200; ++n) {
            auto result = browser().session().session_store->take_most_recently_closed();
            if (result.is_error() || !result.value().has_value())
                break;
            auto unit = result.value().release_value();
            SidebarModel legacy;
            for (auto const& tab : unit.tabs) {
                auto pid = legacy.add_page(qstring_from_ak_string(tab.active_url.serialize()), qstring_from_ak_string(tab.active_url.serialize()));
                if (legacy.pages.size() - 1 == unit.active_tab_index)
                    legacy.active = legacy.item_for_page(pid)->id;
            }
            if (legacy.pages.isEmpty())
                continue;
            if (!recovered_window && unit.was_window) {
                legacy.closed = m_model.closed;
                load_window(legacy.to_json());
                recovered_window = true;
            } else
                m_model.closed.append(legacy.to_json());
        }
        m_ready = true;
        save();
        return;
    }
    store.restoring = true;
    store.live.remove(m_window_id);
    store.windows = {};
    bool first = true;
    for (auto it = saved.begin(); it != saved.end(); ++it) {
        Sidebar* sidebar = this;
        if (!first) {
            auto& window = Ladybird::Application::the().new_window({}, Ladybird::Application::the().configuration_for_new_window());
            sidebar = window.thoth_sidebar();
        }
        if (!sidebar)
            continue;
        store.live.remove(sidebar->m_window_id);
        sidebar->m_window_id = it.key();
        store.live.insert(it.key(), sidebar);
        sidebar->load_window(it.value().toObject());
        first = false;
    }
    store.restoring = false;
    for (auto* sidebar : store.live)
        store.windows[sidebar->m_window_id] = sidebar->m_model.to_json();
    m_ready = true;
    for (auto* sidebar : store.live) {
        sidebar->m_ready = true;
        sidebar->save();
    }
}
void Sidebar::window_closing()
{
    refresh();
    auto live_windows = 0;
    if (!m_private) {
        for (auto* sidebar : registry().live) {
            if (!sidebar->m_closing)
                ++live_windows;
        }
    }
    if (!m_private && !registry().quitting && live_windows > 1) {
        registry().windows.remove(m_window_id);
        auto document = QJsonObject { { "schema", 1 }, { "windows", registry().windows } };
        SidebarModel::save_document(registry().path, document);
    }
    m_closing = true;
}
void Sidebar::application_quitting(bool quitting)
{
    auto& store = registry();
    store.quitting = quitting;
    for (auto* sidebar : store.live)
        sidebar->refresh();
}
}
