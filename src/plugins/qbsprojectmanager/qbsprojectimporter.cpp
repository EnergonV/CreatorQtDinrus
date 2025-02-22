// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qbsprojectimporter.h"

#include "qbspmlogging.h"
#include "qbssession.h"

#include <coreplugin/documentmanager.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildinfo.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/toolchain.h>
#include <qtsupport/qtkitinformation.h>
#include <utils/filepath.h>
#include <utils/hostosinfo.h>

#include <QFileInfo>

using namespace ProjectExplorer;
using namespace Utils;

namespace QbsProjectManager {
namespace Internal {

struct BuildGraphData
{
    FilePath bgFilePath;
    QVariantMap overriddenProperties;
    FilePath cCompilerPath;
    FilePath cxxCompilerPath;
    FilePath qtBinPath;
    FilePath sysroot;
    QString buildVariant;
};
static BuildGraphData extractBgData(const QbsSession::BuildGraphInfo &bgInfo)
{
    BuildGraphData bgData;
    bgData.bgFilePath = bgInfo.bgFilePath;
    bgData.overriddenProperties = bgInfo.overriddenProperties;
    const QVariantMap &moduleProps = bgInfo.requestedProperties;
    const QVariantMap prjCompilerPathByLanguage
            = moduleProps.value("cpp.compilerPathByLanguage").toMap();
    const QString prjCompilerPath = moduleProps.value("cpp.compilerPath").toString();
    const QStringList prjToolchain = moduleProps.value("qbs.toolchain").toStringList();
    const bool prjIsMsvc = prjToolchain.contains("msvc");
    bgData.cCompilerPath = FilePath::fromString(
                prjIsMsvc ? prjCompilerPath : prjCompilerPathByLanguage.value("c").toString());
    bgData.cxxCompilerPath = FilePath::fromString(
                prjIsMsvc ? prjCompilerPath : prjCompilerPathByLanguage.value("cpp").toString());
    bgData.qtBinPath = FilePath::fromString(moduleProps.value("Qt.core.binPath").toString());
    bgData.sysroot = FilePath::fromString(moduleProps.value("qbs.sysroot").toString());
    bgData.buildVariant = moduleProps.value("qbs.buildVariant").toString();
    return bgData;
}

QbsProjectImporter::QbsProjectImporter(const FilePath &path) : QtProjectImporter(path)
{
}

static FilePath buildDir(const FilePath &projectFilePath, const Kit *k)
{
    const QString projectName = projectFilePath.completeBaseName();
    return BuildConfiguration::buildDirectoryFromTemplate(
                Project::projectDirectory(projectFilePath),
                projectFilePath, projectName, k, QString(), BuildConfiguration::Unknown, "qbs");
}

static bool hasBuildGraph(const QString &dir)
{
    const QString &dirName = QFileInfo(dir).fileName();
    return QFileInfo::exists(dir + QLatin1Char('/') + dirName + QLatin1String(".bg"));
}

static QStringList candidatesForDirectory(const QString &dir)
{
    QStringList candidates;
    const QStringList &subDirs = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subDir : subDirs) {
        const QString absSubDir = dir + QLatin1Char('/') + subDir;
        if (hasBuildGraph(absSubDir))
            candidates << absSubDir;
    }
    return candidates;
}

QStringList QbsProjectImporter::importCandidates()
{
    const QString projectDir = projectFilePath().toFileInfo().absolutePath();
    QStringList candidates = candidatesForDirectory(projectDir);

    QSet<QString> seenCandidates;
    seenCandidates.insert(projectDir);
    const auto &kits = KitManager::kits();
    for (Kit * const k : kits) {
        QFileInfo fi = buildDir(projectFilePath(), k).toFileInfo();
        const QString candidate = fi.absolutePath();
        if (!seenCandidates.contains(candidate)) {
            seenCandidates.insert(candidate);
            candidates << candidatesForDirectory(candidate);
        }
    }
    qCDebug(qbsPmLog) << "build directory candidates:" << candidates;
    return candidates;
}

QList<void *> QbsProjectImporter::examineDirectory(const FilePath &importPath,
                                                   QString *warningMessage) const
{
    Q_UNUSED(warningMessage)
    qCDebug(qbsPmLog) << "examining build directory" << importPath.toUserOutput();
    QList<void *> data;
    const FilePath bgFilePath = importPath.pathAppended(importPath.fileName() + ".bg");
    const QStringList relevantProperties({
            "qbs.buildVariant", "qbs.sysroot", "qbs.toolchain",
            "cpp.compilerPath", "cpp.compilerPathByLanguage",
            "Qt.core.binPath"
    });

    const QbsSession::BuildGraphInfo bgInfo = QbsSession::getBuildGraphInfo(
                bgFilePath, relevantProperties);
    if (bgInfo.error.hasError()) {
        qCDebug(qbsPmLog) << "error getting build graph info:" << bgInfo.error.toString();
        return data;
    }
    qCDebug(qbsPmLog) << "retrieved build graph info:" << bgInfo.requestedProperties;
    data << new BuildGraphData(extractBgData(bgInfo));
    return data;
}

bool QbsProjectImporter::matchKit(void *directoryData, const Kit *k) const
{
    const auto * const bgData = static_cast<BuildGraphData *>(directoryData);
    qCDebug(qbsPmLog) << "matching kit" << k->displayName() << "against imported build"
                      << bgData->bgFilePath.toUserOutput();
    if (ToolChainKitAspect::toolChains(k).isEmpty() && bgData->cCompilerPath.isEmpty()
            && bgData->cxxCompilerPath.isEmpty()) {
        return true;
    }
    const ToolChain * const cToolchain = ToolChainKitAspect::cToolChain(k);
    const ToolChain * const cxxToolchain = ToolChainKitAspect::cxxToolChain(k);
    if (!bgData->cCompilerPath.isEmpty()) {
        if (!cToolchain)
            return false;
        if (bgData->cCompilerPath != cToolchain->compilerCommand())
            return false;
    }
    if (!bgData->cxxCompilerPath.isEmpty()) {
        if (!cxxToolchain)
            return false;
        if (bgData->cxxCompilerPath != cxxToolchain->compilerCommand())
            return false;
    }
    const QtSupport::QtVersion * const qtVersion = QtSupport::QtKitAspect::qtVersion(k);
    if (!bgData->qtBinPath.isEmpty()) {
        if (!qtVersion)
            return false;
        if (bgData->qtBinPath != qtVersion->hostBinPath())
            return false;
    }
    if (bgData->sysroot != SysRootKitAspect::sysRoot(k))
        return false;

    qCDebug(qbsPmLog) << "Kit matches";
    return true;
}

Kit *QbsProjectImporter::createKit(void *directoryData) const
{
    const auto * const bgData = static_cast<BuildGraphData *>(directoryData);
    qCDebug(qbsPmLog) << "creating kit for imported build" << bgData->bgFilePath.toUserOutput();
    QtVersionData qtVersionData;
    if (!bgData->qtBinPath.isEmpty()) {
        const FilePath qmakeFilePath = bgData->qtBinPath.pathAppended("qmake").withExecutableSuffix();
        qtVersionData = findOrCreateQtVersion(qmakeFilePath);
    }
    return createTemporaryKit(qtVersionData,[this, bgData](Kit *k) -> void {
        QList<ToolChainData> tcData;
        if (!bgData->cxxCompilerPath.isEmpty())
            tcData << findOrCreateToolChains({bgData->cxxCompilerPath, Constants::CXX_LANGUAGE_ID});
        if (!bgData->cCompilerPath.isEmpty())
            tcData << findOrCreateToolChains({bgData->cCompilerPath, Constants::C_LANGUAGE_ID});
        foreach (const ToolChainData &tc, tcData) {
            if (!tc.tcs.isEmpty())
                ToolChainKitAspect::setToolChain(k, tc.tcs.first());
        }
        SysRootKitAspect::setSysRoot(k, bgData->sysroot);
    });
}

const QList<BuildInfo> QbsProjectImporter::buildInfoList(void *directoryData) const
{
    const auto * const bgData = static_cast<BuildGraphData *>(directoryData);
    BuildInfo info;
    info.displayName = bgData->bgFilePath.completeBaseName();
    info.buildType = bgData->buildVariant == "debug"
            ? BuildConfiguration::Debug : BuildConfiguration::Release;
    info.buildDirectory = bgData->bgFilePath.parentDir().parentDir();
    QVariantMap config = bgData->overriddenProperties;
    config.insert("configName", info.displayName);
    info.extraInfo = config;
    qCDebug(qbsPmLog) << "creating build info for " << info.displayName << ' ' << bgData->buildVariant;
    return {info};
}

void QbsProjectImporter::deleteDirectoryData(void *directoryData) const
{
    delete static_cast<BuildGraphData *>(directoryData);
}

} // namespace Internal
} // namespace QbsProjectManager
