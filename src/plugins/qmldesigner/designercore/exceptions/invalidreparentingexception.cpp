// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "invalidreparentingexception.h"
/*!
\class QmlDesigner::InvalidReparentingException
\ingroup CoreExceptions
    \brief The InvalidReparentingException class provides an exception for
    invalid reparenting.

\see ModelNode
*/
namespace QmlDesigner {
/*!
    Constructs an exception. \a line uses the __LINE__ macro,
    \a function uses the __FUNCTION__ or the Q_FUNC_INFO macro, and \a file uses
    the __FILE__ macro.
*/
InvalidReparentingException::InvalidReparentingException(int line,
                                                         const QByteArray &function,
                                                         const QByteArray &file)
 : Exception(line, function, file)
{
    createWarning();
}

/*!
Returns the type of this exception as a string.
*/
QString InvalidReparentingException::type() const
{
    return QLatin1String("InvalidReparentingException");
}
} // namespace QmlDesigner
