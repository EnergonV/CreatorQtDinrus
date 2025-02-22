// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "sshsettingspage.h"
#include "sshsettings.h"

#include <coreplugin/icore.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <utils/hostosinfo.h>
#include <utils/pathchooser.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QSpinBox>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

class SshSettingsWidget : public Core::IOptionsPageWidget
{
    Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::SshSettingsWidget)

public:
    SshSettingsWidget();
    void saveSettings();

private:
    void apply() final { saveSettings(); }

    void setupConnectionSharingCheckBox();
    void setupConnectionSharingSpinBox();
    void setupSshPathChooser();
    void setupSftpPathChooser();
    void setupAskpassPathChooser();
    void setupKeygenPathChooser();
    void setupPathChooser(PathChooser &chooser, const FilePath &initialPath, bool &changedFlag);
    void updateCheckboxEnabled();
    void updateSpinboxEnabled();

    QCheckBox m_connectionSharingCheckBox;
    QSpinBox m_connectionSharingSpinBox;
    PathChooser m_sshChooser;
    PathChooser m_sftpChooser;
    PathChooser m_askpassChooser;
    PathChooser m_keygenChooser;
    bool m_sshPathChanged = false;
    bool m_sftpPathChanged = false;
    bool m_askpassPathChanged = false;
    bool m_keygenPathChanged = false;
};

SshSettingsPage::SshSettingsPage()
{
    setId(Constants::SSH_SETTINGS_PAGE_ID);
    setDisplayName(SshSettingsWidget::tr("SSH"));
    setCategory(Constants::DEVICE_SETTINGS_CATEGORY);
    setDisplayCategory(QCoreApplication::translate("ProjectExplorer", "SSH"));
    setCategoryIconPath(":/projectexplorer/images/settingscategory_devices.png");
    setWidgetCreator([] { return new SshSettingsWidget; });
}

SshSettingsWidget::SshSettingsWidget()
{
    setupConnectionSharingCheckBox();
    setupConnectionSharingSpinBox();
    setupSshPathChooser();
    setupSftpPathChooser();
    setupAskpassPathChooser();
    setupKeygenPathChooser();
    auto * const layout = new QFormLayout(this);
    layout->addRow(tr("Enable connection sharing:"), &m_connectionSharingCheckBox);
    layout->addRow(tr("Connection sharing timeout:"), &m_connectionSharingSpinBox);
    layout->addRow(tr("Path to ssh executable:"), &m_sshChooser);
    layout->addRow(tr("Path to sftp executable:"), &m_sftpChooser);
    layout->addRow(tr("Path to ssh-askpass executable:"), &m_askpassChooser);
    layout->addRow(tr("Path to ssh-keygen executable:"), &m_keygenChooser);
    updateCheckboxEnabled();
    updateSpinboxEnabled();
}

void SshSettingsWidget::saveSettings()
{
    SshSettings::setConnectionSharingEnabled(m_connectionSharingCheckBox.isChecked());
    SshSettings::setConnectionSharingTimeout(m_connectionSharingSpinBox.value());
    if (m_sshPathChanged)
        SshSettings::setSshFilePath(m_sshChooser.filePath());
    if (m_sftpPathChanged)
        SshSettings::setSftpFilePath(m_sftpChooser.filePath());
    if (m_askpassPathChanged)
        SshSettings::setAskpassFilePath(m_askpassChooser.filePath());
    if (m_keygenPathChanged)
        SshSettings::setKeygenFilePath(m_keygenChooser.filePath());
    SshSettings::storeSettings(Core::ICore::settings());
}

void SshSettingsWidget::setupConnectionSharingCheckBox()
{
    m_connectionSharingCheckBox.setChecked(SshSettings::connectionSharingEnabled());
    connect(&m_connectionSharingCheckBox, &QCheckBox::toggled,
            this, &SshSettingsWidget::updateSpinboxEnabled);
}

void SshSettingsWidget::setupConnectionSharingSpinBox()
{
    m_connectionSharingSpinBox.setMinimum(1);
    m_connectionSharingSpinBox.setValue(SshSettings::connectionSharingTimeout());
    m_connectionSharingSpinBox.setSuffix(tr(" minutes"));
}

void SshSettingsWidget::setupSshPathChooser()
{
    setupPathChooser(m_sshChooser, SshSettings::sshFilePath(), m_sshPathChanged);
}

void SshSettingsWidget::setupSftpPathChooser()
{
    setupPathChooser(m_sftpChooser, SshSettings::sftpFilePath(), m_sftpPathChanged);
}

void SshSettingsWidget::setupAskpassPathChooser()
{
    setupPathChooser(m_askpassChooser, SshSettings::askpassFilePath(), m_askpassPathChanged);
}

void SshSettingsWidget::setupKeygenPathChooser()
{
    setupPathChooser(m_keygenChooser, SshSettings::keygenFilePath(), m_keygenPathChanged);
}

void SshSettingsWidget::setupPathChooser(PathChooser &chooser, const FilePath &initialPath,
                                         bool &changedFlag)
{
    chooser.setExpectedKind(PathChooser::ExistingCommand);
    chooser.setFilePath(initialPath);
    connect(&chooser, &PathChooser::filePathChanged, [&changedFlag] { changedFlag = true; });
}

void SshSettingsWidget::updateCheckboxEnabled()
{
    if (!Utils::HostOsInfo::isWindowsHost())
        return;
    m_connectionSharingCheckBox.setEnabled(false);
    static_cast<QFormLayout *>(layout())->labelForField(&m_connectionSharingCheckBox)
            ->setEnabled(false);
}

void SshSettingsWidget::updateSpinboxEnabled()
{
    m_connectionSharingSpinBox.setEnabled(m_connectionSharingCheckBox.isChecked());
    static_cast<QFormLayout *>(layout())->labelForField(&m_connectionSharingSpinBox)
            ->setEnabled(m_connectionSharingCheckBox.isChecked());
}

} // namespace Internal
} // namespace ProjectExplorer
