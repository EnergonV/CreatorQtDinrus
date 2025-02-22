// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>
#include <QSet>
#include <QSharedPointer>
#include <QVersionNumber>
#include <QtGlobal>

#if defined(MCUSUPPORT_LIBRARY)
#define MCUSUPPORTSHARED_EXPORT Q_DECL_EXPORT
#else
#define MCUSUPPORTSHARED_EXPORT Q_DECL_IMPORT
#endif

namespace McuSupport::Internal {

class McuTarget;
class McuAbstractPackage;
class McuToolChainPackage;

using McuPackagePtr = QSharedPointer<McuAbstractPackage>;
using McuToolChainPackagePtr = QSharedPointer<McuToolChainPackage>;
using McuTargetPtr = QSharedPointer<McuTarget>;

static const QVersionNumber minimalVersion{2, 0, 0};
static const QVersionNumber newVersion{2, 3};
using Targets = QList<McuTargetPtr>;
using Packages = QSet<McuPackagePtr>;

} // namespace McuSupport::Internal
