// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/optional.h>
#include <utils/smallstringview.h>

#include <type_traits>

namespace Sqlite {

constexpr int compare(Utils::SmallStringView first, Utils::SmallStringView second) noexcept
{
    return first.compare(second);
}

enum class UpdateChange { No, Update };

template<typename SqliteRange,
         typename Range,
         typename CompareKey,
         typename InsertCallback,
         typename UpdateCallback,
         typename DeleteCallback>
void insertUpdateDelete(SqliteRange &&sqliteRange,
                        Range &&values,
                        CompareKey compareKey,
                        InsertCallback insertCallback,
                        UpdateCallback updateCallback,
                        DeleteCallback deleteCallback)
{
    auto currentSqliteIterator = sqliteRange.begin();
    auto endSqliteIterator = sqliteRange.end();
    auto currentValueIterator = values.begin();
    auto endValueIterator = values.end();
    Utils::optional<std::decay_t<decltype(*currentValueIterator)>> lastValue;

    while (true) {
        bool hasMoreValues = currentValueIterator != endValueIterator;
        bool hasMoreSqliteValues = currentSqliteIterator != endSqliteIterator;
        if (hasMoreValues && hasMoreSqliteValues) {
            auto &&sqliteValue = *currentSqliteIterator;
            auto &&value = *currentValueIterator;
            auto compare = compareKey(sqliteValue, value);
            if (compare == 0) {
                UpdateChange updateChange = updateCallback(sqliteValue, value);
                switch (updateChange) {
                case UpdateChange::Update:
                    lastValue = value;
                    break;
                case UpdateChange::No:
                    lastValue.reset();
                    break;
                }
                ++currentSqliteIterator;
                ++currentValueIterator;
            } else if (compare > 0) {
                insertCallback(value);
                ++currentValueIterator;
            } else if (compare < 0) {
                if (lastValue) {
                    if (compareKey(sqliteValue, *lastValue) != 0)
                        deleteCallback(sqliteValue);
                    lastValue.reset();
                } else {
                    deleteCallback(sqliteValue);
                }
                ++currentSqliteIterator;
            }
        } else if (hasMoreValues) {
            insertCallback(*currentValueIterator);
            ++currentValueIterator;
        } else if (hasMoreSqliteValues) {
            auto &&sqliteValue = *currentSqliteIterator;
            if (lastValue) {
                if (compareKey(sqliteValue, *lastValue) != 0)
                    deleteCallback(sqliteValue);
                lastValue.reset();
            } else {
                deleteCallback(sqliteValue);
            }
            ++currentSqliteIterator;
        } else {
            break;
        }
    }
}

} // namespace Sqlite
