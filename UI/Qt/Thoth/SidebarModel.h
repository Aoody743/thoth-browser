/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace Thoth {
struct SidebarPage {
    QString id;
    QString url;
    QString title;
    QString favicon;
    bool sleeping { false };
};
struct Workspace {
    QString id;
    QString name;
    QString color { "#b9a5df" };
    QString symbol { "orbit" };
    QString active;
};
struct SidebarItem {
    QString id;
    QString title;
    QString parent;
    QString workspace;
    QString section { "temporary" };
    QStringList pages;
    QString active;
    bool folder { false };
    bool expanded { true };
    double ratio { 0.5 };
};

// UI-independent organization. Renderer tabs and upstream session IDs are deliberately not identities here.
class SidebarModel {
public:
    SidebarModel();
    QList<Workspace> workspaces;
    QString active_workspace;
    QList<SidebarPage> pages;
    QList<SidebarItem> items;
    QList<QJsonObject> closed;
    QString active;
    int width { 228 };
    bool collapsed { false };

    static QString new_id();
    Workspace* workspace(QString const&);
    QString add_workspace(QString name, QString color = "#b9a5df", QString symbol = "orbit");
    bool switch_workspace(QString const&);
    bool move_to_workspace(QString const& item, QString const& workspace);
    bool remove_workspace(QString const&);
    bool is_visible(SidebarItem const&) const;
    SidebarPage* page(QString const&);
    SidebarItem* item(QString const&);
    SidebarItem* item_for_page(QString const&);
    QString add_page(QString url = "about:blank", QString title = "New Tab");
    QString add_folder(QString title, QString parent = {});
    bool move(QString const& id, QString section, QString parent = {}, QString before = {});
    bool merge(QString const& first, QString const& second);
    bool ungroup(QString const&);
    bool close(QString const&, bool remember = true);
    bool close_page(QString const&);
    QString reopen();
    QStringList find_pages(QString const& query) const;
    QJsonObject export_workspaces() const;
    bool import_workspaces(QJsonObject const&);
    QJsonObject to_json() const;
    bool load(QJsonObject const&);
    static bool save_document(QString const& path, QJsonObject const&, QString* error = nullptr);
    static QJsonObject read_document(QString const& path);
};
}
