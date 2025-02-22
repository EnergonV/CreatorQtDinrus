// Copyright (C) 2016 Denis Shienkov <denis.shienkov@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "stlinkutilgdbserverprovider.h"

#include <baremetal/baremetalconstants.h>
#include <baremetal/debugserverprovidermanager.h>

#include <utils/fileutils.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>
#include <utils/variablechooser.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QSpinBox>

using namespace Utils;

namespace BareMetal {
namespace Internal {

const char executableFileKeyC[] = "ExecutableFile";
const char verboseLevelKeyC[] = "VerboseLevel";
const char extendedModeKeyC[] = "ExtendedMode";
const char resetBoardKeyC[] = "ResetBoard";
const char transportLayerKeyC[] = "TransportLayer";

// StLinkUtilGdbServerProvider

StLinkUtilGdbServerProvider::StLinkUtilGdbServerProvider()
    : GdbServerProvider(Constants::GDBSERVER_STLINK_UTIL_PROVIDER_ID)
{
    setInitCommands(defaultInitCommands());
    setResetCommands(defaultResetCommands());
    setChannel("localhost", 4242);
    setTypeDisplayName(GdbServerProvider::tr("ST-LINK Utility"));
    setConfigurationWidgetCreator([this] { return new StLinkUtilGdbServerProviderConfigWidget(this); });
}

QString StLinkUtilGdbServerProvider::defaultInitCommands()
{
    return {"load\n"};
}

QString StLinkUtilGdbServerProvider::defaultResetCommands()
{
    return {};
}

QString StLinkUtilGdbServerProvider::channelString() const
{
    switch (startupMode()) {
    case StartupOnNetwork:
        // Just return as "host:port" form.
        return GdbServerProvider::channelString();
    case StartupOnPipe:
        // Unsupported mode
        return {};
    default: // wrong
        return {};
    }
}

CommandLine StLinkUtilGdbServerProvider::command() const
{
    CommandLine cmd{m_executableFile, {}};

    if (m_extendedMode)
        cmd.addArg("--multi");

    if (!m_resetBoard)
        cmd.addArg("--no-reset");

    if (m_transport != UnspecifiedTransport)
        cmd.addArg("--stlink_version=" + QString::number(m_transport));
    cmd.addArg("--listen_port=" + QString::number(channel().port()));
    cmd.addArg("--verbose=" + QString::number(m_verboseLevel));

    return cmd;
}

QSet<GdbServerProvider::StartupMode>
StLinkUtilGdbServerProvider::supportedStartupModes() const
{
    return {StartupOnNetwork};
}

bool StLinkUtilGdbServerProvider::isValid() const
{
    if (!GdbServerProvider::isValid())
        return false;

    const StartupMode m = startupMode();

    if (m == StartupOnNetwork) {
        if (channel().host().isEmpty())
            return false;
    }

    if (m == StartupOnNetwork) {
        if (m_executableFile.isEmpty())
            return false;
    }

    return true;
}

QVariantMap StLinkUtilGdbServerProvider::toMap() const
{
    QVariantMap data = GdbServerProvider::toMap();
    data.insert(executableFileKeyC, m_executableFile.toVariant());
    data.insert(verboseLevelKeyC, m_verboseLevel);
    data.insert(extendedModeKeyC, m_extendedMode);
    data.insert(resetBoardKeyC, m_resetBoard);
    data.insert(transportLayerKeyC, m_transport);
    return data;
}

bool StLinkUtilGdbServerProvider::fromMap(const QVariantMap &data)
{
    if (!GdbServerProvider::fromMap(data))
        return false;

    m_executableFile = FilePath::fromVariant(data.value(executableFileKeyC));
    m_verboseLevel = data.value(verboseLevelKeyC).toInt();
    m_extendedMode = data.value(extendedModeKeyC).toBool();
    m_resetBoard = data.value(resetBoardKeyC).toBool();
    m_transport = static_cast<TransportLayer>(
                data.value(transportLayerKeyC).toInt());
    return true;
}

bool StLinkUtilGdbServerProvider::operator==(const IDebugServerProvider &other) const
{
    if (!GdbServerProvider::operator==(other))
        return false;

    const auto p = static_cast<const StLinkUtilGdbServerProvider *>(&other);
    return m_executableFile == p->m_executableFile
            && m_verboseLevel == p->m_verboseLevel
            && m_extendedMode == p->m_extendedMode
            && m_resetBoard == p->m_resetBoard
            && m_transport == p->m_transport;
}

// StLinkUtilGdbServerProviderFactory

StLinkUtilGdbServerProviderFactory::StLinkUtilGdbServerProviderFactory()
{
    setId(Constants::GDBSERVER_STLINK_UTIL_PROVIDER_ID);
    setDisplayName(GdbServerProvider::tr("ST-LINK Utility"));
    setCreator([] { return new StLinkUtilGdbServerProvider; });
}

// StLinkUtilGdbServerProviderConfigWidget

StLinkUtilGdbServerProviderConfigWidget::StLinkUtilGdbServerProviderConfigWidget(
        StLinkUtilGdbServerProvider *p)
    : GdbServerProviderConfigWidget(p)
{
    Q_ASSERT(p);

    m_hostWidget = new HostWidget(this);
    m_mainLayout->addRow(tr("Host:"), m_hostWidget);

    m_executableFileChooser = new Utils::PathChooser;
    m_executableFileChooser->setExpectedKind(Utils::PathChooser::ExistingCommand);
    m_mainLayout->addRow(tr("Executable file:"), m_executableFileChooser);

    m_verboseLevelSpinBox = new QSpinBox;
    m_verboseLevelSpinBox->setRange(0, 99);
    m_verboseLevelSpinBox->setToolTip(tr("Specify the verbosity level (0..99)."));
    m_mainLayout->addRow(tr("Verbosity level:"), m_verboseLevelSpinBox);

    m_extendedModeCheckBox = new QCheckBox;
    m_extendedModeCheckBox->setToolTip(tr("Continue listening for connections "
                                          "after disconnect."));
    m_mainLayout->addRow(tr("Extended mode:"), m_extendedModeCheckBox);

    m_resetBoardCheckBox = new QCheckBox;
    m_resetBoardCheckBox->setToolTip(tr("Reset board on connection."));
    m_mainLayout->addRow(tr("Reset on connection:"), m_resetBoardCheckBox);

    m_transportLayerComboBox = new QComboBox;
    m_transportLayerComboBox->setToolTip(tr("Transport layer type."));
    m_mainLayout->addRow(tr("Version:"), m_transportLayerComboBox);

    m_initCommandsTextEdit = new QPlainTextEdit(this);
    m_initCommandsTextEdit->setToolTip(defaultInitCommandsTooltip());
    m_mainLayout->addRow(tr("Init commands:"), m_initCommandsTextEdit);
    m_resetCommandsTextEdit = new QPlainTextEdit(this);
    m_resetCommandsTextEdit->setToolTip(defaultResetCommandsTooltip());
    m_mainLayout->addRow(tr("Reset commands:"), m_resetCommandsTextEdit);

    populateTransportLayers();
    addErrorLabel();
    setFromProvider();

    const auto chooser = new VariableChooser(this);
    chooser->addSupportedWidget(m_initCommandsTextEdit);
    chooser->addSupportedWidget(m_resetCommandsTextEdit);

    connect(m_hostWidget, &HostWidget::dataChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_executableFileChooser, &Utils::PathChooser::rawPathChanged,
            this, &GdbServerProviderConfigWidget::dirty);

    connect(m_verboseLevelSpinBox, &QSpinBox::valueChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_extendedModeCheckBox, &QAbstractButton::clicked,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_resetBoardCheckBox, &QAbstractButton::clicked,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_transportLayerComboBox, &QComboBox::currentIndexChanged,
            this, &GdbServerProviderConfigWidget::dirty);

    connect(m_initCommandsTextEdit, &QPlainTextEdit::textChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_resetCommandsTextEdit, &QPlainTextEdit::textChanged,
            this, &GdbServerProviderConfigWidget::dirty);
}

void StLinkUtilGdbServerProviderConfigWidget::apply()
{
    const auto p = static_cast<StLinkUtilGdbServerProvider *>(m_provider);
    Q_ASSERT(p);

    p->setChannel(m_hostWidget->channel());
    p->m_executableFile = m_executableFileChooser->filePath();
    p->m_verboseLevel = m_verboseLevelSpinBox->value();
    p->m_extendedMode = m_extendedModeCheckBox->isChecked();
    p->m_resetBoard = m_resetBoardCheckBox->isChecked();
    p->m_transport = transportLayer();
    p->setInitCommands(m_initCommandsTextEdit->toPlainText());
    p->setResetCommands(m_resetCommandsTextEdit->toPlainText());
    GdbServerProviderConfigWidget::apply();
}

void StLinkUtilGdbServerProviderConfigWidget::discard()
{
    setFromProvider();
    GdbServerProviderConfigWidget::discard();
}

StLinkUtilGdbServerProvider::TransportLayer
StLinkUtilGdbServerProviderConfigWidget::transportLayerFromIndex(int idx) const
{
    return static_cast<StLinkUtilGdbServerProvider::TransportLayer>(
                m_transportLayerComboBox->itemData(idx).toInt());
}

StLinkUtilGdbServerProvider::TransportLayer
StLinkUtilGdbServerProviderConfigWidget::transportLayer() const
{
    const int idx = m_transportLayerComboBox->currentIndex();
    return transportLayerFromIndex(idx);
}

void StLinkUtilGdbServerProviderConfigWidget::setTransportLayer(
        StLinkUtilGdbServerProvider::TransportLayer tl)
{
    for (int idx = 0; idx < m_transportLayerComboBox->count(); ++idx) {
        if (tl == transportLayerFromIndex(idx)) {
            m_transportLayerComboBox->setCurrentIndex(idx);
            break;
        }
    }
}

void StLinkUtilGdbServerProviderConfigWidget::populateTransportLayers()
{
    m_transportLayerComboBox->insertItem(
                m_transportLayerComboBox->count(), tr("ST-LINK/V1"),
                StLinkUtilGdbServerProvider::ScsiOverUsb);
    m_transportLayerComboBox->insertItem(
                m_transportLayerComboBox->count(), tr("ST-LINK/V2"),
                StLinkUtilGdbServerProvider::RawUsb);
    m_transportLayerComboBox->insertItem(
                m_transportLayerComboBox->count(), tr("Keep unspecified"),
                StLinkUtilGdbServerProvider::UnspecifiedTransport);
}

void StLinkUtilGdbServerProviderConfigWidget::setFromProvider()
{
    const auto p = static_cast<StLinkUtilGdbServerProvider *>(m_provider);
    Q_ASSERT(p);

    const QSignalBlocker blocker(this);
    m_hostWidget->setChannel(p->channel());
    m_executableFileChooser->setFilePath(p->m_executableFile);
    m_verboseLevelSpinBox->setValue(p->m_verboseLevel);
    m_extendedModeCheckBox->setChecked(p->m_extendedMode);
    m_resetBoardCheckBox->setChecked(p->m_resetBoard);
    setTransportLayer(p->m_transport);
    m_initCommandsTextEdit->setPlainText(p->initCommands());
    m_resetCommandsTextEdit->setPlainText(p->resetCommands());
}

} // namespace Internal
} // namespace ProjectExplorer
