/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <UI/Qt/Tab.h>
#include <UI/Qt/BrowserWindow.h>
#include <UI/Qt/StringUtils.h>
#include <UI/Qt/Thoth/SitePolicyStore.h>
#include <UI/Qt/Thoth/Localization.h>
#include <UI/Qt/Thoth/PageSleepInput.h>
#include <LibWebView/Application.h>
#include <LibWebView/CookieJar.h>
#include <LibWebView/StorageJar.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <LibWebCommon/StorageAPI/StorageEndpoint.h>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QUrl>

namespace Ladybird {
bool Tab::eventFilter(QObject* object, QEvent* event)
{
    // Conservatively protect pages the user interacted with. Scroll/move events
    // alone are harmless; text, paste, drop and clicks may change form/app state.
    if (object == m_view && Thoth::input_may_change_page(*event))
        m_has_user_input = true;
    return QWidget::eventFilter(object, event);
}

bool Tab::can_sleep() const
{
    return sleep_block_reason().isEmpty();
}

QString Tab::sleep_block_reason() const
{
    if (isVisible()) return "Visible page";
    if (is_loading()) return "Loading";
    if (m_has_user_input) return "Page interaction";
    if (m_microphone_granted) return "Microphone access";
    if (m_camera_granted) return "Camera access";
    if (view().needs_beforeunload_check()) return "Unload handler";
    if (view().can_undo()) return "Editing history";
    if (view().crash_overlay_active()) return "Crash recovery";
    if (view().audio_play_state() == Web::HTML::AudioPlayState::Playing) return "Playing audio";
    if (WebView::Application::the().file_downloader().has_active_downloads()) return "Active download";
    return {};
}

void Tab::show_site_settings()
{
    auto origin = qstring_from_ak_string(view().url().origin().serialize());
    auto supported = !Thoth::SitePolicyStore::canonical_origin(origin).isEmpty();
    auto url = view().url();
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("ThothSiteSettings");
    dialog->setWindowTitle("Site Settings");
    dialog->setMinimumWidth(430);
    auto* layout = new QVBoxLayout(dialog);
    auto* heading = new QLabel(supported ? origin : QString("This page has no configurable website origin."), dialog);
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    heading->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(heading);
    auto* form = new QFormLayout;
    auto* microphone = new QComboBox(dialog);
    microphone->setObjectName("ThothMicrophonePolicy");
    microphone->addItems({ "Ask Every Time", "Allow", "Block" });
    microphone->setCurrentIndex(static_cast<int>(m_site_policies->microphone(origin)));
    microphone->setEnabled(supported);
    form->addRow("Microphone", microphone);
    auto* camera = new QComboBox(dialog);
    camera->setObjectName("ThothCameraPolicy");
    camera->addItems({ "Ask Every Time", "Allow", "Block" });
    camera->setCurrentIndex(static_cast<int>(m_site_policies->camera(origin)));
    camera->setEnabled(supported);
    form->addRow("Camera", camera);
    form->addRow("Notifications", new QLabel("Not supported by this browser yet", dialog));
    layout->addLayout(form);
    auto* explanation = new QLabel(m_window->is_private() == WebView::IsPrivate::Yes
            ? "Private-window decisions stay in memory until the private session ends."
            : "Camera and microphone decisions are saved for this origin in this browser profile.", dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    auto* note = new QLabel("Changing or resetting a decision stops camera and microphone access and reloads this site's open pages. Location and autoplay controls are in Settings.", dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* reset = new QPushButton("Reset Site Permission", dialog);
    reset->setEnabled(supported);
    layout->addWidget(reset);
    auto* clear = new QPushButton("Clear Cookies and Web Storage…", dialog);
    clear->setObjectName("ThothClearSiteData");
    clear->setEnabled(supported);
    layout->addWidget(clear);
    auto* settings = new QPushButton("Location, Autoplay and Privacy Settings…", dialog);
    layout->addWidget(settings);
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(settings, &QPushButton::clicked, dialog, [this, dialog] {
        navigate_from_user_input("about:settings#permissions");
        dialog->close();
    });
    auto apply = [this, origin, url, microphone, camera, dialog](int device, Thoth::SitePolicyStore::Decision decision) {
        auto saved = device == 2 ? m_site_policies->reset_permissions(origin)
                                : device == 1 ? m_site_policies->set_camera(origin, decision)
                                              : m_site_policies->set_microphone(origin, decision);
        if (!saved) {
            QSignalBlocker microphone_blocker(microphone);
            QSignalBlocker camera_blocker(camera);
            microphone->setCurrentIndex(static_cast<int>(m_site_policies->microphone(origin)));
            camera->setCurrentIndex(static_cast<int>(m_site_policies->camera(origin)));
            QMessageBox::warning(dialog, Thoth::localized_text("Site Permissions"), Thoth::localized_text("Unable to save the site permission. The previous decision is unchanged."));
            return;
        }
        auto* session = &view().session();
        WebView::WebContentClient::for_each_client([&](auto& client) {
            if (&client.session() == session)
                client.revoke_audio_capture(url);
            return IterationDecision::Continue;
        });
        // Dismiss older prompts before they can overwrite the new decision.
        for (auto* widget : QApplication::allWidgets()) {
            auto* tab = qobject_cast<Tab*>(widget);
            if (!tab || &tab->view().session() != session || !tab->view().url().origin().is_same_origin(url.origin()))
                continue;
            ++tab->m_media_permission_generation;
            tab->m_microphone_granted = false;
            tab->m_camera_granted = false;
            tab->view().page().revoke_video_capture();
            auto pending = AK::move(tab->m_pending_permission_requests);
            for (auto& entry : pending) {
                if (entry.value.dialog)
                    entry.value.dialog->reject();
                for (auto& decided : entry.value.on_decided)
                    decided(false);
            }
            tab->view().reload();
        }
    };
    connect(microphone, &QComboBox::currentIndexChanged, dialog, [apply](int index) { apply(0, static_cast<Thoth::SitePolicyStore::Decision>(index)); });
    connect(camera, &QComboBox::currentIndexChanged, dialog, [apply](int index) { apply(1, static_cast<Thoth::SitePolicyStore::Decision>(index)); });
    connect(reset, &QPushButton::clicked, dialog, [microphone, camera, apply] {
        QSignalBlocker microphone_blocker(microphone);
        QSignalBlocker camera_blocker(camera);
        microphone->setCurrentIndex(0);
        camera->setCurrentIndex(0);
        apply(2, Thoth::SitePolicyStore::Decision::Ask);
    });
    connect(clear, &QPushButton::clicked, dialog, [this, url, dialog] {
        auto* confirmation = new QMessageBox(QMessageBox::Question, "Clear Site Data",
            "Remove this host's cookies and this origin's local/session storage? Open pages for this origin will reload. Shared-domain cookies may sign you out of related subdomains. IndexedDB and HTTP cache are not included.",
            QMessageBox::Yes | QMessageBox::Cancel, dialog);
        confirmation->setAttribute(Qt::WA_DeleteOnClose);
        confirmation->setDefaultButton(QMessageBox::Cancel);
        connect(confirmation, &QMessageBox::finished, dialog, [this, url](int result) {
            if (result != QMessageBox::Yes)
                return;
            auto& session = view().session();
            auto host = QUrl(qstring_from_ak_string(url.serialize())).host();
            for (auto const& cookie : session.cookie_jar->get_all_cookies()) {
                auto domain = qstring_from_ak_string(cookie.domain);
                if (domain.startsWith('.'))
                    domain.remove(0, 1);
                if (host.compare(domain, Qt::CaseInsensitive) == 0 || host.endsWith('.' + domain, Qt::CaseInsensitive))
                    session.cookie_jar->delete_cookie({ cookie.name, cookie.domain, cookie.path, cookie.partition_key });
            }
            auto key = url.origin().serialize();
            auto endpoint = Web::StorageAPI::StorageEndpointType::LocalStorage;
            session.storage_jar->clear_storage_key(endpoint, key);
            WebView::ViewImplementation::for_each_view([&](auto& page) {
                if (&page.session() == &session && page.url().origin().is_same_origin(url.origin())) {
                    page.clear_session_storage();
                    page.notify_storage_changed({ endpoint, key, DevTools::DevToolsDelegate::StorageChange::Type::Cleared, {} });
                    page.reload();
                }
                return IterationDecision::Continue;
            });
        });
        confirmation->open();
    });
    Thoth::localize_widget_tree(*dialog);
    dialog->open();
}
}
