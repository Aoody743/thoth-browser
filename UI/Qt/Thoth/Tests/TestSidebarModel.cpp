/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../SidebarModel.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdlib>

static void check(bool condition, char const* message)
{
    if (!condition) {
        qCritical() << "FAIL:" << message;
        std::exit(1);
    }
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Thoth::SidebarModel model;
    auto a = model.add_page("https://same.example/", "First");
    auto b = model.add_page("https://same.example/", "Second");
    check(a != b, "same URL retains independent page identity");
    auto ai = model.item_for_page(a)->id;
    auto bi = model.item_for_page(b)->id;
    auto folder = model.add_folder("Study");
    auto nested = model.add_folder("Class", folder);
    check(!model.move(folder, "pinned", nested), "cannot nest folder inside descendant");
    check(model.move(ai, "pinned", nested), "move page into nested folder");
    check(model.merge(ai, bi), "merge two independent pages");
    check(model.items.size() == 3 && model.item(ai)->pages.size() == 2, "one sidebar row owns two pages");
    check(!model.move(ai, "favourite"), "favourites contain only single pages");
    auto c = model.add_page("https://third.example/", "Third");
    auto ci = model.item_for_page(c)->id;
    check(!model.merge(ai, ci), "groups cannot exceed two pages");
    model.active = ai;
    model.item(ai)->active = b;
    model.item(ai)->ratio = 0.37;
    model.width = 286;
    model.page(a)->favicon = "test-favicon-data";
    auto json = model.to_json();
    Thoth::SidebarModel recovered;
    check(recovered.load(json), "snapshot restores");
    check(recovered.item(ai)->active == b && recovered.item(ai)->ratio == 0.37 && recovered.item(ai)->parent == nested, "focus ratio and folder restore");
    check(recovered.page(a)->favicon == "test-favicon-data", "favicon data survives restart");
    check(recovered.width == 286 && recovered.pages.size() == 3, "geometry and duplicate URL pages restore");
    check(model.close(ai), "close group");
    check(!model.page(a) && !model.page(b), "whole group closes");
    check(model.reopen() == ai && model.item(ai)->pages == QStringList({ a, b }) && model.item(ai)->parent == nested, "undo restores group identity order and parent");
    check(model.ungroup(ai), "ungroup");
    check(model.item_for_page(b)->id != ai && model.item_for_page(b)->parent == nested, "ungroup retains folder");
    auto second = model.item_for_page(b)->id;
    check(model.move(second, "favourite"), "pin favourite");
    check(model.move(second, "temporary"), "unpin to temporary");
    check(model.move(second, "temporary", {}, ci), "reorder before sibling");
    check(model.merge(ai, second), "regroup");
    check(model.close_page(b), "close one pane");
    check(model.item(ai)->pages == QStringList({ a }) && !model.page(b), "remaining pane becomes single");
    check(model.reopen() == ai && model.item(ai)->pages == QStringList({ a, b }) && model.item(ai)->active == b, "undo individual pane rejoins the original split");
    model.closed.prepend(QJsonObject { { "schema", 2 }, { "pages", QJsonArray {} }, { "items", QJsonArray {} } });
    auto closed_count = model.closed.size();
    check(model.reopen().isEmpty() && model.closed.size() == closed_count, "failed undo keeps the closed-item history");
    auto invalid = json;
    invalid["schema"] = 999;
    auto unchanged = recovered.to_json();
    check(!recovered.load(invalid) && recovered.to_json() == unchanged, "invalid schema does not clobber current state");
    check(model.close(folder), "close nested folder");
    check(!model.item(nested), "descendants close");
    model.reopen();
    check(model.item(folder) && model.item(nested) && model.item(nested)->parent == folder && model.item_for_page(a)->parent == nested, "nested folder undo restores the complete hierarchy");
    auto bad = json;
    auto bad_items = bad["items"].toArray();
    auto cyclic = bad_items[0].toObject();
    cyclic["parent"] = cyclic["id"];
    bad_items[0] = cyclic;
    bad["items"] = bad_items;
    check(!recovered.load(bad) && recovered.to_json() == unchanged, "corrupt cyclic hierarchy is rejected atomically");
    bad = json;
    auto bad_pages = bad["pages"].toArray();
    bad_pages.append(bad_pages[0]);
    bad["pages"] = bad_pages;
    check(!recovered.load(bad), "duplicate page identities are rejected");
    auto order = model.to_json().value("items");
    check(model.move(ci, "temporary", {}, ci) && model.to_json()["items"] == order, "dropping on itself preserves order");
    Thoth::SidebarModel spaces;
    auto personal = spaces.active_workspace;
    auto first_page = spaces.add_page("https://one.example", "One");
    auto first_item = spaces.item_for_page(first_page)->id;
    spaces.active = first_item;
    auto shared_page = spaces.add_page("https://shared.example", "Shared");
    auto shared_item = spaces.item_for_page(shared_page)->id;
    check(spaces.move(shared_item, "favourite"), "favourite becomes global");
    auto study = spaces.add_workspace("Study", "#e7efe9", "book");
    check(spaces.switch_workspace(study) && spaces.active.isEmpty(), "new space begins with no selected page");
    check(!spaces.is_visible(*spaces.item(first_item)) && spaces.is_visible(*spaces.item(shared_item)), "only favourites span spaces");
    auto study_page = spaces.add_page("https://study.example", "Study");
    auto study_item = spaces.item_for_page(study_page)->id;
    spaces.active = study_item;
    check(!spaces.merge(first_item, study_item), "cross-space merge cannot silently move pages");
    check(spaces.switch_workspace(personal) && spaces.active == first_item, "switch restores previous active item");
    check(spaces.switch_workspace(study) && spaces.active == study_item, "spaces preserve independent selections");
    auto space_folder = spaces.add_folder("Notes");
    check(spaces.move(study_item, "pinned", space_folder), "folder accepts its space's page");
    check(spaces.move_to_workspace(space_folder, personal), "move whole folder across spaces");
    check(spaces.item(study_item)->workspace == personal && spaces.item(study_item)->parent == space_folder, "folder transfer preserves children");
    check(spaces.item(shared_item)->workspace.isEmpty(), "global favourite is not duplicated or moved");
    check(spaces.switch_workspace(personal), "switch before closing a page");
    spaces.active = study_item;
    check(spaces.close(study_item) && spaces.reopen() == study_item && spaces.item(study_item)->workspace == personal, "closed pages restore workspace membership");
    Thoth::SidebarModel spaces_restored;
    check(spaces_restored.load(spaces.to_json()) && spaces_restored.workspaces.size() == 2 && spaces_restored.active_workspace == personal, "space definitions and selection survive restart");
    auto count_before = spaces.pages.size();
    check(spaces.remove_workspace(personal) && spaces.pages.size() == count_before && spaces.item(study_item)->workspace == study, "removing a space keeps its pages");
    check(!spaces.remove_workspace(study), "the last space cannot be removed");
    auto legacy = spaces.to_json();
    legacy["schema"] = 1;
    legacy.remove("workspaces");
    legacy.remove("active_workspace");
    auto legacy_items = legacy["items"].toArray();
    for (int n = 0; n < legacy_items.size(); ++n) {
        auto entry = legacy_items[n].toObject();
        entry.remove("workspace");
        legacy_items[n] = entry;
    }
    legacy["items"] = legacy_items;
    Thoth::SidebarModel migrated;
    check(migrated.load(legacy) && migrated.workspaces.size() == 1 && migrated.pages.size() == spaces.pages.size(), "alpha2 single-space data migrates without losing pages");
    for (auto const& entry : migrated.items)
        check(entry.section == "favourite" || entry.workspace == migrated.active_workspace, "migration assigns every organized item to the default space");
    QTemporaryDir directory;
    auto path = directory.filePath("sidebar.json");
    QJsonObject document { { "schema", 1 }, { "windows", QJsonObject { { "window1", json } } } };
    check(Thoth::SidebarModel::save_document(path, document), "atomic save");
    document["windows"] = QJsonObject { { "window1", recovered.to_json() } };
    check(Thoth::SidebarModel::save_document(path, document), "second save keeps backup");
    QFile broken(path);
    check(broken.open(QIODevice::WriteOnly), "open corruption fixture");
    broken.write("{ truncated");
    broken.close();
    auto backup = Thoth::SidebarModel::read_document(path);
    check(backup["windows"].toObject()["window1"].toObject() == json, "corruption recovers previous valid backup");
    QJsonObject multi { { "schema", 1 }, { "windows", QJsonObject { { "first", json }, { "second", recovered.to_json() } } } };
    check(Thoth::SidebarModel::save_document(path, multi), "save multiple independent windows");
    check(Thoth::SidebarModel::read_document(path)["windows"].toObject().size() == 2, "restore multiple window snapshots");
    Thoth::SidebarModel source;
    auto research = source.add_workspace("Research");
    source.switch_workspace(research);
    auto left = source.add_page("https://alpha.example/paper", "Alpha Paper");
    auto right = source.add_page("https://beta.example/notes", "Beta Notes");
    auto group = source.item_for_page(left)->id;
    source.merge(group, source.item_for_page(right)->id);
    source.item(group)->active = right;
    source.item(group)->ratio = 0.38;
    source.page(left)->sleeping = true;
    check(source.find_pages("RESEARCH beta") == QStringList { right }, "search spans workspace/title case-insensitively");
    check(source.find_pages("alpha.example/paper") == QStringList { left }, "search includes URL");
    check(source.find_pages("unmatched").isEmpty(), "no match");
    source.switch_workspace(source.workspaces.first().id);
    check(source.find_pages("notes") == QStringList { right }, "search finds inactive workspace split pane");
    auto portable = source.export_workspaces();
    Thoth::SidebarModel destination;
    auto existing = destination.add_page("https://existing.example/", "Existing");
    auto old_active = destination.active;
    check(destination.import_workspaces(portable), "import workspace archive");
    check(destination.active == old_active && destination.page(existing), "import preserves current pages and selection");
    auto matches = destination.find_pages("research alpha");
    check(matches.size() == 1 && matches.first() != left, "import remaps identities");
    auto* imported = destination.item_for_page(matches.first());
    check(imported && imported->pages.size() == 2 && imported->active != right && imported->ratio == 0.38, "import preserves split structure and focus with fresh IDs");
    check(destination.page(matches.first())->sleeping, "sleep state survives export/import");
    check(destination.import_workspaces(portable) && destination.find_pages("research alpha").size() == 2, "repeat import cannot collide");
    Thoth::SidebarModel roundtrip;
    check(roundtrip.load(destination.to_json()) && roundtrip.pages.size() == 5, "imported model survives persistence validation");
    auto before_bad_import = destination.to_json();
    auto invalid_archive = portable;
    invalid_archive["version"] = 99;
    check(!destination.import_workspaces(invalid_archive) && destination.to_json() == before_bad_import, "unknown archive version leaves all state intact");
    auto hostile = source;
    hostile.page(left)->url = "javascript:alert(1)";
    check(!destination.import_workspaces(hostile.export_workspaces()) && destination.to_json() == before_bad_import, "executable imported URL rejected atomically");
    qInfo() << "PASS: identity, nested folders, groups, close/undo, snapshot recovery, page search, portable workspaces and sleep state";
}
