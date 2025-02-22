// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "deployconfiguration.h"

#include "buildsteplist.h"
#include "deploymentdataview.h"
#include "kitinformation.h"
#include "project.h"
#include "projectexplorerconstants.h"
#include "target.h"

#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QDebug>

using namespace Utils;

namespace ProjectExplorer {

const char BUILD_STEP_LIST_COUNT[] = "ProjectExplorer.BuildConfiguration.BuildStepListCount";
const char BUILD_STEP_LIST_PREFIX[] = "ProjectExplorer.BuildConfiguration.BuildStepList.";
const char USES_DEPLOYMENT_DATA[] = "ProjectExplorer.DeployConfiguration.CustomDataEnabled";
const char DEPLOYMENT_DATA[] = "ProjectExplorer.DeployConfiguration.CustomData";

DeployConfiguration::DeployConfiguration(Target *target, Id id)
    : ProjectConfiguration(target, id),
      m_stepList(this, Constants::BUILDSTEPS_DEPLOY)
{
    QTC_CHECK(target && target == this->target());
    //: Default DeployConfiguration display name
    setDefaultDisplayName(tr("Deploy locally"));
}

BuildStepList *DeployConfiguration::stepList()
{
    return &m_stepList;
}

const BuildStepList *DeployConfiguration::stepList() const
{
    return &m_stepList;
}

QWidget *DeployConfiguration::createConfigWidget()
{
    if (!m_configWidgetCreator)
        return nullptr;
    return m_configWidgetCreator(this);
}

QVariantMap DeployConfiguration::toMap() const
{
    QVariantMap map(ProjectConfiguration::toMap());
    map.insert(QLatin1String(BUILD_STEP_LIST_COUNT), 1);
    map.insert(QLatin1String(BUILD_STEP_LIST_PREFIX) + QLatin1Char('0'), m_stepList.toMap());
    map.insert(USES_DEPLOYMENT_DATA, usesCustomDeploymentData());
    QVariantMap deployData;
    for (int i = 0; i < m_customDeploymentData.fileCount(); ++i) {
        const DeployableFile &f = m_customDeploymentData.fileAt(i);
        deployData.insert(f.localFilePath().toString(), f.remoteDirectory());
    }
    map.insert(DEPLOYMENT_DATA, deployData);
    return map;
}

bool DeployConfiguration::fromMap(const QVariantMap &map)
{
    if (!ProjectConfiguration::fromMap(map))
        return false;

    int maxI = map.value(QLatin1String(BUILD_STEP_LIST_COUNT), 0).toInt();
    if (maxI != 1)
        return false;
    QVariantMap data = map.value(QLatin1String(BUILD_STEP_LIST_PREFIX) + QLatin1Char('0')).toMap();
    if (!data.isEmpty()) {
        m_stepList.clear();
        if (!m_stepList.fromMap(data)) {
            qWarning() << "Failed to restore deploy step list";
            m_stepList.clear();
            return false;
        }
    } else {
        qWarning() << "No data for deploy step list found!";
        return false;
    }

    m_usesCustomDeploymentData = map.value(USES_DEPLOYMENT_DATA, false).toBool();
    const QVariantMap deployData = map.value(DEPLOYMENT_DATA).toMap();
    for (auto it = deployData.begin(); it != deployData.end(); ++it)
        m_customDeploymentData.addFile(FilePath::fromString(it.key()), it.value().toString());
    return true;
}

bool DeployConfiguration::isActive() const
{
    return target()->isActive() && target()->activeDeployConfiguration() == this;
}


///
// DeployConfigurationFactory
///

static QList<DeployConfigurationFactory *> g_deployConfigurationFactories;

DeployConfigurationFactory::DeployConfigurationFactory()
{
    g_deployConfigurationFactories.append(this);
}

DeployConfigurationFactory::~DeployConfigurationFactory()
{
    g_deployConfigurationFactories.removeOne(this);
}

Id DeployConfigurationFactory::creationId() const
{
    return m_deployConfigBaseId;
}

QString DeployConfigurationFactory::defaultDisplayName() const
{
    return m_defaultDisplayName;
}

bool DeployConfigurationFactory::canHandle(Target *target) const
{
    if (m_supportedProjectType.isValid()) {
        if (target->project()->id() != m_supportedProjectType)
            return false;
    }

    if (containsType(target->project()->projectIssues(target->kit()), Task::TaskType::Error))
        return false;

    if (!m_supportedTargetDeviceTypes.isEmpty()) {
        if (!m_supportedTargetDeviceTypes.contains(
                    DeviceTypeKitAspect::deviceTypeId(target->kit())))
            return false;
    }

    return true;
}

void DeployConfigurationFactory::setConfigWidgetCreator(const DeployConfiguration::WidgetCreator &configWidgetCreator)
{
    m_configWidgetCreator = configWidgetCreator;
}

void DeployConfigurationFactory::setUseDeploymentDataView()
{
    m_configWidgetCreator = [](DeployConfiguration *dc) {
        return new Internal::DeploymentDataView(dc);
    };
}

void DeployConfigurationFactory::setConfigBaseId(Id deployConfigBaseId)
{
    m_deployConfigBaseId = deployConfigBaseId;
}

DeployConfiguration *DeployConfigurationFactory::createDeployConfiguration(Target *t)
{
    auto dc = new DeployConfiguration(t, m_deployConfigBaseId);
    dc->setDefaultDisplayName(m_defaultDisplayName);
    dc->m_configWidgetCreator = m_configWidgetCreator;
    return dc;
}

DeployConfiguration *DeployConfigurationFactory::create(Target *parent)
{
    QTC_ASSERT(canHandle(parent), return nullptr);
    DeployConfiguration *dc = createDeployConfiguration(parent);
    QTC_ASSERT(dc, return nullptr);
    BuildStepList *stepList = dc->stepList();
    for (const BuildStepList::StepCreationInfo &info : qAsConst(m_initialSteps)) {
        if (!info.condition || info.condition(parent))
            stepList->appendStep(info.stepId);
    }
    return dc;
}

DeployConfiguration *DeployConfigurationFactory::clone(Target *parent,
                                                       const DeployConfiguration *source)
{
    return restore(parent, source->toMap());
}

DeployConfiguration *DeployConfigurationFactory::restore(Target *parent, const QVariantMap &map)
{
    const Id id = idFromMap(map);
    DeployConfigurationFactory *factory = Utils::findOrDefault(g_deployConfigurationFactories,
        [parent, id](DeployConfigurationFactory *f) {
            if (!f->canHandle(parent))
                return false;
            return id.name().startsWith(f->m_deployConfigBaseId.name());
        });
    if (!factory)
        return nullptr;
    DeployConfiguration *dc = factory->createDeployConfiguration(parent);
    QTC_ASSERT(dc, return nullptr);
    if (!dc->fromMap(map)) {
        delete dc;
        dc = nullptr;
    } else if (factory->postRestore()) {
        factory->postRestore()(dc, map);
    }

    return dc;
}

const QList<DeployConfigurationFactory *> DeployConfigurationFactory::find(Target *parent)
{
    return Utils::filtered(g_deployConfigurationFactories,
        [&parent](DeployConfigurationFactory *factory) {
            return factory->canHandle(parent);
        });
}

void DeployConfigurationFactory::addSupportedTargetDeviceType(Utils::Id id)
{
    m_supportedTargetDeviceTypes.append(id);
}

void DeployConfigurationFactory::setDefaultDisplayName(const QString &defaultDisplayName)
{
    m_defaultDisplayName = defaultDisplayName;
}

void DeployConfigurationFactory::setSupportedProjectType(Utils::Id id)
{
    m_supportedProjectType = id;
}

void DeployConfigurationFactory::addInitialStep(Utils::Id stepId, const std::function<bool (Target *)> &condition)
{
    m_initialSteps.append({stepId, condition});
}

///
// DefaultDeployConfigurationFactory
///

DefaultDeployConfigurationFactory::DefaultDeployConfigurationFactory()
{
    setConfigBaseId("ProjectExplorer.DefaultDeployConfiguration");
    addSupportedTargetDeviceType(Constants::DESKTOP_DEVICE_TYPE);
    //: Display name of the default deploy configuration
    setDefaultDisplayName(DeployConfiguration::tr("Deploy Configuration"));
}

} // namespace ProjectExplorer
