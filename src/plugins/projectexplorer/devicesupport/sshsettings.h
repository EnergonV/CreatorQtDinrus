// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.h"

#include <utils/fileutils.h>

#include <functional>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT SshSettings
{
public:
    static void loadSettings(QSettings *settings);
    static void storeSettings(QSettings *settings);

    static void setConnectionSharingEnabled(bool share);
    static bool connectionSharingEnabled();

    static void setConnectionSharingTimeout(int timeInMinutes);
    static int connectionSharingTimeout();

    static void setSshFilePath(const Utils::FilePath &ssh);
    static Utils::FilePath sshFilePath();

    static void setSftpFilePath(const Utils::FilePath &sftp);
    static Utils::FilePath sftpFilePath();

    static void setAskpassFilePath(const Utils::FilePath &askPass);
    static Utils::FilePath askpassFilePath();

    static void setKeygenFilePath(const Utils::FilePath &keygen);
    static Utils::FilePath keygenFilePath();

    using SearchPathRetriever = std::function<Utils::FilePaths()>;
    static void setExtraSearchPathRetriever(const SearchPathRetriever &pathRetriever);
};

} // namespace ProjectExplorer
