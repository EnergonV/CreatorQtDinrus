// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "googletest.h"

#include "spydummy.h"

#include <sqlitedatabase.h>
#include <sqlitereadstatement.h>
#include <sqlitetable.h>
#include <sqlitewritestatement.h>

#include <utils/temporarydirectory.h>

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QVariant>

#include <functional>

namespace {

using testing::Contains;

using Sqlite::ColumnType;
using Sqlite::JournalMode;
using Sqlite::OpenMode;
using Sqlite::Table;

class SqliteDatabase : public ::testing::Test
{
protected:
    SqliteDatabase()
    {
        database.lock();
        database.setJournalMode(JournalMode::Memory);
        database.setDatabaseFilePath(databaseFilePath);
        Table table;
        table.setName("test");
        table.addColumn("id", Sqlite::ColumnType::Integer, {Sqlite::PrimaryKey{}});
        table.addColumn("name");

        database.open();

        table.initialize(database);
    }

    ~SqliteDatabase()
    {
        if (database.isOpen())
            database.close();
        database.unlock();
    }

    std::vector<Utils::SmallString> names() const
    {
        return Sqlite::ReadStatement<1>("SELECT name FROM test", database).values<Utils::SmallString>(8);
    }

    static void updateHookCallback(
        void *object, int type, char const *database, char const *table, long long rowId)
    {
        static_cast<SqliteDatabase *>(object)->callback(static_cast<Sqlite::ChangeType>(type),
                                                        database,
                                                        table,
                                                        rowId);
    }

protected:
    SpyDummy spyDummy;
    QString databaseFilePath{":memory:"};
    mutable Sqlite::Database database;
    Sqlite::TransactionInterface &transactionInterface = database;
    MockFunction<void(Sqlite::ChangeType tupe, char const *, char const *, long long)> callbackMock;
    std::function<void(Sqlite::ChangeType tupe, char const *, char const *, long long)>
        callback = callbackMock.AsStdFunction();
};

TEST_F(SqliteDatabase, SetDatabaseFilePath)
{
    ASSERT_THAT(database.databaseFilePath(), databaseFilePath);
}

TEST_F(SqliteDatabase, SetJournalMode)
{
    database.setJournalMode(JournalMode::Memory);

    ASSERT_THAT(database.journalMode(), JournalMode::Memory);
}

TEST_F(SqliteDatabase, LockingModeIsByDefaultExlusive)
{
    ASSERT_THAT(database.lockingMode(), Sqlite::LockingMode::Exclusive);
}

TEST_F(SqliteDatabase, CreateDatabaseWithLockingModeNormal)
{
    Utils::PathString path{Utils::TemporaryDirectory::masterDirectoryPath()
                           + "/database_exclusive_locked.db"};

    Sqlite::Database database{path, JournalMode::Wal, Sqlite::LockingMode::Normal};

    ASSERT_THAT(database.lockingMode(), Sqlite::LockingMode::Normal);
}

TEST_F(SqliteDatabase, ExclusivelyLockedDatabaseIsLockedForSecondConnection)
{
    using namespace std::chrono_literals;
    Utils::PathString path{Utils::TemporaryDirectory::masterDirectoryPath()
                           + "/database_exclusive_locked.db"};
    Sqlite::Database database{path};

    ASSERT_THROW(Sqlite::Database database2(path, 1ms), Sqlite::StatementIsBusy);
}

TEST_F(SqliteDatabase, NormalLockedDatabaseCanBeReopened)
{
    Utils::PathString path{Utils::TemporaryDirectory::masterDirectoryPath()
                           + "/database_exclusive_locked.db"};
    Sqlite::Database database{path, JournalMode::Wal, Sqlite::LockingMode::Normal};

    ASSERT_NO_THROW((Sqlite::Database{path, JournalMode::Wal, Sqlite::LockingMode::Normal}));
}

TEST_F(SqliteDatabase, SetOpenlMode)
{
    database.setOpenMode(OpenMode::ReadOnly);

    ASSERT_THAT(database.openMode(), OpenMode::ReadOnly);
}

TEST_F(SqliteDatabase, OpenDatabase)
{
    database.close();

    database.open();

    ASSERT_TRUE(database.isOpen());
}

TEST_F(SqliteDatabase, CloseDatabase)
{
    database.close();

    ASSERT_FALSE(database.isOpen());
}

TEST_F(SqliteDatabase, DatabaseIsNotInitializedAfterOpening)
{
    ASSERT_FALSE(database.isInitialized());
}

TEST_F(SqliteDatabase, DatabaseIsIntializedAfterSettingItBeforeOpening)
{
    database.setIsInitialized(true);

    ASSERT_TRUE(database.isInitialized());
}

TEST_F(SqliteDatabase, DatabaseIsInitializedIfDatabasePathExistsAtOpening)
{
    Sqlite::Database database{TESTDATA_DIR "/sqlite_database.db"};

    ASSERT_TRUE(database.isInitialized());
}

TEST_F(SqliteDatabase, DatabaseIsNotInitializedIfDatabasePathDoesNotExistAtOpening)
{
    Sqlite::Database database{Utils::PathString{Utils::TemporaryDirectory::masterDirectoryPath()
                                                + "/database_does_not_exist.db"}};

    ASSERT_FALSE(database.isInitialized());
}

TEST_F(SqliteDatabase, GetChangesCount)
{
    Sqlite::WriteStatement<1> statement("INSERT INTO test(name) VALUES (?)", database);
    statement.write(42);

    ASSERT_THAT(database.changesCount(), 1);
}

TEST_F(SqliteDatabase, GetTotalChangesCount)
{
    Sqlite::WriteStatement<1> statement("INSERT INTO test(name) VALUES (?)", database);
    statement.write(42);

    ASSERT_THAT(database.lastInsertedRowId(), 1);
}

TEST_F(SqliteDatabase, GetLastInsertedRowId)
{
    Sqlite::WriteStatement<1> statement("INSERT INTO test(name) VALUES (?)", database);
    statement.write(42);

    ASSERT_THAT(database.lastInsertedRowId(), 1);
}

TEST_F(SqliteDatabase, LastRowId)
{
    database.setLastInsertedRowId(42);

    ASSERT_THAT(database.lastInsertedRowId(), 42);
}

TEST_F(SqliteDatabase, DeferredBegin)
{
    ASSERT_NO_THROW(transactionInterface.deferredBegin());

    transactionInterface.commit();
}

TEST_F(SqliteDatabase, ImmediateBegin)
{
    ASSERT_NO_THROW(transactionInterface.immediateBegin());

    transactionInterface.commit();
}

TEST_F(SqliteDatabase, ExclusiveBegin)
{
    ASSERT_NO_THROW(transactionInterface.exclusiveBegin());

    transactionInterface.commit();
}

TEST_F(SqliteDatabase, Commit)
{
    transactionInterface.deferredBegin();

    ASSERT_NO_THROW(transactionInterface.commit());
}

TEST_F(SqliteDatabase, Rollback)
{
    transactionInterface.deferredBegin();

    ASSERT_NO_THROW(transactionInterface.rollback());
}

TEST_F(SqliteDatabase, SetUpdateHookSet)
{
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(_, _, _, _));
    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, SetNullUpdateHook)
{
    database.setUpdateHook(this, updateHookCallback);

    database.setUpdateHook(nullptr, nullptr);

    EXPECT_CALL(callbackMock, Call(_, _, _, _)).Times(0);
    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, ResetUpdateHook)
{
    database.setUpdateHook(this, updateHookCallback);

    database.resetUpdateHook();

    EXPECT_CALL(callbackMock, Call(_, _, _, _)).Times(0);
    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, DeleteUpdateHookCall)
{
    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(Eq(Sqlite::ChangeType::Delete), _, _, _));

    Sqlite::WriteStatement("DELETE FROM test WHERE name = 42", database).execute();
}

TEST_F(SqliteDatabase, InsertUpdateHookCall)
{
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(Eq(Sqlite::ChangeType::Insert), _, _, _));

    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, UpdateUpdateHookCall)
{
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(Eq(Sqlite::ChangeType::Insert), _, _, _));

    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, RowIdUpdateHookCall)
{
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(_, _, _, Eq(42)));

    Sqlite::WriteStatement<2>("INSERT INTO test(rowid, name) VALUES (?,?)", database).write(42, "foo");
}

TEST_F(SqliteDatabase, DatabaseUpdateHookCall)
{
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(_, StrEq("main"), _, _));

    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, TableUpdateHookCall)
{
    database.setUpdateHook(this, updateHookCallback);

    EXPECT_CALL(callbackMock, Call(_, _, StrEq("test"), _));

    Sqlite::WriteStatement<1>("INSERT INTO test(name) VALUES (?)", database).write(42);
}

TEST_F(SqliteDatabase, SessionsCommit)
{
    database.setAttachedTables({"test"});
    Sqlite::WriteStatement<2>("INSERT INTO test(id, name) VALUES (?,?)", database).write(1, "foo");
    database.unlock();

    Sqlite::ImmediateSessionTransaction transaction{database};
    Sqlite::WriteStatement<2>("INSERT INTO test(id, name) VALUES (?,?)", database).write(2, "bar");
    transaction.commit();
    database.lock();
    Sqlite::WriteStatement<2>("INSERT OR REPLACE INTO test(id, name) VALUES (?,?)", database)
        .write(2, "hoo");
    database.applyAndUpdateSessions();

    ASSERT_THAT(names(), ElementsAre("foo", "bar"));
}

TEST_F(SqliteDatabase, SessionsRollback)
{
    database.setAttachedTables({"test"});
    Sqlite::WriteStatement<2>("INSERT INTO test(id, name) VALUES (?,?)", database).write(1, "foo");
    database.unlock();

    {
        Sqlite::ImmediateSessionTransaction transaction{database};
        Sqlite::WriteStatement<2>("INSERT INTO test(id, name) VALUES (?,?)", database).write(2, "bar");
    }
    database.lock();
    Sqlite::WriteStatement<2>("INSERT OR REPLACE INTO test(id, name) VALUES (?,?)", database)
        .write(2, "hoo");
    database.applyAndUpdateSessions();

    ASSERT_THAT(names(), ElementsAre("foo", "hoo"));
}

} // namespace
