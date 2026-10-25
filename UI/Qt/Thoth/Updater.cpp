/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "Updater.h"
#include "Localization.h"
#include <AK/Platform.h>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#if defined(AK_OS_WINDOWS) && defined(THOTH_UPDATE_CHECK_URL)

// NB: The Windows updater checks the project's GitHub releases for a newer
//     published (non-prerelease) release carrying a Windows installer, downloads
//     it to the temporary directory and runs it. Sparkle keeps doing signed
//     updates on macOS; this keeps the two platforms' update flows separate.

namespace {

struct AvailableUpdate {
    QString version;
    QUrl installer_url;
};

QString normalize_version(QString tag)
{
    return tag.startsWith('v') ? tag.mid(1) : tag;
}

QVector<int> version_components(QString const& version)
{
    QVector<int> components;
    for (auto const& part : version.split('.'))
        components.append(part.toInt());
    while (components.size() < 3)
        components.append(0);
    return components;
}

bool is_newer_version(QString const& candidate, QString const& current)
{
    auto candidate_components = version_components(candidate);
    auto current_components = version_components(current);
    for (auto i = 0; i < 3; ++i) {
        if (candidate_components[i] != current_components[i])
            return candidate_components[i] > current_components[i];
    }
    return false;
}

bool installer_asset_name_matches(QString const& name)
{
    return name.startsWith("ThothBrowser-") && name.endsWith("-win64-setup.exe");
}

AvailableUpdate parse_releases(QByteArray const& payload, QString const& current_version)
{
    auto document = QJsonDocument::fromJson(payload);
    if (!document.isArray())
        return {};
    for (auto const& release_value : document.array()) {
        auto release = release_value.toObject();
        if (release.value("draft").toBool() || release.value("prerelease").toBool())
            continue;
        auto version = normalize_version(release.value("tag_name").toString());
        if (version.isEmpty() || !is_newer_version(version, current_version))
            continue;
        for (auto const& asset_value : release.value("assets").toArray()) {
            auto asset = asset_value.toObject();
            auto const& name = asset.value("name").toString();
            if (!installer_asset_name_matches(name))
                continue;
            auto url = QUrl(asset.value("browser_download_url").toString());
            if (url.isValid())
                return { version, url };
        }
    }
    return {};
}

class UpdateController final : public QObject {
public:
    static UpdateController& the()
    {
        static UpdateController controller;
        return controller;
    }

    void set_restart_guards(std::function<bool()> prepare_restart, std::function<void()> restart_cancelled)
    {
        m_prepare_restart = move(prepare_restart);
        m_restart_cancelled = move(restart_cancelled);
    }

    void check(bool interactive)
    {
        if (m_check_reply)
            return;

        m_check_interactive = interactive;
        QNetworkRequest request { QUrl(THOTH_UPDATE_CHECK_URL) };
        request.setRawHeader("User-Agent", "ThothBrowser-Updater/" THOTH_VERSION);
        request.setRawHeader("Accept", "application/vnd.github+json");
        m_check_reply = manager().get(request);
        QObject::connect(m_check_reply, &QNetworkReply::finished, [this] {
            auto* reply = m_check_reply;
            m_check_reply = nullptr;
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (m_check_interactive)
                    show_message(QMessageBox::Warning, Thoth::localized_text("Update Check Failed"),
                        Thoth::localized_text("Could not reach the update server. Check your connection and try again."));
                return;
            }

            auto update = parse_releases(reply->readAll(), QLatin1String(THOTH_VERSION));
            if (update.version.isEmpty()) {
                if (m_check_interactive)
                    show_message(QMessageBox::Information, Thoth::localized_text("Up to Date"),
                        Thoth::localized_text("You are running the latest version of Thoth Browser."));
                return;
            }

            auto* prompt = new QMessageBox(QMessageBox::Question, Thoth::localized_text("Update Available"),
                Thoth::localized_text("Thoth Browser %1 is available. Download and install it now?").arg(update.version),
                QMessageBox::Yes | QMessageBox::No, QApplication::activeWindow());
            prompt->setAttribute(Qt::WA_DeleteOnClose);
            (void)prompt->addButton(Thoth::localized_text("Download and Install"), QMessageBox::YesRole);
            (void)prompt->addButton(Thoth::localized_text("Later"), QMessageBox::NoRole);
            QObject::connect(prompt, &QMessageBox::finished, prompt, [this, update](int result) {
                if (result == QMessageBox::Yes)
                    download(update);
            });
            prompt->open();
        });
    }

private:
    static QNetworkAccessManager& manager()
    {
        static QNetworkAccessManager manager;
        return manager;
    }

    void show_message(QMessageBox::Icon icon, QString const& title, QString const& text)
    {
        auto* box = new QMessageBox(icon, title, text, QMessageBox::NoButton, QApplication::activeWindow());
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }

    void download(AvailableUpdate const& update)
    {
        if (m_download_reply)
            return;

        auto installer_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + "/ThothBrowser-" + update.version + "-setup.exe";
        m_installer_file.setFileName(installer_path);
        if (!m_installer_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            show_message(QMessageBox::Warning, Thoth::localized_text("Update Failed"),
                Thoth::localized_text("Could not create the download file."));
            return;
        }

        auto* progress = new QProgressDialog(Thoth::localized_text("Downloading Thoth Browser %1…").arg(update.version),
            Thoth::localized_text("Cancel"), 0, 100, QApplication::activeWindow());
        progress->setWindowTitle(Thoth::localized_text("Downloading Update"));
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setAutoClose(false);

        QNetworkRequest request { update.installer_url };
        request.setRawHeader("User-Agent", "ThothBrowser-Updater/" THOTH_VERSION);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        m_download_reply = manager().get(request);

        QObject::connect(m_download_reply, &QNetworkReply::downloadProgress, progress, [progress](qint64 received, qint64 total) {
            if (total > 0)
                progress->setValue(static_cast<int>((received * 100) / total));
            else
                progress->setRange(0, 0);
        });
        QObject::connect(m_download_reply, &QNetworkReply::readyRead, [this] {
            m_installer_file.write(m_download_reply->readAll());
        });
        QObject::connect(progress, &QProgressDialog::canceled, [this] {
            m_download_reply->abort();
        });
        QObject::connect(m_download_reply, &QNetworkReply::finished, [this, progress, update] {
            auto* reply = m_download_reply;
            m_download_reply = nullptr;
            reply->deleteLater();
            progress->reset();
            progress->deleteLater();

            auto aborted = reply->error() == QNetworkReply::OperationCanceledError;
            m_installer_file.write(reply->readAll());
            m_installer_file.close();

            if (aborted)
                return;
            if (reply->error() != QNetworkReply::NoError || m_installer_file.size() == 0) {
                m_installer_file.remove();
                show_message(QMessageBox::Warning, Thoth::localized_text("Update Failed"),
                    Thoth::localized_text("The update could not be downloaded. Try again later."));
                return;
            }

            // Hand the machine to the installer only once quitting cannot lose anything.
            auto restart_ok = !m_prepare_restart || m_prepare_restart();
            if (!restart_ok) {
                if (m_restart_cancelled)
                    m_restart_cancelled();
                show_message(QMessageBox::Information, Thoth::localized_text("Update Ready"),
                    Thoth::localized_text("The installer was saved to %1. Run it after your downloads finish.").arg(m_installer_file.fileName()));
                return;
            }

            if (!QProcess::startDetached(m_installer_file.fileName(), {})) {
                show_message(QMessageBox::Warning, Thoth::localized_text("Update Failed"),
                    Thoth::localized_text("The installer could not be started."));
                return;
            }
            QApplication::quit();
        });
    }

    std::function<bool()> m_prepare_restart;
    std::function<void()> m_restart_cancelled;
    QNetworkReply* m_check_reply { nullptr };
    QNetworkReply* m_download_reply { nullptr };
    QFile m_installer_file;
    bool m_check_interactive { false };
};

}

namespace Thoth {

void add_update_menu(QMenu& menu, std::function<bool()> prepare_restart, std::function<void()> restart_cancelled)
{
    UpdateController::the().set_restart_guards(move(prepare_restart), move(restart_cancelled));

    auto* action = menu.addAction(localized_text("Check for Updates…"));
    QObject::connect(action, &QAction::triggered, [] {
        UpdateController::the().check(true);
    });

    // Check quietly once per session, shortly after startup.
    QTimer::singleShot(5000, [] {
        UpdateController::the().check(false);
    });
}

}

#else

namespace Thoth {

void add_update_menu(QMenu& menu, std::function<bool()>, std::function<void()>)
{
    auto* action = menu.addAction(localized_text("Automatic updates are currently available on macOS."));
    action->setEnabled(false);
}

}

#endif
