// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "filestatuscache.h"
#include "filesysteminterface.h"
#include "nonlockingmutex.h"

namespace Sqlite {
class Database;
}

namespace QmlDesigner {

template<typename ProjectStorage, typename Mutex>
class SourcePathCache;

template<typename Database>
class ProjectStorage;

using PathCache = SourcePathCache<ProjectStorage<Sqlite::Database>, NonLockingMutex>;

class FileSystem : public FileSystemInterface
{
public:
    FileSystem(PathCache &sourcePathCache)
        : m_sourcePathCache(sourcePathCache)
    {}

    SourceIds directoryEntries(const QString &directoryPath) const override;
    long long lastModified(SourceId sourceId) const override;
    FileStatus fileStatus(SourceId sourceId) const override;
    QString contentAsQString(const QString &filePath) const override;

    void remove(const SourceIds &sourceIds) override;

private:
    PathCache &m_sourcePathCache;
};

} // namespace QmlDesigner
