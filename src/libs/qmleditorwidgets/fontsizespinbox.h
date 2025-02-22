// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "qmleditorwidgets_global.h"
#include <QAbstractSpinBox>

namespace QmlEditorWidgets {

class QMLEDITORWIDGETS_EXPORT FontSizeSpinBox : public QAbstractSpinBox
{
    Q_OBJECT

     Q_PROPERTY(bool isPixelSize READ isPixelSize WRITE setIsPixelSize NOTIFY formatChanged)
     Q_PROPERTY(bool isPointSize READ isPointSize WRITE setIsPointSize NOTIFY formatChanged)
     Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

public:
    explicit FontSizeSpinBox(QWidget *parent = nullptr);

     bool isPixelSize() { return !m_isPointSize; }
     bool isPointSize() { return m_isPointSize; }

     void stepBy(int steps) override;

     QValidator::State validate (QString &input, int &pos) const override;
     int value() const { return m_value; }

signals:
     void formatChanged();
     void valueChanged(int);

public:
     void setIsPointSize(bool b)
     {
         if (isPointSize() == b)
             return;

         m_isPointSize = b;
         setText();
         emit formatChanged();
     }

     void setIsPixelSize(bool b)
     {
         if (isPixelSize() == b)
             return;

         m_isPointSize = !b;
         setText();
         emit formatChanged();
     }


     void clear() override;
     void setValue (int val);

 protected:
    StepEnabled stepEnabled() const override;

private:
    void onEditingFinished();
    void setText();

    bool m_isPointSize;
    int m_value;

};

} //QmlDesigner
