// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.h"
#include "baseqtversion.h"

namespace QtSupport {

class QTSUPPORT_EXPORT QtVersionManager : public QObject
{
    Q_OBJECT
    // for getUniqueId();
    friend class QtVersion;
    friend class QtVersionFactory;
    friend class Internal::QtOptionsPageWidget;

public:
    static QtVersionManager *instance();
    QtVersionManager();
    ~QtVersionManager() override;
    static void initialized();

    static bool isLoaded();

    // This will *always* return at least one (Qt in Path), even if that is
    // unconfigured. The lists here are in load-time order! Use sortVersions(...) if you
    // need a list sorted by Qt Version number.
    //
    // Note: DO NOT STORE THESE POINTERS!
    //       The QtVersionManager may delete them at random times and you will
    //       need to get a new pointer by calling this function again!
    static QtVersions versions(const QtVersion::Predicate &predicate = {});
    static QtVersion *version(int id);
    static QtVersion *version(const QtVersion::Predicate &predicate);

    // Sorting is potentially expensive since it might require qmake --query to run for each version!
    static QtVersions sortVersions(const QtVersions &input);

    static void addVersion(QtVersion *version);
    static void removeVersion(QtVersion *version);

    // Call latest in extensionsInitialized of plugin depending on QtSupport
    static void registerExampleSet(const QString &displayName,
                                   const QString &manifestPath,
                                   const QString &examplesPath);

signals:
    // content of QtVersion objects with qmake path might have changed
    void qtVersionsChanged(const QList<int> &addedIds, const QList<int> &removedIds, const QList<int> &changedIds);
    void qtVersionsLoaded();

private:
    enum class DocumentationSetting { HighestOnly, All, None };

    static void updateDocumentation(const QtVersions &added,
                                    const QtVersions &removed,
                                    const QtVersions &allNew);
    void updateFromInstaller(bool emitSignal = true);
    void triggerQtVersionRestore();

    // Used by QtOptionsPage
    static void setNewQtVersions(const QtVersions &newVersions);
    static void setDocumentationSetting(const DocumentationSetting &setting);
    static DocumentationSetting documentationSetting();
    // Used by QtVersion
    static int getUniqueId();
};

} // namespace QtSupport
