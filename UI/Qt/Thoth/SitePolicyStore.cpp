/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "SitePolicyStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUrl>
#include <QWeakPointer>

namespace Thoth {
QString SitePolicyStore::canonical_origin(QString const& text)
{
    QUrl url(text);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http") || url.host().isEmpty() || !url.userInfo().isEmpty())
        return {};
    url.setPath({});
    url.setQuery({});
    url.setFragment({});
    if ((url.scheme() == "https" && url.port() == 443) || (url.scheme() == "http" && url.port() == 80))
        url.setPort(-1);
    return url.toString(QUrl::FullyEncoded);
}

SitePolicyStore::SitePolicyStore(QString path)
    : m_path(std::move(path))
{
    QFile file(m_path);
    if (m_path.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        return;
    auto document = QJsonDocument::fromJson(file.readAll()).object();
    if (document["schema"].toInt() != 1)
        return;
    auto load_decisions = [&](QString const& name, QJsonObject& target) {
        auto decisions = document[name].toObject();
        for (auto it = decisions.begin(); it != decisions.end(); ++it) {
            if (canonical_origin(it.key()) == it.key() && !it.key().isEmpty() && it.value().isBool())
                target[it.key()] = it.value();
        }
    };
    load_decisions("microphone", m_decisions);
    load_decisions("camera", m_camera_decisions);
}

QSharedPointer<SitePolicyStore> SitePolicyStore::shared(QString const& key, QString const& path)
{
    static QHash<QString, QWeakPointer<SitePolicyStore>> stores;
    auto store = stores.value(key).toStrongRef();
    if (!store) {
        store = QSharedPointer<SitePolicyStore>::create(path);
        stores[key] = store;
    }
    return store;
}

SitePolicyStore::Decision SitePolicyStore::decision(QJsonObject const& decisions, QString const& origin) const
{
    auto key = canonical_origin(origin);
    if (key.isEmpty() || !decisions.contains(key))
        return Decision::Ask;
    return decisions[key].toBool() ? Decision::Allow : Decision::Block;
}

SitePolicyStore::Decision SitePolicyStore::microphone(QString const& origin) const
{
    return decision(m_decisions, origin);
}

SitePolicyStore::Decision SitePolicyStore::camera(QString const& origin) const
{
    return decision(m_camera_decisions, origin);
}

bool SitePolicyStore::set_microphone(QString const& origin, Decision decision)
{
    return set_decision(origin, decision, false);
}

bool SitePolicyStore::set_camera(QString const& origin, Decision decision)
{
    return set_decision(origin, decision, true);
}

bool SitePolicyStore::set_decision(QString const& origin, Decision decision, bool camera)
{
    auto key = canonical_origin(origin);
    if (key.isEmpty())
        return false;
    auto microphone_decisions = m_decisions;
    auto camera_decisions = m_camera_decisions;
    auto& next = camera ? camera_decisions : microphone_decisions;
    if (decision == Decision::Ask)
        next.remove(key);
    else
        next[key] = decision == Decision::Allow;
    return save(microphone_decisions, camera_decisions);
}

bool SitePolicyStore::reset_permissions(QString const& origin)
{
    auto key = canonical_origin(origin);
    if (key.isEmpty())
        return false;
    auto microphone_decisions = m_decisions;
    auto camera_decisions = m_camera_decisions;
    microphone_decisions.remove(key);
    camera_decisions.remove(key);
    return save(microphone_decisions, camera_decisions);
}

bool SitePolicyStore::save(QJsonObject const& microphone, QJsonObject const& camera)
{
    if (!m_path.isEmpty()) {
        if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
            return false;
        QSaveFile file(m_path);
        auto bytes = QJsonDocument(QJsonObject { { "schema", 1 }, { "microphone", microphone }, { "camera", camera } }).toJson();
        if (bytes.size() > 1024 * 1024 || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return false;
    }
    m_decisions = microphone;
    m_camera_decisions = camera;
    return true;
}
}
