// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <qmldebug/qdebugmessageclient.h>
#include <qmldebug/qmloutputparser.h>

namespace Debugger::Internal {

void appendDebugOutput(QtMsgType type, const QString &message, const QmlDebug::QDebugContextInfo &info);

void clearExceptionSelection();
QStringList highlightExceptionCode(int lineNumber, const QString &filePath, const QString &errorMessage);

} // Debugger::Internal
