// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "autotestconstants.h"

#include <utils/fileutils.h>

#include <QColor>
#include <QMetaType>
#include <QSharedPointer>
#include <QString>

namespace Autotest {

class ITestTreeItem;

enum class ResultType {
    // result types (have icon, color, short text)
    Pass, FIRST_TYPE = Pass,
    Fail,
    ExpectedFail,
    UnexpectedPass,
    Skip,
    BlacklistedPass,
    BlacklistedFail,
    BlacklistedXPass,
    BlacklistedXFail,

    // special (message) types (have icon, color, short text)
    Benchmark,
    MessageDebug,
    MessageInfo,
    MessageWarn,
    MessageFatal,
    MessageSystem,
    MessageError,

    // special message - get's icon (but no color/short text) from parent
    MessageLocation,
    // anything below is an internal message (or a pure message without icon)
    MessageInternal, INTERNAL_MESSAGES_BEGIN = MessageInternal,
    // start item (get icon/short text depending on children)
    TestStart,
    // usually no icon/short text - more or less an indicator (and can contain test duration)
    TestEnd,
    // special global (temporary) message
    MessageCurrentTest, INTERNAL_MESSAGES_END = MessageCurrentTest,

    Application,        // special.. not to be used outside of testresultmodel
    Invalid,            // indicator for unknown result items
    LAST_TYPE = Invalid
};

inline auto qHash(const ResultType &result)
{
    return QT_PREPEND_NAMESPACE(qHash(int(result)));
}

class TestResult
{
public:
    TestResult() = default;
    TestResult(const QString &id, const QString &name);
    virtual ~TestResult() {}

    virtual const QString outputString(bool selected) const;
    virtual const ITestTreeItem *findTestTreeItem() const;

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    ResultType result() const { return m_result; }
    QString description() const { return m_description; }
    Utils::FilePath fileName() const { return m_file; }
    int line() const { return m_line; }

    void setDescription(const QString &description) { m_description = description; }
    void setFileName(const Utils::FilePath &fileName) { m_file = fileName; }
    void setLine(int line) { m_line = line; }
    void setResult(ResultType type) { m_result = type; }

    static ResultType resultFromString(const QString &resultString);
    static ResultType toResultType(int rt);
    static QString resultToString(const ResultType type);
    static QColor colorForType(const ResultType type);

    virtual bool isDirectParentOf(const TestResult *other, bool *needsIntermediate) const;
    virtual bool isIntermediateFor(const TestResult *other) const;
    virtual TestResult *createIntermediateResultFor(const TestResult *other);
private:
    QString m_id;
    QString m_name;
    ResultType m_result = ResultType::Invalid;  // the real result..
    QString m_description;
    Utils::FilePath m_file;
    int m_line = 0;
};

using TestResultPtr = QSharedPointer<TestResult>;

} // namespace Autotest

Q_DECLARE_METATYPE(Autotest::TestResult)
Q_DECLARE_METATYPE(Autotest::ResultType)
