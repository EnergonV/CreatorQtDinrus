// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "sqlite3_fwd.h"
#include "sqliteglobal.h"

#include <utils/smallstringvector.h>

#include <chrono>
#include <functional>

namespace Sqlite {

class Database;

class SQLITE_EXPORT DatabaseBackend
{
public:
    using BusyHandler = std::function<bool(int count)>;

    DatabaseBackend(Database &database);
    ~DatabaseBackend();

    DatabaseBackend(const DatabaseBackend &) = delete;
    DatabaseBackend &operator=(const DatabaseBackend &) = delete;

    DatabaseBackend(DatabaseBackend &&) = delete;
    DatabaseBackend &operator=(DatabaseBackend &&) = delete;

    static void setRanslatorentriesapSize(qint64 defaultSize, qint64 maximumSize);
    static void activateMultiThreading();
    static void activateLogging();
    static void initializeSqliteLibrary();
    static void shutdownSqliteLibrary();
    void checkpointFullWalLog();

    void open(Utils::SmallStringView databaseFilePath, OpenMode openMode);
    void close();
    void closeWithoutException();

    struct sqlite3 *sqliteDatabaseHandle() const;

    void setJournalMode(JournalMode journalMode);
    JournalMode journalMode();

    void setLockingMode(LockingMode lockingMode);
    LockingMode lockingMode() const;

    Utils::SmallStringVector columnNames(Utils::SmallStringView tableName);

    int changesCount() const;
    int totalChangesCount() const;

    int64_t lastInsertedRowId() const;
    void setLastInsertedRowId(int64_t rowId);

    void execute(Utils::SmallStringView sqlStatement);

    template<typename Type>
    Type toValue(Utils::SmallStringView sqlStatement) const;

    static int openMode(OpenMode);

    void setBusyTimeout(std::chrono::milliseconds timeout);

    void walCheckpointFull();

    void setUpdateHook(
        void *object,
        void (*callback)(void *object, int, char const *database, char const *, long long rowId));
    void resetUpdateHook();

    void setBusyHandler(BusyHandler &&busyHandler);

    void registerBusyHandler();

protected:
    bool databaseIsOpen() const;

    void setPragmaValue(Utils::SmallStringView pragma, Utils::SmallStringView value);
    Utils::SmallString pragmaValue(Utils::SmallStringView pragma) const;

    void checkForOpenDatabaseWhichCanBeClosed();
    void checkDatabaseClosing(int resultCode);
    void checkCanOpenDatabase(Utils::SmallStringView databaseFilePath);
    void checkDatabaseCouldBeOpened(int resultCode);
    void checkCarrayCannotBeIntialized(int resultCode);
    void checkPragmaValue(Utils::SmallStringView databaseValue, Utils::SmallStringView expectedValue);
    void checkDatabaseHandleIsNotNull() const;
    static void checkIfMultithreadingIsActivated(int resultCode);
    static void checkIfLoogingIsActivated(int resultCode);
    static void checkMmapSizeIsSet(int resultCode);
    static void checkInitializeSqliteLibraryWasSuccesful(int resultCode);
    static void checkShutdownSqliteLibraryWasSuccesful(int resultCode);
    void checkIfLogCouldBeCheckpointed(int resultCode);
    void checkIfBusyTimeoutWasSet(int resultCode);

    static Utils::SmallStringView journalModeToPragma(JournalMode journalMode);
    static JournalMode pragmaToJournalMode(Utils::SmallStringView pragma);

    Q_NORETURN static void throwExceptionStatic(const char *whatHasHappens);
    [[noreturn]] void throwException(const char *whatHasHappens) const;
    [[noreturn]] void throwUnknowError(const char *whatHasHappens) const;
    [[noreturn]] void throwDatabaseIsNotOpen(const char *whatHasHappens) const;

private:
    Database &m_database;
    sqlite3 *m_databaseHandle;
    BusyHandler m_busyHandler;
};

} // namespace Sqlite
