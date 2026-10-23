/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

namespace Thoth {
class SitePolicyStore {
public:
    enum class Decision { Ask, Allow, Block };
    explicit SitePolicyStore(QString path = {});
    static QSharedPointer<SitePolicyStore> shared(QString const& session_key, QString const& path);
    Decision microphone(QString const& origin) const;
    bool set_microphone(QString const& origin, Decision);
    Decision camera(QString const& origin) const;
    bool set_camera(QString const& origin, Decision);
    bool reset_permissions(QString const& origin);
    static QString canonical_origin(QString const&);

private:
    QString m_path;
    Decision decision(QJsonObject const&, QString const& origin) const;
    bool set_decision(QString const& origin, Decision, bool camera);
    bool save(QJsonObject const& microphone, QJsonObject const& camera);
    QJsonObject m_decisions;
    QJsonObject m_camera_decisions;
};
}
