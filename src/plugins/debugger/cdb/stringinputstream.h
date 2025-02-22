// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>

namespace Debugger {
namespace Internal {

class StringInputStream
{
    Q_DISABLE_COPY(StringInputStream)

public:
    using ModifierFunc = void (StringInputStream &);

    explicit StringInputStream(QString &str);

    StringInputStream &operator<<(char a)              { m_target.append(a); return *this; }
    StringInputStream &operator<<(const char *a)       { m_target.append(QString::fromUtf8(a)); return *this; }
    StringInputStream &operator<<(const QString &a)    { m_target.append(a); return *this; }
    StringInputStream &operator<<(QStringView a)
    {
        m_target.append(a.toString());
        return *this;
    }

    StringInputStream &operator<<(int i) { appendInt(i); return *this; }
    StringInputStream &operator<<(unsigned i) { appendInt(i); return *this; }
    StringInputStream &operator<<(quint64 i) { appendInt(i); return *this; }
    StringInputStream &operator<<(qint64 i) { appendInt(i); return *this; }

    // Stream a modifier by invoking it
    StringInputStream &operator<<(ModifierFunc mf) { mf(*this); return *this; }

    void setHexPrefix(bool hp) { m_hexPrefix = hp; }
    bool hexPrefix() const     { return  m_hexPrefix; }
    void setIntegerBase(int b) { m_integerBase = b; }
    int integerBase() const    { return m_integerBase; }
    // Append a separator if required (target does not end with it)
    void appendSeparator(char c = ' ');

private:
    template <class IntType> void appendInt(IntType i);

    QString &m_target;
    int m_integerBase = 10;
    bool m_hexPrefix = false;
    int m_width = 0;
};

template <class IntType>
void StringInputStream::appendInt(IntType i)
{
    const bool hexPrefix = m_integerBase == 16 && m_hexPrefix;
    if (hexPrefix)
        m_target.append("0x");
    const QString n = QString::number(i, m_integerBase);
    if (m_width > 0) {
        int pad = m_width - n.size();
        if (hexPrefix)
            pad -= 2;
        if (pad > 0)
            m_target.append(QString('0', QLatin1Char(pad)));
    }
    m_target.append(n);
}

// Streamable modifiers for StringInputStream
void hexPrefixOn(StringInputStream &bs);
void hexPrefixOff(StringInputStream &bs);
void hex(StringInputStream &bs);
void dec(StringInputStream &bs);
void blankSeparator(StringInputStream &bs);

// Bytearray parse helpers
QByteArray trimFront(QByteArray in);
QByteArray trimBack(QByteArray in);
QByteArray simplify(const QByteArray &inIn);

} // namespace Internal
} // namespace Debugger
