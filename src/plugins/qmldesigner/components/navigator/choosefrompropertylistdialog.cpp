// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "choosefrompropertylistdialog.h"
#include "nodemetainfo.h"
#include "ui_choosefrompropertylistdialog.h"

namespace QmlDesigner {

// This will filter and return possible properties that the given type can be bound to
ChooseFromPropertyListFilter::ChooseFromPropertyListFilter(const NodeMetaInfo &insertInfo,
                                                           const NodeMetaInfo &parentInfo,
                                                           bool breakOnFirst)
{
    // TODO: Metainfo based matching system (QDS-6240)

    // Fall back to a hardcoded list of supported cases:
    // Texture
    //  -> DefaultMaterial
    //  -> PrincipledMaterial
    //  -> SpriteParticle3D
    //  -> TextureInput
    //  -> SceneEnvironment
    // Effect
    //  -> SceneEnvironment
    // Shader, Command, Buffer
    //  -> Pass
    // InstanceListEntry
    //  -> InstanceList
    // Pass
    //  -> Effect
    // Particle3D
    //  -> ParticleEmitter3D
    // ParticleAbstractShape3D
    //  -> ParticleEmitter3D
    //  -> Attractor3D
    // Material
    //  -> Model

    const TypeName textureType = "QtQuick3D.Texture";
    if (insertInfo.isSubclassOf(textureType)) {
        const TypeName textureTypeCpp = "<cpp>.QQuick3DTexture";
        if (parentInfo.isSubclassOf("QtQuick3D.DefaultMaterial")
            || parentInfo.isSubclassOf("QtQuick3D.PrincipledMaterial")) {
            // All texture properties are valid targets
            for (const auto &property : parentInfo.properties()) {
                const TypeName &propType = property.propertyType().typeName();
                if (propType == textureType || propType == textureTypeCpp) {
                    propertyList.append(QString::fromUtf8(property.name()));
                    if (breakOnFirst)
                        return;
                }
            }
        } else if (parentInfo.isSubclassOf("QtQuick3D.Particles3D.SpriteParticle3D")) {
            propertyList.append("sprite");
        } else if (parentInfo.isSubclassOf("QtQuick3D.TextureInput")) {
            propertyList.append("texture");
        } else if (parentInfo.isSubclassOf("QtQuick3D.SceneEnvironment")) {
            propertyList.append("lightProbe");
        }
    } else if (insertInfo.isSubclassOf("QtQuick3D.Effect")) {
        if (parentInfo.isSubclassOf("QtQuick3D.SceneEnvironment"))
            propertyList.append("effects");
    } else if (insertInfo.isSubclassOf("QtQuick3D.Shader")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Pass"))
            propertyList.append("shaders");
    } else if (insertInfo.isSubclassOf("QtQuick3D.Command")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Pass"))
            propertyList.append("commands");
    } else if (insertInfo.isSubclassOf("QtQuick3D.Buffer")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Pass"))
            propertyList.append("output");
    } else if (insertInfo.isSubclassOf("QtQuick3D.InstanceListEntry")) {
        if (parentInfo.isSubclassOf("QtQuick3D.InstanceList"))
            propertyList.append("instances");
    } else if (insertInfo.isSubclassOf("QtQuick3D.Pass")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Effect"))
            propertyList.append("passes");
    } else if (insertInfo.isSubclassOf("QtQuick3D.Particles3D.Particle3D")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Particles3D.ParticleEmitter3D"))
            propertyList.append("particle");
    } else if (insertInfo.isSubclassOf("QQuick3DParticleAbstractShape")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Particles3D.ParticleEmitter3D")
                || parentInfo.isSubclassOf("QtQuick3D.Particles3D.Attractor3D"))
            propertyList.append("shape");
    } else if (insertInfo.isSubclassOf("QtQuick3D.Material")) {
        if (parentInfo.isSubclassOf("QtQuick3D.Particles3D.Model"))
            propertyList.append("materials");
    }
}

// This dialog displays specified properties and allows the user to choose one
ChooseFromPropertyListDialog::ChooseFromPropertyListDialog(const QStringList &propNames,
                                                           QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::ChooseFromPropertyListDialog)
{
    if (propNames.count() == 1) {
       m_selectedProperty = propNames.first().toLatin1();
       m_isSoloProperty = true;
       return;
    }
    m_ui->setupUi(this);
    setWindowTitle(tr("Select property"));
    m_ui->label->setText(tr("Bind to property:"));
    m_ui->label->setToolTip(tr("Binds this component to the parent's selected property."));
    setFixedSize(size());

    connect(m_ui->listProps, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        m_selectedProperty = item->isSelected() ? item->data(Qt::DisplayRole).toByteArray() : QByteArray();
    });

    connect(m_ui->listProps,
            &QListWidget::itemDoubleClicked,
            this,
            [this]([[maybe_unused]] QListWidgetItem *item) { QDialog::accept(); });

    fillList(propNames);
}

ChooseFromPropertyListDialog::~ChooseFromPropertyListDialog()
{
    delete m_ui;
}

TypeName ChooseFromPropertyListDialog::selectedProperty() const
{
    return m_selectedProperty;
}

// Create dialog for selecting any property matching newNode type
// Subclass type matches are also valid
ChooseFromPropertyListDialog *ChooseFromPropertyListDialog::createIfNeeded(
        const ModelNode &targetNode, const ModelNode &newNode, QWidget *parent)
{
    const NodeMetaInfo info = newNode.metaInfo();
    const NodeMetaInfo targetInfo = targetNode.metaInfo();
    ChooseFromPropertyListFilter *filter = new ChooseFromPropertyListFilter(info, targetInfo);

    if (!filter->propertyList.isEmpty())
        return new ChooseFromPropertyListDialog(filter->propertyList, parent);

    return nullptr;
}

// Create dialog for selecting writable properties of exact property type
ChooseFromPropertyListDialog *ChooseFromPropertyListDialog::createIfNeeded(
    const ModelNode &targetNode, const NodeMetaInfo &propertyType, QWidget *parent)
{
    const NodeMetaInfo metaInfo = targetNode.metaInfo();
    QStringList matchingNames;
    for (const auto &property : metaInfo.properties()) {
        if (property.propertyType() == propertyType && property.isWritable())
            matchingNames.append(QString::fromUtf8(property.name()));
    }

    if (!matchingNames.isEmpty())
        return new ChooseFromPropertyListDialog(matchingNames, parent);

    return nullptr;
}

void ChooseFromPropertyListDialog::fillList(const QStringList &propNames)
{
    if (propNames.isEmpty())
        return;

    QString defaultProp = propNames.first();
    QStringList sortedNames = propNames;
    sortedNames.sort();
    for (const auto &propName : qAsConst(sortedNames)) {
        QListWidgetItem *newItem = new QListWidgetItem(propName);
        m_ui->listProps->addItem(newItem);
    }

    // Select the default prop
    m_ui->listProps->setCurrentRow(sortedNames.indexOf(defaultProp));
    m_selectedProperty = defaultProp.toLatin1();
}

}
