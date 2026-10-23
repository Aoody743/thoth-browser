/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "Sidebar.h"
#include "Localization.h"
#include <LibWebView/Application.h>
#include <LibWebView/BookmarkStore.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/StringUtils.h>
#include <UI/Qt/TabBar.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

namespace Thoth {
void Sidebar::show_page_search()
{
    if (auto* existing = findChild<QDialog*>("ThothPageSearch")) {
        existing->raise();
        existing->activateWindow();
        return;
    }
    sync_pages();
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("ThothPageSearch");
    dialog->setWindowTitle("Find Open Page");
    dialog->resize(620, 440);
    auto* layout = new QVBoxLayout(dialog);
    auto* input = new QLineEdit(dialog);
    input->setObjectName("ThothPageSearchInput");
    input->setAccessibleName("Search open pages");
    input->setPlaceholderText("Search titles, URLs and workspaces");
    layout->addWidget(input);
    auto* results = new QListWidget(dialog);
    results->setObjectName("ThothPageSearchResults");
    results->setAccessibleName("Matching pages");
    layout->addWidget(results, 1);
    auto* count = new QLabel(dialog);
    layout->addWidget(count);
    auto populate = [this, input, results, count] {
        results->clear();
        for (auto const& id : m_model.find_pages(input->text())) {
            auto* page = m_model.page(id);
            auto* item = m_model.item_for_page(id);
            if (!page || !item)
                continue;
            auto* workspace = m_model.workspace(item->workspace);
            auto location = workspace ? workspace->name : QString("Favourites");
            if (item->pages.size() == 2)
                location += item->pages.first() == id ? " · Left pane" : " · Right pane";
            if (page->sleeping)
                location += " · Sleeping";
            auto* row = new QListWidgetItem(icon(id), page->title + "\n" + location + " — " + page->url, results);
            row->setData(Qt::UserRole, id);
            row->setToolTip(page->url);
        }
        if (results->count())
            results->setCurrentRow(0);
        count->setText(results->count() ? QString("%1 pages · Enter to switch · Esc to close").arg(results->count()) : "No matching pages");
    };
    connect(input, &QLineEdit::textChanged, dialog, populate);
    auto open = [this, dialog, results] {
        auto* row = results->currentItem();
        if (!row)
            return;
        auto id = row->data(Qt::UserRole).toString();
        auto* item = m_model.item_for_page(id);
        if (!item)
            return;
        activate(item->id, id);
        dialog->accept();
    };
    connect(input, &QLineEdit::returnPressed, dialog, open);
    connect(results, &QListWidget::itemActivated, dialog, [open] { open(); });
    for (auto const& [key, direction] : { std::pair { Qt::Key_Down, 1 }, std::pair { Qt::Key_Up, -1 } }) {
        auto* shortcut = new QShortcut(QKeySequence(key), input);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, dialog, [results, direction] {
            if (results->count())
                results->setCurrentRow((results->currentRow() + direction + results->count()) % results->count());
        });
    }
    populate();
    localize_widget_tree(*dialog);
    dialog->open();
    input->setFocus();
}

void Sidebar::show_site_settings()
{
    if (auto* tab = browser().current_tab())
        tab->show_site_settings();
}

void Sidebar::export_workspaces()
{
    if (m_private)
        return;
    sync_pages();
    auto path = QFileDialog::getSaveFileName(this, "Export Workspaces", "thoth-workspaces.json", "Thoth Workspaces (*.json)");
    if (path.isEmpty())
        return;
    auto bytes = QJsonDocument(m_model.export_workspaces()).toJson();
    if (bytes.size() > 8 * 1024 * 1024) {
        QMessageBox::warning(this, "Export Workspaces", "The backup exceeds the supported 8 MiB size. Reduce saved pages or favicons before exporting.");
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        QMessageBox::warning(this, "Export Workspaces", "Unable to save the workspace backup: " + file.errorString());
}

void Sidebar::import_workspaces()
{
    auto path = QFileDialog::getOpenFileName(this, "Import Workspaces", {}, "Thoth Workspaces (*.json)");
    if (path.isEmpty())
        return;
    QFile file(path);
    QJsonParseError error;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
        QMessageBox::warning(this, "Import Workspaces", "Unable to read the backup, or the file exceeds 8 MiB.");
        return;
    }
    auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() || !m_model.import_workspaces(document.object())) {
        QMessageBox::warning(this, "Import Workspaces", "This is not a supported, valid Thoth workspace backup. Current pages have not changed.");
        return;
    }
    m_last_render.clear();
    rebuild();
    save();
    QMessageBox::information(this, "Import Workspaces", "Workspaces were added. Your existing pages are unchanged. Imported pages load when you open them.");
}

void Sidebar::bookmark_page(QString const& id)
{
    sync_pages();
    auto* page = m_model.page(id);
    if (!page)
        return;
    auto url = ak_url_from_qstring(page->url);
    if (!url.has_value())
        return;
    auto& bookmarks = WebView::Application::bookmark_store();
    if (!bookmarks.is_bookmarked(*url))
        bookmarks.add_bookmark(*url, ak_string_from_qstring(page->title), {});
}

void Sidebar::import_bookmarks()
{
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("ThothBookmarkPicker");
    dialog->setWindowTitle("Copy Bookmarks to Sidebar");
    dialog->resize(520, 420);
    auto* layout = new QVBoxLayout(dialog);
    auto* note = new QLabel("Choose a bookmark or folder to copy into this workspace. Copies are independent: renaming or removing one does not change the other. Use Bookmarks to import/export HTML.", dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* tree = new QTreeWidget(dialog);
    tree->setObjectName("ThothBookmarkChoices");
    tree->setHeaderLabels({ "Bookmark", "URL" });
    layout->addWidget(tree, 1);
    std::function<void(Vector<WebView::BookmarkItem> const&, QTreeWidgetItem*)> add = [&](auto const& items, QTreeWidgetItem* parent) {
        for (auto const& item : items) {
            auto* row = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
            row->setData(0, Qt::UserRole, qstring_from_ak_string(item.id));
            if (item.is_folder()) {
                row->setText(0, item.folder().title.has_value() ? qstring_from_ak_string(*item.folder().title) : "Folder");
                add(item.folder().children, row);
            } else {
                row->setText(0, qstring_from_ak_string(item.bookmark().title.value_or(item.bookmark().url.serialize())));
                row->setText(1, qstring_from_ak_string(item.bookmark().url.serialize()));
            }
        }
    };
    add(WebView::Application::bookmark_store().root_items(), nullptr);
    tree->expandAll();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, dialog);
    auto* pinned = buttons->addButton("Copy to Workspace", QDialogButtonBox::ActionRole);
    auto* favourite = buttons->addButton("Copy to Favourites", QDialogButtonBox::ActionRole);
    auto* manage = buttons->addButton("Bookmarks…", QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto update = [tree, pinned, favourite] {
        auto* row = tree->currentItem();
        pinned->setEnabled(row);
        auto item = row ? WebView::Application::bookmark_store().find_item_by_id(ak_string_from_qstring(row->data(0, Qt::UserRole).toString())) : Optional<WebView::BookmarkItem const&> {};
        favourite->setEnabled(item.has_value() && item->is_bookmark());
    };
    connect(tree, &QTreeWidget::currentItemChanged, dialog, [update] { update(); });
    update();
    connect(manage, &QPushButton::clicked, dialog, [this, dialog] {
        browser().new_tab_from_url(*ak_url_from_qstring("about:bookmarks"), Web::HTML::ActivateTab::Yes, Ladybird::BrowserWindow::TabLocation::end());
        dialog->close();
    });
    auto copy = [this, dialog, tree](bool as_favourite) {
        if (!tree->currentItem())
            return;
        auto selected = WebView::Application::bookmark_store().find_item_by_id(ak_string_from_qstring(tree->currentItem()->data(0, Qt::UserRole).toString()));
        if (!selected.has_value())
            return;
        int copied = 0;
        std::function<void(WebView::BookmarkItem const&, QString)> transfer = [&](auto const& item, QString parent) {
            if (item.is_folder()) {
                auto folder = m_model.add_folder(item.folder().title.has_value() ? qstring_from_ak_string(*item.folder().title) : "Folder", parent);
                for (auto const& child : item.folder().children)
                    transfer(child, folder);
            } else {
                auto const& bookmark = item.bookmark();
                auto url = qstring_from_ak_string(bookmark.url.serialize());
                if (!QStringList { "https", "http", "file", "about" }.contains(QUrl(url).scheme()))
                    return;
                auto pid = m_model.add_page(url, qstring_from_ak_string(bookmark.title.value_or(bookmark.url.serialize())));
                m_model.move(m_model.item_for_page(pid)->id, as_favourite ? "favourite" : "pinned", parent);
                ++copied;
            }
        };
        transfer(*selected, {});
        rebuild();
        save();
        if (!copied)
            QMessageBox::information(dialog, "Copy Bookmarks", "No supported page URLs were found. HTTP, HTTPS, file and about URLs can be copied.");
        dialog->accept();
    };
    connect(pinned, &QPushButton::clicked, dialog, [copy] { copy(false); });
    connect(favourite, &QPushButton::clicked, dialog, [copy] { copy(true); });
    localize_widget_tree(*dialog);
    dialog->open();
}

bool Sidebar::sleep_page(QString const& pid)
{
    auto* tab = m_live.value(pid).data();
    auto* item = m_model.item_for_page(pid);
    if (!tab || !item || item->id == m_model.active || !tab->can_sleep())
        return false;
    sync_pages();
    auto active = m_model.active;
    auto* active_item = m_model.item(active);
    auto active_page = active_item ? active_item->active : QString {};
    m_mutating = true;
    auto closed = browser().definitely_close_tab(browser().tab_index(tab), false);
    m_mutating = false;
    if (closed) {
        if (auto* page = m_model.page(pid))
            page->sleeping = true;
    }
    activate(active, active_page);
    return closed;
}

void Sidebar::sleep_other_pages()
{
    auto candidates = m_live.keys();
    int slept = 0;
    QMap<QString, int> protected_pages;
    for (auto const& pid : candidates) {
        if (sleep_page(pid)) {
            ++slept;
        } else if (auto* tab = m_live.value(pid).data()) {
            auto* item = m_model.item_for_page(pid);
            auto reason = item && item->id == m_model.active ? QString("Current page or split") : tab->sleep_block_reason();
            ++protected_pages[reason.isEmpty() ? QString("Page could not close") : reason];
        }
    }
    QStringList reasons;
    for (auto it = protected_pages.cbegin(); it != protected_pages.cend(); ++it)
        reasons.append(QString("%1: %2").arg(it.key()).arg(it.value()));
    QMessageBox::information(this, "Sleep Background Pages", QString("%1 pages put to sleep. Their URLs and sidebar positions are kept; opening them reloads them.\n\nProtected pages:\n%2\n\nPage scripts and form state are not saved. Active downloads prevent sleep.").arg(slept).arg(reasons.join('\n')));
}

void Sidebar::add_tools_menu(QMenu& menu)
{
    auto* search = menu.addAction("Find Open Page…");
    search->setObjectName("ThothFindOpenPageAction");
    search->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    browser().addAction(search);
    connect(search, &QAction::triggered, this, &Sidebar::show_page_search);
    auto* site = menu.addAction("Site Settings…");
    connect(site, &QAction::triggered, this, &Sidebar::show_site_settings);
    auto* sleep = menu.addAction("Sleep Background Pages");
    connect(sleep, &QAction::triggered, this, &Sidebar::sleep_other_pages);
    menu.addSeparator();
    auto* bookmarks = menu.addAction("Copy Bookmarks to Sidebar…");
    connect(bookmarks, &QAction::triggered, this, &Sidebar::import_bookmarks);
    auto* backup = menu.addAction("Export Workspaces…");
    backup->setEnabled(!m_private);
    if (m_private)
        backup->setToolTip("Private workspaces are not exported");
    connect(backup, &QAction::triggered, this, &Sidebar::export_workspaces);
    auto* restore = menu.addAction("Import Workspaces…");
    connect(restore, &QAction::triggered, this, &Sidebar::import_workspaces);
    auto* close_pane = menu.addAction("Close Active Split Pane");
    connect(close_pane, &QAction::triggered, this, &Sidebar::close_active_pane);
    auto update_close_pane = [this, close_pane] {
        auto* item = m_model.item(m_model.active);
        close_pane->setEnabled(item && item->pages.size() == 2);
    };
    connect(&menu, &QMenu::aboutToShow, this, update_close_pane);
    if (auto* native = browser().menuBar()->findChild<QMenu*>("ThothApplicationMenu")) {
        native->addSeparator();
        for (auto* action : { search, site, sleep, bookmarks, backup, restore, close_pane })
            native->addAction(action);
        connect(native, &QMenu::aboutToShow, this, update_close_pane);
    }
}

void Sidebar::close_active_pane()
{
    auto* item = m_model.item(m_model.active);
    if (item && item->pages.size() == 2)
        close_item(item->id, item->active);
}
}
