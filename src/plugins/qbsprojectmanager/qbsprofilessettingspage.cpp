// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qbsprofilessettingspage.h"
#include "ui_qbsprofilessettingswidget.h"

#include "qbsprofilemanager.h"
#include "qbsprojectmanagerconstants.h"
#include "qbssettings.h"

#include <coreplugin/icore.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectexplorericons.h>
#include <projectexplorer/taskhub.h>
#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/treemodel.h>

#include <QCoreApplication>
#include <QHash>
#include <QWidget>

using namespace ProjectExplorer;

namespace QbsProjectManager {
namespace Internal {

class ProfileTreeItem : public Utils::TypedTreeItem<ProfileTreeItem, ProfileTreeItem>
{
public:
    ProfileTreeItem() = default;
    ProfileTreeItem(const QString &key, const QString &value) : m_key(key), m_value(value) { }

private:
    QVariant data(int column, int role) const override
    {
        if (role != Qt::DisplayRole)
            return {};
        if (column == 0)
            return m_key;
        if (column == 1)
            return m_value;
        return {};
    }

    const QString m_key;
    const QString m_value;
};

class ProfileModel : public Utils::TreeModel<ProfileTreeItem>
{
    Q_OBJECT
public:
    ProfileModel() : TreeModel(static_cast<QObject *>(nullptr))
    {
        setHeader(QStringList{tr("Key"), tr("Value")});
        reload();
    }

    void reload()
    {
        ProfileTreeItem * const newRoot = new ProfileTreeItem(QString(), QString());
        QHash<QStringList, ProfileTreeItem *> itemMap;
        const QStringList output = QbsProfileManager::runQbsConfig(
                    QbsProfileManager::QbsConfigOp::Get, "profiles").split('\n', Qt::SkipEmptyParts);
        for (QString line : output) {
            line = line.trimmed();
            line = line.mid(QString("profiles.").length());
            const int colonIndex = line.indexOf(':');
            if (colonIndex == -1)
                continue;
            const QStringList key = line.left(colonIndex).trimmed().split('.', Qt::SkipEmptyParts);
            const QString value = line.mid(colonIndex + 1).trimmed();
            QStringList partialKey;
            ProfileTreeItem *parent = newRoot;
            for (const QString &keyComponent : key) {
                partialKey << keyComponent;
                ProfileTreeItem *&item = itemMap[partialKey];
                if (!item) {
                    item = new ProfileTreeItem(keyComponent, partialKey == key ? value : QString());
                    parent->appendChild(item);
                }
                parent = item;
            }
        }
        setRootItem(newRoot);
    }
};

class QbsProfilesSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    QbsProfilesSettingsWidget();

private:
    void refreshKitsList();
    void displayCurrentProfile();

    Ui::QbsProfilesSettingsWidget m_ui;
    ProfileModel m_model;
};

QbsProfilesSettingsPage::QbsProfilesSettingsPage()
{
    setId("Y.QbsProfiles");
    setDisplayName(QCoreApplication::translate("QbsProjectManager", "Profiles"));
    setCategory(Constants::QBS_SETTINGS_CATEGORY);
}

QWidget *QbsProfilesSettingsPage::widget()
{
    if (!m_widget)
        m_widget = new QbsProfilesSettingsWidget;
    return m_widget;
}

void QbsProfilesSettingsPage::finish()
{
    delete m_widget;
    m_widget = nullptr;
}

QbsProfilesSettingsWidget::QbsProfilesSettingsWidget()
{
    m_ui.setupUi(this);
    connect(QbsProfileManager::instance(), &QbsProfileManager::qbsProfilesUpdated,
            this, &QbsProfilesSettingsWidget::refreshKitsList);
    connect(m_ui.expandButton, &QAbstractButton::clicked,
            m_ui.propertiesView, &QTreeView::expandAll);
    connect(m_ui.collapseButton, &QAbstractButton::clicked,
            m_ui.propertiesView, &QTreeView::collapseAll);
    refreshKitsList();
}

void QbsProfilesSettingsWidget::refreshKitsList()
{
    m_ui.kitsComboBox->disconnect(this);
    m_ui.propertiesView->setModel(nullptr);
    m_model.reload();
    m_ui.profileValueLabel->clear();
    Utils::Id currentId;
    if (m_ui.kitsComboBox->count() > 0)
        currentId = Utils::Id::fromSetting(m_ui.kitsComboBox->currentData());
    m_ui.kitsComboBox->clear();
    int newCurrentIndex = -1;
    QList<Kit *> validKits = KitManager::kits();
    Utils::erase(validKits, [](const Kit *k) { return !k->isValid(); });
    const bool hasKits = !validKits.isEmpty();
    for (const Kit * const kit : qAsConst(validKits)) {
        if (kit->id() == currentId)
            newCurrentIndex = m_ui.kitsComboBox->count();
        m_ui.kitsComboBox->addItem(kit->displayName(), kit->id().toSetting());
    }
    if (newCurrentIndex != -1)
        m_ui.kitsComboBox->setCurrentIndex(newCurrentIndex);
    else if (hasKits)
        m_ui.kitsComboBox->setCurrentIndex(0);
    displayCurrentProfile();
    connect(m_ui.kitsComboBox, &QComboBox::currentIndexChanged,
            this, &QbsProfilesSettingsWidget::displayCurrentProfile);
}

void QbsProfilesSettingsWidget::displayCurrentProfile()
{
    m_ui.propertiesView->setModel(nullptr);
    if (m_ui.kitsComboBox->currentIndex() == -1)
        return;
    const Utils::Id kitId = Utils::Id::fromSetting(m_ui.kitsComboBox->currentData());
    const Kit * const kit = KitManager::kit(kitId);
    QTC_ASSERT(kit, return);
    const QString profileName = QbsProfileManager::ensureProfileForKit(kit);
    m_ui.profileValueLabel->setText(profileName);
    for (int i = 0; i < m_model.rowCount(); ++i) {
        const QModelIndex currentProfileIndex = m_model.index(i, 0);
        if (m_model.data(currentProfileIndex, Qt::DisplayRole).toString() != profileName)
            continue;
        m_ui.propertiesView->setModel(&m_model);
        m_ui.propertiesView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_ui.propertiesView->setRootIndex(currentProfileIndex);
        return;
    }
}

} // namespace Internal
} // namespace QbsProjectManager

#include "qbsprofilessettingspage.moc"
