// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "invalidmetainfoexception.h"
#include "propertymetainfo.h"
#include "qmldesignercorelib_global.h"

#include <projectstorage/projectstoragefwd.h>
#include <projectstorage/projectstoragetypes.h>
#include <projectstorageids.h>

#include <utils/optional.h>

#include <QList>
#include <QString>
#include <QIcon>

#include <vector>

QT_BEGIN_NAMESPACE
class QDeclarativeContext;
QT_END_NAMESPACE

namespace QmlDesigner {

class MetaInfo;
class Model;
class AbstractProperty;

class QMLDESIGNERCORE_EXPORT NodeMetaInfo
{
public:
    NodeMetaInfo() = default;
    NodeMetaInfo(Model *model, const TypeName &typeName, int majorVersion, int minorVersion);
    NodeMetaInfo(TypeId typeId, NotNullPointer<const ProjectStorage<Sqlite::Database>> projectStorage)
        : m_typeId{typeId}
        , m_projectStorage{projectStorage}
    {}
    NodeMetaInfo(NotNullPointer<const ProjectStorage<Sqlite::Database>> projectStorage)
        : m_projectStorage{projectStorage}
    {}
    ~NodeMetaInfo();

    bool isValid() const;
    bool isFileComponent() const;
    bool hasProperty(Utils::SmallStringView propertyName) const;
    PropertyMetaInfos properties() const;
    PropertyMetaInfos localProperties() const;
    PropertyMetaInfo property(const PropertyName &propertyName) const;
    PropertyNameList signalNames() const;
    PropertyNameList slotNames() const;
    PropertyName defaultPropertyName() const;
    bool hasDefaultProperty() const;

    std::vector<NodeMetaInfo> classHierarchy() const;
    std::vector<NodeMetaInfo> superClasses() const;
    NodeMetaInfo directSuperClass() const;

    bool defaultPropertyIsComponent() const;

    TypeName typeName() const;
    TypeName simplifiedTypeName() const;
    int majorVersion() const;
    int minorVersion() const;

    QString componentFileName() const;

    bool availableInVersion(int majorVersion, int minorVersion) const;
    bool isSubclassOf(const TypeName &type, int majorVersion = -1, int minorVersion = -1) const;
    bool isSubclassOf(const NodeMetaInfo &metaInfo) const;

    bool isGraphicalItem() const;
    bool isQmlItem() const;
    bool isLayoutable() const;
    bool isView() const;
    bool isTabView() const;
    bool isAlias() const;
    bool isQmlComponent() const;
    bool isFont() const;
    bool isColor() const;
    bool isBool() const;
    bool isInteger() const;
    bool isFloat() const;
    bool isVariant() const;
    bool isString() const;
    bool isUrl() const;
    bool isQtQuick3DTexture() const;

    bool isEnumeration() const;
    QString importDirectoryPath() const;

    friend bool operator==(const NodeMetaInfo &first, const NodeMetaInfo &second)
    {
        if constexpr (useProjectStorage())
            return first.m_typeId == second.m_typeId;
        else
            return first.m_privateData == second.m_privateData;
    }

private:
    const Storage::Info::Type &typeData() const;

private:
    TypeId m_typeId;
    NotNullPointer<const ProjectStorage<Sqlite::Database>> m_projectStorage = {};
    mutable Utils::optional<Storage::Info::Type> m_typeData;
    QSharedPointer<class NodeMetaInfoPrivate> m_privateData;
};

using NodeMetaInfos = std::vector<NodeMetaInfo>;

} //QmlDesigner
