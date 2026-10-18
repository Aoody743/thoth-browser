/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once
#include "SidebarModel.h"
#include <QHash>
#include <QIcon>
#include <QPointer>
#include <QWidget>

class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QSplitter;
class QVBoxLayout;
class QHBoxLayout;
class QMenu;
class QToolButton;
class QTimer;
class QVariantAnimation;
class QWidget;
namespace Ladybird {
class Tab;
class TabWidget;
class BrowserWindow;
}
namespace Thoth {
class Sidebar final : public QWidget {
public:
    Sidebar(Ladybird::TabWidget&, QStackedWidget& navigation);
    ~Sidebar() override;
    void page_added(Ladybird::Tab*);
    void attach_navigation(Ladybird::Tab*);
    void page_removed(Ladybird::Tab*);
    void page_removal_finished();
    void cycle_workspace(int);
    void select_workspace(int);
    void active_changed(Ladybird::Tab*);
    void page_loading_changed(Ladybird::Tab*);
    void refresh();
    void choose_split();
    void close_current();
    bool reopen();
    bool can_reopen() const { return !m_model.closed.isEmpty(); }
    bool keeps_window_open() const { return true; }
    void ensure_nonempty();
    void restore_active();
    void cycle(int direction);
    void toggle_collapsed();
    void expand();
    void set_chrome_visible(bool);
    void set_browser_menu(QMenu&);
    void refresh_appearance();
    void refresh_language();
    void set_outer_splitter(QSplitter*);
    void restore_session();
    void window_closing();
    static void application_quitting(bool);
    void save();

protected:
    bool event(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    Ladybird::BrowserWindow& browser() const;
    void rebuild();
    void apply_style();
    void activate(QString const& item, QString page = {});
    Ladybird::Tab* ensure_page(QString const&);
    QString page_id(Ladybird::Tab*) const;
    QString label(SidebarItem const&) const;
    QIcon icon(QString const&) const;
    void context_menu(QString const&, QPoint, QString pane = {});
    void close_item(QString const&, QString pane = {});
    void drop_item(QString const&, QString section, QString parent, QString before = {});
    void load_window(QJsonObject const&);
    void sync_pages();
    void apply_collapsed_presentation(bool reveal, bool animate = false);
    void update_collapsed_overlay_geometry();
    void reveal_collapsed_sidebar();
    void hide_collapsed_sidebar();
    void install_hover_event_filters(QWidget*);
    void update_compact_header();
    void update_new_tab_prompt_geometry();
    void style_browser_menu();
    void workspace_menu(QString const&, QPoint);
    void switch_workspace(QString const&);
    void rebuild_workspaces();
    void create_workspace();
    void new_page();

    Ladybird::TabWidget& m_tabs;
    SidebarModel m_model;
    QHash<QString, QPointer<Ladybird::Tab>> m_live;
    QHash<QString, QIcon> m_icons;
    QHash<QString, QString> m_pending_close;
    QHash<QString, bool> m_pane_close;
    QStackedWidget& m_navigation;
    QStackedWidget* m_navigation_buttons {};
    QWidget* m_body {};
    QListWidget* m_favourites {};
    QTreeWidget* m_tree {};
    QSplitter* m_outer {};
    QPointer<QWidget> m_host;
    QPointer<QWidget> m_reveal_handle;
    QPointer<QWidget> m_new_tab_prompt;
    QTimer* m_hover_timer {};
    QTimer* m_loading_animation_timer {};
    QVariantAnimation* m_collapse_animation {};
    QToolButton* m_space_header {};
    QToolButton* m_browser_menu_button {};
    QPointer<QMenu> m_browser_menu;
    QHBoxLayout* m_space_dock {};
    QString m_workspace_render;
    bool m_removing_page { false };
    bool m_workspace_swipe_lock { false };
    int m_workspace_swipe { 0 };
    QString m_window_id;
    QString m_next_page_id;
    QByteArray m_last_render;
    bool m_styling { false };
    bool m_hover_revealed { false };
    bool m_ignore_hover_until_leave { false };
    bool m_dragging_sidebar_item { false };
    bool m_chrome_visible { true };
    int m_hover_outside_ms { 0 };
    int m_loading_animation_frame { 0 };
    bool m_private { false };
    bool m_mutating { false };
    bool m_ready { false };
    bool m_closing { false };
};
}
