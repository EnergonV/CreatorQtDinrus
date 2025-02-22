// Copyright (C) 2016 Dmitry Savchenko
// Copyright (C) 2016 Vasiliy Sorokin
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "lineparser.h"

#include <QMap>

namespace Todo {
namespace Internal {

LineParser::LineParser(const KeywordList &keywordList)
{
    setKeywordList(keywordList);
}

void LineParser::setKeywordList(const KeywordList &keywordList)
{
    m_keywords = keywordList;
}

QList<TodoItem> LineParser::parse(const QString &line)
{
    QMap<int, int> entryCandidates = findKeywordEntryCandidates(line);
    QList<KeywordEntry> entries = keywordEntriesFromCandidates(entryCandidates, line);
    return todoItemsFromKeywordEntries(entries);
}

bool LineParser::isKeywordSeparator(const QChar &ch)
{
    return ch.isSpace() || (ch == ':') || (ch == '/') || (ch == '*') || (ch == '(');
}

LineParser::KeywordEntryCandidates LineParser::findKeywordEntryCandidates(const QString &line)
{
    KeywordEntryCandidates entryCandidates;

    for (int i = 0; i < m_keywords.count(); ++i) {
        int searchFrom = -1;
        forever {
            const int index = line.lastIndexOf(m_keywords.at(i).name, searchFrom);

            if (index == -1)
                break; // 'forever' loop exit condition

            searchFrom = index - line.length() - 1;

            if (isKeywordAt(index, line, m_keywords.at(i).name))
                entryCandidates.insert(index, i);
        }
    }

    return entryCandidates;
}

bool LineParser::isKeywordAt(int index, const QString &line, const QString &keyword)
{
    if (!isFirstCharOfTheWord(index, line))
        return false;

    if (!isLastCharOfTheWord(index + keyword.length() - 1, line))
        return false;

    return true;
}

bool LineParser::isFirstCharOfTheWord(int index, const QString &line)
{
    return (index == 0) || isKeywordSeparator(line.at(index - 1));
}

bool LineParser::isLastCharOfTheWord(int index, const QString &line)
{
    return (index == line.length() - 1) || isKeywordSeparator(line.at(index + 1));
}

QList<LineParser::KeywordEntry> LineParser::keywordEntriesFromCandidates(
    const QMap<int, int> &candidates, const QString &line)
{
    // Ensure something is found
    if (candidates.isEmpty())
        return QList<KeywordEntry>();

    // Convert candidates to entries
    std::vector<KeywordEntry> tmp;
    for (auto it = candidates.cbegin(), end = candidates.cend(); it != end; ++it)
        tmp.emplace_back(KeywordEntry{it.value(), it.key(), QString()});

    QList<KeywordEntry> entries;
    for (auto it = tmp.crbegin(), end = tmp.crend(); it != end; ++it) {
        KeywordEntry entry = *it;

        int keywordLength = m_keywords.at(entry.keywordIndex).name.length();

        int entryTextLength = -1;
        if (!entries.empty())
            entryTextLength = entries.last().keywordStart - (entry.keywordStart + keywordLength);

        entry.text = line.mid(entry.keywordStart + keywordLength, entryTextLength);

        if (trimSeparators(entry.text).isEmpty() && !entries.empty())
            // Take the text form the previous entry, consider:
            // '<keyword1>: <keyword2>: <some text>'
            entry.text = entries.last().text;

        entries << entry;
    }

    return entries;
}

QString LineParser::trimSeparators(const QString &string)
{
    QString result = string.trimmed();

    while (startsWithSeparator(result))
        result = result.right(result.length() - 1);

    while (endsWithSeparator(result))
        result = result.left(result.length() - 1);

    return result;
}

bool LineParser::startsWithSeparator(const QString &string)
{
    return !string.isEmpty() && isKeywordSeparator(string.at(0));
}

bool LineParser::endsWithSeparator(const QString &string)
{
    return !string.isEmpty() && isKeywordSeparator(string.at(string.length() - 1));
}

QList<TodoItem> LineParser::todoItemsFromKeywordEntries(const QList<KeywordEntry> &entries)
{
    QList<TodoItem> todoItems;

    foreach (const KeywordEntry &entry, entries) {
        TodoItem item;
        item.text =  m_keywords.at(entry.keywordIndex).name + entry.text;
        item.color = m_keywords.at(entry.keywordIndex).color;
        item.iconType = m_keywords.at(entry.keywordIndex).iconType;
        todoItems << item;
    }

    return todoItems;
}

} // namespace Internal
} // namespace Todo
