// Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "androiddebugsupport.h"

#include "androidconstants.h"
#include "androidglobal.h"
#include "androidrunner.h"
#include "androidmanager.h"
#include "androidqtversion.h"

#include <debugger/debuggerkitinformation.h>
#include <debugger/debuggerrunconfigurationaspect.h>
#include <debugger/debuggerruncontrol.h>

#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchain.h>

#include <qtsupport/qtkitinformation.h>

#include <utils/hostosinfo.h>

#include <QHostAddress>
#include <QJsonDocument>
#include <QLoggingCategory>

namespace {
static Q_LOGGING_CATEGORY(androidDebugSupportLog, "qtc.android.run.androiddebugsupport", QtWarningMsg)
}

using namespace Debugger;
using namespace ProjectExplorer;
using namespace Utils;

namespace Android {
namespace Internal {

static QStringList uniquePaths(const QStringList &files)
{
    QSet<QString> paths;
    for (const QString &file : files)
        paths << QFileInfo(file).absolutePath();
    return Utils::toList(paths);
}

static QStringList getSoLibSearchPath(const ProjectNode *node)
{
    if (!node)
        return {};

    QStringList res;
    node->forEachProjectNode([&res](const ProjectNode *node) {
         res.append(node->data(Constants::AndroidSoLibPath).toStringList());
    });

    const QString jsonFile = AndroidQtVersion::androidDeploymentSettings(
                node->getProject()->activeTarget()).toString();
    QFile deploymentSettings(jsonFile);
    if (deploymentSettings.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(deploymentSettings.readAll(), &error);
        if (error.error == QJsonParseError::NoError) {
            auto rootObj = doc.object();
            auto it = rootObj.find("stdcpp-path");
            if (it != rootObj.constEnd())
                res.append(QFileInfo(it.value().toString()).absolutePath());
        }
    }

    res.removeDuplicates();
    return res;
}

static QStringList getExtraLibs(const ProjectNode *node)
{
    if (!node)
        return {};
    return node->data(Android::Constants::AndroidExtraLibs).toStringList();
}

AndroidDebugSupport::AndroidDebugSupport(RunControl *runControl, const QString &intentName)
    : Debugger::DebuggerRunTool(runControl)
{
    setId("AndroidDebugger");
    setLldbPlatform("remote-android");
    m_runner = new AndroidRunner(runControl, intentName);
    addStartDependency(m_runner);
}

void AndroidDebugSupport::start()
{
    Target *target = runControl()->target();
    Kit *kit = target->kit();

    setStartMode(AttachToRemoteServer);
    const QString packageName = AndroidManager::packageName(target);
    setRunControlName(packageName);
    setUseContinueInsteadOfRun(true);
    setAttachPid(m_runner->pid());

    QtSupport::QtVersion *qtVersion = QtSupport::QtKitAspect::qtVersion(kit);
    if (!Utils::HostOsInfo::isWindowsHost()
        && (qtVersion
            && AndroidConfigurations::currentConfig().ndkVersion(qtVersion)
                   >= QVersionNumber(11, 0, 0))) {
        qCDebug(androidDebugSupportLog) << "UseTargetAsync: " << true;
        setUseTargetAsync(true);
    }

    if (isCppDebugging()) {
        qCDebug(androidDebugSupportLog) << "C++ debugging enabled";
        const ProjectNode *node = target->project()->findNodeForBuildKey(runControl()->buildKey());
        QStringList solibSearchPath = getSoLibSearchPath(node);
        QStringList extraLibs = getExtraLibs(node);
        if (qtVersion)
            solibSearchPath.append(qtVersion->qtSoPaths());
        solibSearchPath.append(uniquePaths(extraLibs));

        FilePath buildDir = AndroidManager::buildDirectory(target);
        const RunConfiguration *activeRunConfig = target->activeRunConfiguration();
        if (activeRunConfig)
            solibSearchPath.append(activeRunConfig->buildTargetInfo().workingDirectory.toString());
        solibSearchPath.append(buildDir.toString());
        const auto androidLibsPath = AndroidManager::androidBuildDirectory(target)
                                         .pathAppended("libs")
                                         .pathAppended(AndroidManager::apkDevicePreferredAbi(target))
                                         .toString();
        solibSearchPath.append(androidLibsPath);
        solibSearchPath.removeDuplicates();
        setSolibSearchPath(solibSearchPath);
        qCDebug(androidDebugSupportLog).noquote() << "SoLibSearchPath: " << solibSearchPath;
        setSymbolFile(buildDir.pathAppended("app_process"));
        setSkipExecutableValidation(true);
        setUseExtendedRemote(true);
        QString devicePreferredAbi = AndroidManager::apkDevicePreferredAbi(target);
        setAbi(AndroidManager::androidAbi2Abi(devicePreferredAbi));

        if (cppEngineType() == LldbEngineType) {
            setRemoteChannel("adb://" + AndroidManager::deviceSerialNumber(target),
                             m_runner->debugServerPort().number());
        } else {
            QUrl debugServer;
            debugServer.setPort(m_runner->debugServerPort().number());
            debugServer.setHost(QHostAddress(QHostAddress::LocalHost).toString());
            setRemoteChannel(debugServer);
        }

        auto qt = static_cast<AndroidQtVersion *>(qtVersion);
        const int minimumNdk = qt ? qt->minimumNDK() : 0;

        int sdkVersion = qMax(AndroidManager::minimumSDK(kit), minimumNdk);
        if (qtVersion) {
            const FilePath ndkLocation =
                    AndroidConfigurations::currentConfig().ndkLocation(qtVersion);
            Utils::FilePath sysRoot = ndkLocation
                    / "platforms"
                    / QString("android-%1").arg(sdkVersion)
                    / devicePreferredAbi; // Legacy Ndk structure
            if (!sysRoot.exists())
                sysRoot = AndroidConfig::toolchainPathFromNdk(ndkLocation) / "sysroot";
            setSysRoot(sysRoot);
            qCDebug(androidDebugSupportLog).noquote() << "Sysroot: " << sysRoot.toUserOutput();
        }
    }
    if (isQmlDebugging()) {
        qCDebug(androidDebugSupportLog) << "QML debugging enabled. QML server: "
                                        << m_runner->qmlServer().toDisplayString();
        setQmlServer(m_runner->qmlServer());
        //TODO: Not sure if these are the right paths.
        if (qtVersion)
            addSearchDirectory(qtVersion->qmlPath());
    }

    qCDebug(androidDebugSupportLog) << "Starting debugger - package name: " << packageName
                                    << ", PID: " << m_runner->pid().pid();
    DebuggerRunTool::start();
}

void AndroidDebugSupport::stop()
{
    qCDebug(androidDebugSupportLog) << "Stop";
    DebuggerRunTool::stop();
}

} // namespace Internal
} // namespace Android
