// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "projectstoragetypes.h"

#include <QString>

namespace QmlDesigner {

class QmlDocumentParserInterface
{
public:
    virtual Storage::Synchronization::Type parse(const QString &sourceContent,
                                                 Storage::Synchronization::Imports &imports,
                                                 SourceId sourceId,
                                                 Utils::SmallStringView directoryPath)
        = 0;

protected:
    ~QmlDocumentParserInterface() = default;
};
} // namespace QmlDesigner
