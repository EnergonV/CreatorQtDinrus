// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "exception.h"

namespace QmlDesigner {

class QMLDESIGNERCORE_EXPORT InvalidSlideIndexException : public Exception
{
public:
    InvalidSlideIndexException(int line,
                               const QByteArray &function,
                               const QByteArray &file);
    QString type() const override;
};

} // namespace QmlDesigner
