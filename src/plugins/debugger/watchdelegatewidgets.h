// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <QLineEdit>
#include <QComboBox>

namespace Debugger::Internal {

class IntegerValidator;

/* Watch edit widgets. The logic is based on the QVariant 'modelData' property,
 * which is accessed by the WatchDelegate. */

/* WatchLineEdit: Base class for Watch delegate line edits with
 * ready-made accessors for the model's QVariants for QString-text use. */
class WatchLineEdit : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText USER false)
    Q_PROPERTY(QVariant modelData READ modelData WRITE setModelData DESIGNABLE false USER true)
public:
    explicit WatchLineEdit(QWidget *parent = nullptr);

    // Ready-made accessors for item views passing QVariants around
    virtual QVariant modelData() const;
    virtual void setModelData(const QVariant &);

    static WatchLineEdit *create(QVariant::Type t, QWidget *parent = nullptr);
};

/* Watch delegate line edit for integer numbers based on quint64/qint64.
 * Does validation using the given number base (10, 16, 8, 2) and signedness.
 * isBigInt() indicates that no checking for number conversion is to be performed
 * (that is, value cannot be handled as quint64/qint64, for 128bit registers, etc). */
class IntegerWatchLineEdit : public WatchLineEdit
{
    Q_OBJECT
    Q_PROPERTY(int base READ base WRITE setBase DESIGNABLE true)
    Q_PROPERTY(bool Signed READ isSigned WRITE setSigned DESIGNABLE true)
    Q_PROPERTY(bool bigInt READ isBigInt WRITE setBigInt DESIGNABLE true)
public:
    explicit IntegerWatchLineEdit(QWidget *parent = nullptr);

    QVariant modelData() const final;
    void setModelData(const QVariant &) final;

    int base() const;
    void setBase(int b);
    bool isSigned() const;
    void setSigned(bool s);
    bool isBigInt() const;
    void setBigInt(bool b);

    static bool isUnsignedHexNumber(const QString &v);

private:
    void setNumberText(const QString &);
    inline QVariant modelDataI() const;
    IntegerValidator *m_validator;
};

/* Float line edit */
class FloatWatchLineEdit : public WatchLineEdit
{
public:
    explicit FloatWatchLineEdit(QWidget *parent = nullptr);

    QVariant modelData() const final;
    void setModelData(const QVariant &) final;
};

/* Combo box for booleans */
class BooleanComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant modelData READ modelData WRITE setModelData DESIGNABLE false USER true)
public:
    explicit BooleanComboBox(QWidget *parent = nullptr);

    virtual QVariant modelData() const;
    virtual void setModelData(const QVariant &);
};

} // Debugger::Internal
