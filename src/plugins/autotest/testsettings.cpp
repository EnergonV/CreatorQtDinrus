// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "testsettings.h"

#include "autotestconstants.h"
#include "testframeworkmanager.h"

#include <utils/id.h>

#include <QSettings>

namespace Autotest {
namespace Internal {

static const char timeoutKey[]                  = "Timeout";
static const char omitInternalKey[]             = "OmitInternal";
static const char omitRunConfigWarnKey[]        = "OmitRCWarnings";
static const char limitResultOutputKey[]        = "LimitResultOutput";
static const char limitResultDescriptionKey[]   = "LimitResultDescription";
static const char resultDescriptionMaxSizeKey[] = "ResultDescriptionMaxSize";
static const char autoScrollKey[]               = "AutoScrollResults";
static const char processArgsKey[]              = "ProcessArgs";
static const char displayApplicationKey[]       = "DisplayApp";
static const char popupOnStartKey[]             = "PopupOnStart";
static const char popupOnFinishKey[]            = "PopupOnFinish";
static const char popupOnFailKey[]              = "PopupOnFail";
static const char runAfterBuildKey[]            = "RunAfterBuild";
static const char groupSuffix[]                 = ".group";

constexpr int defaultTimeout = 60000;

TestSettings::TestSettings()
    : timeout(defaultTimeout)
{
}

void TestSettings::toSettings(QSettings *s) const
{
    s->beginGroup(Constants::SETTINGSGROUP);
    s->setValue(timeoutKey, timeout);
    s->setValue(omitInternalKey, omitInternalMssg);
    s->setValue(omitRunConfigWarnKey, omitRunConfigWarn);
    s->setValue(limitResultOutputKey, limitResultOutput);
    s->setValue(limitResultDescriptionKey, limitResultDescription);
    s->setValue(resultDescriptionMaxSizeKey, resultDescriptionMaxSize);
    s->setValue(autoScrollKey, autoScroll);
    s->setValue(processArgsKey, processArgs);
    s->setValue(displayApplicationKey, displayApplication);
    s->setValue(popupOnStartKey, popupOnStart);
    s->setValue(popupOnFinishKey, popupOnFinish);
    s->setValue(popupOnFailKey, popupOnFail);
    s->setValue(runAfterBuildKey, int(runAfterBuild));
    // store frameworks and their current active and grouping state
    for (auto it = frameworks.cbegin(); it != frameworks.cend(); ++it) {
        const Utils::Id &id = it.key();
        s->setValue(id.toString(), it.value());
        s->setValue(id.toString() + groupSuffix, frameworksGrouping.value(id));
    }
    // ..and the testtools as well
    for (auto it = tools.cbegin(); it != tools.cend(); ++it)
        s->setValue(it.key().toString(), it.value());
    s->endGroup();
}

void TestSettings::fromSettings(QSettings *s)
{
    s->beginGroup(Constants::SETTINGSGROUP);
    timeout = s->value(timeoutKey, defaultTimeout).toInt();
    omitInternalMssg = s->value(omitInternalKey, true).toBool();
    omitRunConfigWarn = s->value(omitRunConfigWarnKey, false).toBool();
    limitResultOutput = s->value(limitResultOutputKey, true).toBool();
    limitResultDescription = s->value(limitResultDescriptionKey, false).toBool();
    resultDescriptionMaxSize = s->value(resultDescriptionMaxSizeKey, 10).toInt();
    autoScroll = s->value(autoScrollKey, true).toBool();
    processArgs = s->value(processArgsKey, false).toBool();
    displayApplication = s->value(displayApplicationKey, false).toBool();
    popupOnStart = s->value(popupOnStartKey, true).toBool();
    popupOnFinish = s->value(popupOnFinishKey, true).toBool();
    popupOnFail = s->value(popupOnFailKey, false).toBool();
    runAfterBuild = RunAfterBuildMode(s->value(runAfterBuildKey,
                                               int(RunAfterBuildMode::None)).toInt());
    // try to get settings for registered frameworks
    const TestFrameworks &registered = TestFrameworkManager::registeredFrameworks();
    frameworks.clear();
    frameworksGrouping.clear();
    for (const ITestFramework *framework : registered) {
        // get their active state
        const Utils::Id id = framework->id();
        const QString key = id.toString();
        frameworks.insert(id, s->value(key, framework->active()).toBool());
        // and whether grouping is enabled
        frameworksGrouping.insert(id, s->value(key + groupSuffix, framework->grouping()).toBool());
    }
    // ..and for test tools as well
    const TestTools &registeredTools = TestFrameworkManager::registeredTestTools();
    tools.clear();
    for (const ITestTool *testTool : registeredTools) {
        const Utils::Id id = testTool->id();
        tools.insert(id, s->value(id.toString(), testTool->active()).toBool());
    }
    s->endGroup();
}

} // namespace Internal
} // namespace Autotest
