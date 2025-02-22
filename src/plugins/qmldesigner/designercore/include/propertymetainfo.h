// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <qmldesignercorelib_global.h>

#include <projectstorage/projectstoragefwd.h>
#include <projectstorage/projectstoragetypes.h>
#include <projectstorageids.h>

#include <utils/optional.h>

#include <QSharedPointer>
#include <QString>

#include <vector>

namespace QmlDesigner {

class NodeMetaInfo;

class QMLDESIGNERCORE_EXPORT PropertyMetaInfo
{
public:
    PropertyMetaInfo(QSharedPointer<class NodeMetaInfoPrivate> nodeMetaInfoPrivateData,
                     const PropertyName &propertyName);
    PropertyMetaInfo(PropertyDeclarationId id,
                     NotNullPointer<const ProjectStorage<Sqlite::Database>> projectStorage)
        : m_id{id}
        , m_projectStorage{projectStorage}
    {}
    ~PropertyMetaInfo();

    PropertyName name() const;
    NodeMetaInfo propertyType() const;
    bool isWritable() const;
    bool isListProperty() const;
    bool isEnumType() const;
    bool isPrivate() const;
    bool isPointer() const;
    QVariant castedValue(const QVariant &value) const;

private:
    const Storage::Info::PropertyDeclaration &propertyData() const;
    TypeName propertyTypeName() const;

private:
    QSharedPointer<class NodeMetaInfoPrivate> m_nodeMetaInfoPrivateData;
    PropertyName m_propertyName;
    PropertyDeclarationId m_id;
    NotNullPointer<const ProjectStorage<Sqlite::Database>> m_projectStorage;
    mutable Utils::optional<Storage::Info::PropertyDeclaration> m_propertyData;
};

using PropertyMetaInfos = std::vector<PropertyMetaInfo>;

} // namespace QmlDesigner
