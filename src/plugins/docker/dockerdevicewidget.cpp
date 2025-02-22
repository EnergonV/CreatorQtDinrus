// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "dockerdevicewidget.h"

#include "dockerapi.h"
#include "dockerdevice.h"
#include "dockertr.h"

#include <utils/algorithm.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>
#include <utils/utilsicons.h>

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QToolButton>

using namespace ProjectExplorer;
using namespace Utils;

namespace Docker::Internal {

DockerDeviceWidget::DockerDeviceWidget(const IDevice::Ptr &device)
    : IDeviceWidget(device), m_kitItemDetector(device)
{
    auto dockerDevice = device.dynamicCast<DockerDevice>();
    QTC_ASSERT(dockerDevice, return);

    DockerDeviceData &data = dockerDevice->data();

    auto repoLabel = new QLabel(Tr::tr("Repository:"));
    m_repoLineEdit = new QLineEdit;
    m_repoLineEdit->setText(data.repo);
    m_repoLineEdit->setEnabled(false);

    auto tagLabel = new QLabel(Tr::tr("Tag:"));
    m_tagLineEdit = new QLineEdit;
    m_tagLineEdit->setText(data.tag);
    m_tagLineEdit->setEnabled(false);

    auto idLabel = new QLabel(Tr::tr("Image ID:"));
    m_idLineEdit = new QLineEdit;
    m_idLineEdit->setText(data.imageId);
    m_idLineEdit->setEnabled(false);

    auto daemonStateLabel = new QLabel(Tr::tr("Daemon state:"));
    m_daemonReset = new QToolButton;
    m_daemonReset->setToolTip(Tr::tr("Clears detected daemon state. "
        "It will be automatically re-evaluated next time access is needed."));

    m_daemonState = new QLabel;

    connect(DockerApi::instance(), &DockerApi::dockerDaemonAvailableChanged, this, [this]{
        updateDaemonStateTexts();
    });

    updateDaemonStateTexts();

    connect(m_daemonReset, &QToolButton::clicked, this, [] {
        DockerApi::recheckDockerDaemon();
    });

    m_runAsOutsideUser = new QCheckBox(Tr::tr("Run as outside user"));
    m_runAsOutsideUser->setToolTip(Tr::tr("Uses user ID and group ID of the user running Qt Creator "
                                          "in the docker container."));
    m_runAsOutsideUser->setChecked(data.useLocalUidGid);
    m_runAsOutsideUser->setEnabled(HostOsInfo::isLinuxHost());

    connect(m_runAsOutsideUser, &QCheckBox::toggled, this, [&data](bool on) {
        data.useLocalUidGid = on;
    });

    auto pathListLabel = new InfoLabel(Tr::tr("Paths to mount:"));
    pathListLabel->setAdditionalToolTip(Tr::tr("Source directory list should not be empty."));

    m_pathsListEdit = new PathListEditor;
    m_pathsListEdit->setPlaceholderText(Tr::tr("Host directories to mount into the container"));
    m_pathsListEdit->setToolTip(Tr::tr("Maps paths in this list one-to-one to the "
                                       "docker container."));
    m_pathsListEdit->setPathList(data.mounts);
    m_pathsListEdit->setMaximumHeight(100);
    m_pathsListEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto markupMounts = [this, pathListLabel] {
        const bool isEmpty = m_pathsListEdit->pathList().isEmpty();
        pathListLabel->setType(isEmpty ? InfoLabel::Warning : InfoLabel::None);
    };
    markupMounts();

    connect(m_pathsListEdit, &PathListEditor::changed, this, [dockerDevice, markupMounts, this] {
        dockerDevice->setMounts(m_pathsListEdit->pathList());
        markupMounts();
    });

    auto logView = new QTextBrowser;
    connect(&m_kitItemDetector, &KitDetector::logOutput,
            logView, &QTextBrowser::append);

    auto autoDetectButton = new QPushButton(Tr::tr("Auto-detect Kit Items"));
    auto undoAutoDetectButton = new QPushButton(Tr::tr("Remove Auto-Detected Kit Items"));
    auto listAutoDetectedButton = new QPushButton(Tr::tr("List Auto-Detected Kit Items"));

    auto searchDirsComboBox = new QComboBox;
    searchDirsComboBox->addItem(Tr::tr("Search in PATH"));
    searchDirsComboBox->addItem(Tr::tr("Search in Selected Directories"));

    auto searchDirsLineEdit = new FancyLineEdit;

    searchDirsLineEdit->setPlaceholderText(Tr::tr("Semicolon-separated list of directories"));
    searchDirsLineEdit->setToolTip(
        Tr::tr("Select the paths in the docker image that should be scanned for kit entries."));
    searchDirsLineEdit->setHistoryCompleter("DockerMounts", true);

    auto searchPaths = [searchDirsComboBox, searchDirsLineEdit, dockerDevice] {
        FilePaths paths;
        if (searchDirsComboBox->currentIndex() == 0) {
            paths = dockerDevice->systemEnvironment().path();
        } else {
            for (const QString &path : searchDirsLineEdit->text().split(';'))
                paths.append(FilePath::fromString(path.trimmed()));
        }
        paths = Utils::transform(paths, [dockerDevice](const FilePath &path) {
            return dockerDevice->mapToGlobalPath(path);
        });
        return paths;
    };

    connect(autoDetectButton, &QPushButton::clicked, this,
            [this, logView, dockerDevice, searchPaths] {
        logView->clear();
        dockerDevice->updateContainerAccess();

        m_kitItemDetector.autoDetect(dockerDevice->id().toString(), searchPaths());

        if (DockerApi::instance()->dockerDaemonAvailable().value_or(false) == false)
            logView->append(Tr::tr("Docker daemon appears to be not running."));
        else
            logView->append(Tr::tr("Docker daemon appears to be running."));
        updateDaemonStateTexts();
    });

    connect(undoAutoDetectButton, &QPushButton::clicked, this, [this, logView, device] {
        logView->clear();
        m_kitItemDetector.undoAutoDetect(device->id().toString());
    });

    connect(listAutoDetectedButton, &QPushButton::clicked, this, [this, logView, device] {
        logView->clear();
        m_kitItemDetector.listAutoDetected(device->id().toString());
    });

    using namespace Layouting;

    Form {
        repoLabel, m_repoLineEdit, br,
        tagLabel, m_tagLineEdit, br,
        idLabel, m_idLineEdit, br,
        daemonStateLabel, m_daemonReset, m_daemonState, br,
        m_runAsOutsideUser, br,
        Column {
            pathListLabel,
            m_pathsListEdit,
        }, br,
        Column {
            Space(20),
            Row {
                searchDirsComboBox,
                searchDirsLineEdit
            },
            Row {
                autoDetectButton,
                undoAutoDetectButton,
                listAutoDetectedButton,
                st,
            },
            Tr::tr("Detection log:"),
            logView
        }
    }.attachTo(this);

    searchDirsLineEdit->setVisible(false);
    auto updateDirectoriesLineEdit = [searchDirsLineEdit](int index) {
        searchDirsLineEdit->setVisible(index == 1);
        if (index == 1)
            searchDirsLineEdit->setFocus();
    };
    QObject::connect(searchDirsComboBox, &QComboBox::activated, this, updateDirectoriesLineEdit);
}

void DockerDeviceWidget::updateDaemonStateTexts()
{
    Utils::optional<bool> daemonState = DockerApi::instance()->dockerDaemonAvailable();
    if (!daemonState.has_value()) {
        m_daemonReset->setIcon(Icons::INFO.icon());
        m_daemonState->setText(Tr::tr("Daemon state not evaluated."));
    } else if (daemonState.value()) {
        m_daemonReset->setIcon(Icons::OK.icon());
        m_daemonState->setText(Tr::tr("Docker daemon running."));
    } else {
        m_daemonReset->setIcon(Icons::CRITICAL.icon());
        m_daemonState->setText(Tr::tr("Docker daemon not running."));
    }
}

} // Docker::Internal
