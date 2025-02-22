// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.h>

namespace QmlProfiler {
namespace Internal {

class QmlProfilerSettings;

class QmlProfilerPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QmlProfiler.json")

public:
    static QmlProfilerSettings *globalSettings();

private:
    bool initialize(const QStringList &arguments, QString *errorString) final;
    void extensionsInitialized() final;
    ShutdownFlag aboutToShutdown() final;
    QVector<QObject *> createTestObjects() const final;

    class QmlProfilerPluginPrivate *d = nullptr;
};

} // namespace Internal
} // namespace QmlProfiler
