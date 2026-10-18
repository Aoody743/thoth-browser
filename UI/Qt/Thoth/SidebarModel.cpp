/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SidebarModel.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace Thoth {
SidebarModel::SidebarModel()
{
    active_workspace = add_workspace("Personal");
}
Workspace* SidebarModel::workspace(QString const& id)
{
    for (auto& space : workspaces) {
        if (space.id == id)
            return &space;
    }
    return nullptr;
}
QString SidebarModel::add_workspace(QString name, QString color, QString symbol)
{
    if (name.trimmed().isEmpty() || !QRegularExpression("^#[0-9a-fA-F]{6}$").match(color).hasMatch()
        || (symbol != "orbit" && symbol != "book" && symbol != "globe" && symbol != "diamond"))
        return {};
    auto id = new_id();
    workspaces.append({ id, name.trimmed(), color, symbol, {} });
    return id;
}
bool SidebarModel::is_visible(SidebarItem const& entry) const
{
    return entry.section == "favourite" || entry.workspace == active_workspace;
}
bool SidebarModel::switch_workspace(QString const& id)
{
    auto* destination = workspace(id);
    if (!destination)
        return false;
    if (auto* previous = workspace(active_workspace); previous && item(active))
        previous->active = active;
    active_workspace = id;
    active = item(destination->active) && is_visible(*item(destination->active)) ? destination->active : QString {};
    if (active.isEmpty()) {
        for (auto const& entry : items) {
            if (!entry.folder && entry.workspace == id) {
                active = entry.id;
                break;
            }
        }
    }
    return true;
}
bool SidebarModel::move_to_workspace(QString const& id, QString const& destination)
{
    auto* entry = item(id);
    if (!entry || !workspace(destination))
        return false;
    QStringList moved { id };
    for (qsizetype n = 0; n < moved.size(); ++n) {
        for (auto const& candidate : items) {
            if (candidate.parent == moved[n])
                moved.append(candidate.id);
        }
    }
    auto source = entry->workspace;
    entry->parent.clear();
    for (auto& candidate : items) {
        if (!moved.contains(candidate.id))
            continue;
        candidate.workspace = destination;
        if (candidate.section == "favourite")
            candidate.section = "pinned";
    }
    if (auto* space = workspace(source); space && moved.contains(space->active))
        space->active.clear();
    if (moved.contains(active) && destination != active_workspace)
        active.clear();
    return true;
}
bool SidebarModel::remove_workspace(QString const& id)
{
    if (!workspace(id) || workspaces.size() == 1)
        return false;
    QString target;
    for (auto const& candidate : workspaces) {
        if (candidate.id != id) {
            target = candidate.id;
            break;
        }
    }
    for (auto& entry : items) {
        if (entry.workspace == id)
            entry.workspace = target;
    }
    if (active_workspace == id) {
        active_workspace = target;
        active = workspace(target)->active;
    }
    workspaces.removeIf([&](auto const& space) { return space.id == id; });
    if (!item(active))
        active.clear();
    return true;
}
QString SidebarModel::new_id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
SidebarPage* SidebarModel::page(QString const& id)
{
    for (auto& p : pages) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}
SidebarItem* SidebarModel::item(QString const& id)
{
    for (auto& i : items) {
        if (i.id == id)
            return &i;
    }
    return nullptr;
}
SidebarItem* SidebarModel::item_for_page(QString const& id)
{
    for (auto& i : items) {
        if (i.pages.contains(id))
            return &i;
    }
    return nullptr;
}
QString SidebarModel::add_page(QString url, QString title)
{
    auto id = new_id();
    pages.append({ id, url, title, {} });
    SidebarItem entry;
    entry.id = new_id();
    entry.workspace = active_workspace;
    entry.pages = { id };
    entry.active = id;
    items.append(entry);
    return id;
}
QString SidebarModel::add_folder(QString title, QString parent)
{
    if (!parent.isEmpty() && (!item(parent) || !item(parent)->folder))
        return {};
    SidebarItem entry;
    entry.id = new_id();
    entry.folder = true;
    entry.title = title;
    entry.parent = parent;
    entry.section = "pinned";
    entry.workspace = parent.isEmpty() ? active_workspace : item(parent)->workspace;
    items.append(entry);
    return entry.id;
}
bool SidebarModel::move(QString const& id, QString section, QString parent, QString before)
{
    auto* entry = item(id);
    if (!entry || (section != "favourite" && section != "pinned" && section != "temporary"))
        return false;
    if (section == "favourite" && (entry->folder || entry->pages.size() != 1))
        return false;
    if (entry->folder && section != "pinned")
        return false;
    if (!parent.isEmpty()) {
        auto* folder = item(parent);
        if (!folder || !folder->folder || section != "pinned" || folder->workspace != active_workspace)
            return false;
        auto ancestor = parent;
        while (!ancestor.isEmpty()) {
            if (ancestor == id)
                return false;
            auto* node = item(ancestor);
            if (!node)
                return false;
            ancestor = node->parent;
        }
    }
    if (!before.isEmpty() && before != id) {
        auto* sibling = item(before);
        if (!sibling || sibling->parent != parent || sibling->section != section || sibling->workspace != (section == "favourite" ? QString {} : active_workspace))
            return false;
    }
    if (before == id)
        return section == entry->section && parent == entry->parent;
    auto copy = *entry;
    copy.section = section;
    copy.parent = parent;
    copy.workspace = section == "favourite" ? QString {} : active_workspace;
    auto old = std::find_if(items.begin(), items.end(), [&](auto const& i) { return i.id == id; });
    items.erase(old);
    auto position = std::find_if(items.begin(), items.end(), [&](auto const& i) { return i.id == before; });
    items.insert(position, copy);
    if (copy.folder) {
        QStringList parents { copy.id };
        for (qsizetype n = 0; n < parents.size(); ++n) {
            for (auto& candidate : items) {
                if (candidate.parent == parents[n]) {
                    candidate.workspace = copy.workspace;
                    parents.append(candidate.id);
                }
            }
        }
    }
    return true;
}
bool SidebarModel::merge(QString const& first, QString const& second)
{
    auto* a = item(first);
    auto* b = item(second);
    if (!a || !b || a == b || a->folder || b->folder || a->pages.size() != 1 || b->pages.size() != 1)
        return false;
    if (!a->workspace.isEmpty() && !b->workspace.isEmpty() && a->workspace != b->workspace)
        return false;
    auto second_page = b->pages.first();
    if (a->workspace.isEmpty())
        a->workspace = b->workspace.isEmpty() ? active_workspace : b->workspace;
    a->pages.append(second_page);
    if (a->section == "favourite")
        a->section = "pinned";
    items.removeIf([&](auto const& i) { return i.id == second; });
    return true;
}
bool SidebarModel::ungroup(QString const& id)
{
    auto* entry = item(id);
    if (!entry || entry->pages.size() != 2)
        return false;
    auto index = std::distance(items.begin(), std::find_if(items.begin(), items.end(), [&](auto const& i) { return i.id == id; }));
    // Do not retain a pointer across QList insertion.
    auto other = *entry;
    other.id = new_id();
    other.pages = { entry->pages.last() };
    other.active = other.pages.first();
    other.ratio = 0.5;
    entry->pages.removeLast();
    entry->active = entry->pages.first();
    entry->ratio = 0.5;
    items.insert(index + 1, other);
    return true;
}
bool SidebarModel::close(QString const& id, bool remember)
{
    auto* entry = item(id);
    if (!entry)
        return false;
    QStringList removed { id };
    for (qsizetype n = 0; n < removed.size(); ++n) {
        for (auto const& candidate : items) {
            if (candidate.parent == removed[n])
                removed.append(candidate.id);
        }
    }
    SidebarModel snapshot;
    snapshot.workspaces = workspaces;
    snapshot.active_workspace = active_workspace;
    QJsonObject positions;
    for (qsizetype n = 0; n < items.size(); ++n) {
        auto const& candidate = items[n];
        if (!removed.contains(candidate.id))
            continue;
        auto detached = candidate;
        if (candidate.id == id)
            detached.parent.clear();
        snapshot.items.append(detached);
        positions[candidate.id] = static_cast<int>(n);
        for (auto const& pid : candidate.pages) {
            if (auto* p = page(pid))
                snapshot.pages.append(*p);
        }
    }
    snapshot.active = active;
    if (remember) {
        auto json = snapshot.to_json();
        json["positions"] = positions;
        json["parent"] = entry->parent;
        json["root"] = id;
        closed.prepend(json);
        while (closed.size() > 50)
            closed.removeLast();
    }
    QStringList removed_pages;
    for (auto const& i : snapshot.items)
        removed_pages.append(i.pages);
    items.removeIf([&](auto const& i) { return removed.contains(i.id); });
    pages.removeIf([&](auto const& p) { return removed_pages.contains(p.id); });
    if (removed.contains(active)) {
        active.clear();
        for (auto const& candidate : items) {
            if (!candidate.folder && is_visible(candidate))
                active = candidate.id;
        }
    }
    return true;
}
bool SidebarModel::close_page(QString const& id)
{
    auto* entry = item_for_page(id);
    if (!entry)
        return false;
    if (entry->pages.size() == 1)
        return close(entry->id);
    SidebarModel snapshot;
    snapshot.workspaces = workspaces;
    snapshot.active_workspace = active_workspace;
    auto original = *entry;
    original.parent.clear();
    snapshot.items.append(original);
    for (auto const& pid : entry->pages) {
        auto* snapshot_page = page(pid);
        if (!snapshot_page)
            return false;
        snapshot.pages.append(*snapshot_page);
    }
    snapshot.active = entry->id;
    auto json = snapshot.to_json();
    json["parent"] = entry->parent;
    json["root"] = entry->id;
    json["split_target"] = entry->id;
    json["closed_page"] = id;
    closed.prepend(json);
    while (closed.size() > 50)
        closed.removeLast();
    entry->pages.removeAll(id);
    entry->active = entry->pages.first();
    entry->ratio = 0.5;
    pages.removeIf([&](auto const& p) { return p.id == id; });
    return true;
}
QString SidebarModel::reopen()
{
    if (closed.isEmpty())
        return {};
    auto json = closed.first();
    SidebarModel restored;
    if (!restored.load(json))
        return {};
    if (json.contains("split_target")) {
        auto target = json["split_target"].toString();
        auto closed_page = json["closed_page"].toString();
        auto* original = restored.item(target);
        auto* remaining = item(target);
        if (!original || !original->pages.contains(closed_page) || !restored.page(closed_page) || page(closed_page))
            return {};
        if (remaining && remaining->pages.size() == 1 && original->pages.contains(remaining->pages.first())) {
            auto parent = remaining->parent;
            auto owner = remaining->workspace;
            auto section = remaining->section;
            *remaining = *original;
            remaining->parent = parent;
            remaining->workspace = owner;
            if (!workspace(remaining->workspace))
                remaining->workspace = active_workspace;
            remaining->section = section == "favourite" ? "pinned" : section;
            remaining->active = closed_page;
            pages.append(*restored.page(closed_page));
            active = target;
            closed.removeFirst();
            return active;
        }
        // If the surviving page has since moved or joined another group, restore only the closed page.
        auto page_copy = *restored.page(closed_page);
        auto entry = *original;
        entry.id = new_id();
        entry.pages = { closed_page };
        entry.active = closed_page;
        restored.pages = { page_copy };
        restored.items = { entry };
        restored.active = entry.id;
        json["root"] = entry.id;
    }
    auto positions = json["positions"].toObject();
    for (auto const& p : restored.pages) {
        if (page(p.id))
            return {};
    }
    for (auto const& i : restored.items) {
        if (item(i.id))
            return {};
    }
    for (auto i : restored.items) {
        if (i.section != "favourite" && !workspace(i.workspace))
            i.workspace = active_workspace;
        if (i.id == json["root"].toString()) {
            if (auto* parent = item(json["parent"].toString()); parent && parent->folder) {
                i.parent = parent->id;
                i.workspace = parent->workspace;
            }
        }
        if (!i.parent.isEmpty() && !item(i.parent) && !restored.item(i.parent)) {
            i.parent.clear();
            i.section = "temporary";
        }
        items.insert(std::clamp(positions[i.id].toInt(static_cast<int>(items.size())), 0, static_cast<int>(items.size())), i);
    }
    pages.append(restored.pages);
    closed.removeFirst();
    active = restored.item(restored.active) ? restored.active : restored.items.first().id;
    return active;
}
QJsonObject SidebarModel::to_json() const
{
    QJsonArray page_array, item_array, closed_array, space_array;
    for (auto const& space : workspaces)
        space_array.append(QJsonObject { { "id", space.id }, { "name", space.name }, { "color", space.color }, { "symbol", space.symbol }, { "active", space.active } });
    for (auto const& p : pages)
        page_array.append(QJsonObject { { "id", p.id }, { "url", p.url }, { "title", p.title }, { "favicon", p.favicon } });
    for (auto const& i : items)
        item_array.append(QJsonObject { { "id", i.id }, { "title", i.title }, { "parent", i.parent }, { "workspace", i.workspace }, { "section", i.section }, { "folder", i.folder }, { "expanded", i.expanded }, { "pages", QJsonArray::fromStringList(i.pages) }, { "active", i.active }, { "ratio", i.ratio } });
    for (auto const& c : closed)
        closed_array.append(c);
    return { { "schema", 2 }, { "workspaces", space_array }, { "active_workspace", active_workspace }, { "pages", page_array }, { "items", item_array }, { "closed", closed_array }, { "active", active }, { "width", width }, { "collapsed", collapsed } };
}
bool SidebarModel::load(QJsonObject const& json)
{
    if ((json["schema"].toInt() != 1 && json["schema"].toInt() != 2) || !json["pages"].isArray() || !json["items"].isArray())
        return false;
    SidebarModel next;
    QSet<QString> ids, used_pages;
    if (json["schema"].toInt() == 2) {
        next.workspaces.clear();
        for (auto const& value : json["workspaces"].toArray()) {
            auto o = value.toObject();
            Workspace space { o["id"].toString(), o["name"].toString(), o["color"].toString(), o["symbol"].toString(), o["active"].toString() };
            if (space.id.isEmpty() || ids.contains(space.id) || space.name.trimmed().isEmpty() || !QRegularExpression("^#[0-9a-fA-F]{6}$").match(space.color).hasMatch())
                return false;
            if (space.symbol != "orbit" && space.symbol != "book" && space.symbol != "globe" && space.symbol != "diamond")
                return false;
            ids.insert(space.id);
            next.workspaces.append(space);
        }
        next.active_workspace = json["active_workspace"].toString();
        if (!next.workspace(next.active_workspace))
            return false;
    }
    ids.clear();
    for (auto const& value : json["pages"].toArray()) {
        auto p = value.toObject();
        auto id = p["id"].toString();
        if (id.isEmpty() || ids.contains(id) || !p["url"].isString())
            return false;
        ids.insert(id);
        if (p["favicon"].toString().size() > 128 * 1024)
            return false;
        next.pages.append({ id, p["url"].toString(), p["title"].toString(), p["favicon"].toString() });
    }
    ids.clear();
    for (auto const& value : json["items"].toArray()) {
        auto o = value.toObject();
        SidebarItem i;
        i.id = o["id"].toString();
        i.title = o["title"].toString();
        i.parent = o["parent"].toString();
        i.section = o["section"].toString();
        i.workspace = i.section == "favourite" ? QString {} : json["schema"].toInt() == 1 ? next.active_workspace
                                                                                          : o["workspace"].toString();
        if (i.section != "favourite" && !next.workspace(i.workspace))
            return false;
        i.folder = o["folder"].toBool();
        i.expanded = o["expanded"].toBool(true);
        i.active = o["active"].toString();
        i.ratio = o["ratio"].toDouble(0.5);
        if (i.id.isEmpty() || ids.contains(i.id) || !std::isfinite(i.ratio) || i.ratio < 0.1 || i.ratio > 0.9)
            return false;
        if (i.section != "pinned" && i.section != "temporary" && i.section != "favourite")
            return false;
        ids.insert(i.id);
        for (auto const& pid : o["pages"].toArray()) {
            auto id = pid.toString();
            if (!next.page(id) || used_pages.contains(id))
                return false;
            used_pages.insert(id);
            i.pages.append(id);
        }
        if (i.folder ? (!i.pages.isEmpty() || i.section != "pinned") : (i.pages.isEmpty() || i.pages.size() > 2 || !i.pages.contains(i.active)))
            return false;
        if (i.section == "favourite" && i.pages.size() != 1)
            return false;
        next.items.append(i);
    }
    if (used_pages.size() != next.pages.size())
        return false;
    for (auto const& i : next.items) {
        QSet<QString> ancestors { i.id };
        auto parent = i.parent;
        while (!parent.isEmpty()) {
            auto* folder = next.item(parent);
            if (!folder || !folder->folder || i.section != "pinned" || folder->workspace != i.workspace || ancestors.contains(parent))
                return false;
            ancestors.insert(parent);
            parent = folder->parent;
        }
    }
    next.active = next.item(json["active"].toString()) ? json["active"].toString() : QString {};
    next.width = std::clamp(json["width"].toInt(228), 190, 400);
    next.collapsed = json["collapsed"].toBool();
    for (auto const& c : json["closed"].toArray()) {
        if (next.closed.size() < 50 && c.isObject())
            next.closed.append(c.toObject());
    }
    *this = std::move(next);
    return true;
}
static QJsonObject read_valid(QString const& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32 * 1024 * 1024)
        return {};
    auto doc = QJsonDocument::fromJson(file.readAll());
    auto o = doc.object();
    if (o["schema"].toInt() != 1 || !o["windows"].isObject())
        return {};
    auto windows = o["windows"].toObject();
    for (auto value : windows) {
        SidebarModel model;
        if (!model.load(value.toObject()))
            return {};
    }
    return o;
}
QJsonObject SidebarModel::read_document(QString const& path)
{
    auto current = read_valid(path);
    return current.isEmpty() ? read_valid(path + ".bak") : current;
}
bool SidebarModel::save_document(QString const& path, QJsonObject const& json, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    auto previous = read_valid(path);
    if (!previous.isEmpty()) {
        QSaveFile backup(path + ".bak");
        auto data = QJsonDocument(previous).toJson();
        if (!backup.open(QIODevice::WriteOnly) || backup.write(data) != data.size() || !backup.commit()) {
            if (error)
                *error = backup.errorString();
            return false;
        }
    }
    QSaveFile file(path);
    auto data = QJsonDocument(json).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
}
