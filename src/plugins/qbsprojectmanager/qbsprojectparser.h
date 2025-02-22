// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "qbssession.h"

#include <utils/environment.h>

#include <QFutureInterface>
#include <QJsonObject>
#include <QObject>

namespace QbsProjectManager {
namespace Internal {

class QbsBuildSystem;

class QbsProjectParser : public QObject
{
    Q_OBJECT

public:
    QbsProjectParser(QbsBuildSystem *buildSystem, QFutureInterface<bool> *fi);
    ~QbsProjectParser() override;

    void parse(const QVariantMap &config, const Utils::Environment &env, const QString &dir,
               const QString &configName);
    void cancel();
    Utils::Environment environment() const { return m_environment; }

    QbsSession *session() const { return m_session; }
    QJsonObject projectData() const { return m_projectData; }
    ErrorInfo error() const { return m_error; }

signals:
    void done(bool success);

private:
    Utils::Environment m_environment;
    const QString m_projectFilePath;
    QbsSession * const m_session;
    ErrorInfo m_error;
    QJsonObject m_projectData;
    bool m_parsing = false;
    QFutureInterface<bool> *m_fi = nullptr;
};

} // namespace Internal
} // namespace QbsProjectManager
