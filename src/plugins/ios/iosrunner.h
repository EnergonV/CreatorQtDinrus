// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "iosconfigurations.h"
#include "iostoolhandler.h"
#include "iossimulator.h"

#include <debugger/debuggerconstants.h>
#include <debugger/debuggerruncontrol.h>

#include <projectexplorer/devicesupport/idevicefwd.h>
#include <projectexplorer/runconfiguration.h>

#include <qmldebug/qmldebugcommandlinearguments.h>
#include <qmldebug/qmloutputparser.h>

namespace Ios {
namespace Internal {

class IosRunner : public ProjectExplorer::RunWorker
{
    Q_OBJECT

public:
    IosRunner(ProjectExplorer::RunControl *runControl);
    ~IosRunner() override;

    void setCppDebugging(bool cppDebug);
    void setQmlDebugging(QmlDebug::QmlDebugServicesPreset qmlDebugServices);

    QString bundlePath();
    QString deviceId();
    IosToolHandler::RunKind runType();
    bool cppDebug() const;
    bool qmlDebug() const;
    QmlDebug::QmlDebugServicesPreset qmlDebugServices() const;

    void start() override;
    void stop() final;

    virtual void appOutput(const QString &/*output*/) {}
    virtual void errorMsg(const QString &/*msg*/) {}
    virtual void onStart() { reportStarted(); }

    Utils::Port qmlServerPort() const;
    Utils::Port gdbServerPort() const;
    qint64 pid() const;
    bool isAppRunning() const;

private:
    void handleGotServerPorts(Ios::IosToolHandler *handler, const QString &bundlePath,
                              const QString &deviceId, Utils::Port gdbPort, Utils::Port qmlPort);
    void handleGotInferiorPid(Ios::IosToolHandler *handler, const QString &bundlePath,
                              const QString &deviceId, qint64 pid);
    void handleAppOutput(Ios::IosToolHandler *handler, const QString &output);
    void handleErrorMsg(Ios::IosToolHandler *handler, const QString &msg);
    void handleToolExited(Ios::IosToolHandler *handler, int code);
    void handleFinished(Ios::IosToolHandler *handler);

    IosToolHandler *m_toolHandler = nullptr;
    QString m_bundleDir;
    ProjectExplorer::IDeviceConstPtr m_device;
    IosDeviceType m_deviceType;
    bool m_cppDebug = false;
    QmlDebug::QmlDebugServicesPreset m_qmlDebugServices = QmlDebug::NoQmlDebugServices;

    bool m_cleanExit = false;
    Utils::Port m_qmlServerPort;
    Utils::Port m_gdbServerPort;
    qint64 m_pid = 0;
};


class IosRunSupport : public IosRunner
{
    Q_OBJECT

public:
    explicit IosRunSupport(ProjectExplorer::RunControl *runControl);
    ~IosRunSupport() override;

    void didStartApp(IosToolHandler::OpStatus status);
private:
    void start() override;
};


class IosQmlProfilerSupport : public ProjectExplorer::RunWorker
{
    Q_OBJECT

public:
    IosQmlProfilerSupport(ProjectExplorer::RunControl *runControl);

private:
    void start() override;
    IosRunner *m_runner = nullptr;
    ProjectExplorer::RunWorker *m_profiler = nullptr;
};


class IosDebugSupport : public Debugger::DebuggerRunTool
{
    Q_OBJECT

public:
    IosDebugSupport(ProjectExplorer::RunControl *runControl);

private:
    void start() override;

    const QString m_dumperLib;
    IosRunner *m_runner;
};

} // namespace Internal
} // namespace Ios
