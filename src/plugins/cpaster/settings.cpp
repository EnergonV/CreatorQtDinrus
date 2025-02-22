// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "settings.h"

#include "cpasterconstants.h"

#include <utils/layoutbuilder.h>

using namespace Utils;

namespace CodePaster {

Settings::Settings()
{
    setSettingsGroup("CodePaster");
    setAutoApply(false);

    registerAspect(&username);
    username.setDisplayStyle(StringAspect::LineEditDisplay);
    username.setSettingsKey("UserName");
    username.setLabelText(tr("Username:"));

    registerAspect(&protocols);
    protocols.setSettingsKey("DefaultProtocol");
    protocols.setDisplayStyle(SelectionAspect::DisplayStyle::ComboBox);
    protocols.setLabelText(tr("Default protocol:"));
    protocols.setToSettingsTransformation([this](const QVariant &val) {
        return protocols.displayForIndex(val.toInt());
    });
    protocols.setFromSettingsTransformation([this](const QVariant &val) {
        return protocols.indexForDisplay(val.toString());
    });

    registerAspect(&expiryDays);
    expiryDays.setSettingsKey("ExpiryDays");
    expiryDays.setDefaultValue(1);
    expiryDays.setSuffix(tr(" Days"));
    expiryDays.setLabelText(tr("&Expires after:"));

    registerAspect(&copyToClipboard);
    copyToClipboard.setSettingsKey("CopyToClipboard");
    copyToClipboard.setDefaultValue(true);
    copyToClipboard.setLabelText(tr("Copy-paste URL to clipboard"));

    registerAspect(&displayOutput);
    displayOutput.setSettingsKey("DisplayOutput");
    displayOutput.setDefaultValue(true);
    displayOutput.setLabelText(tr("Display General Messages after sending a post"));
}

// SettingsPage

SettingsPage::SettingsPage(Settings *settings)
{
    setId("A.CodePaster.General");
    setDisplayName(Settings::tr("General"));
    setCategory(Constants::CPASTER_SETTINGS_CATEGORY);
    setDisplayCategory(Settings::tr("Code Pasting"));
    setCategoryIconPath(":/cpaster/images/settingscategory_cpaster.png");
    setSettings(settings);

    setLayouter([settings](QWidget *widget) {
        Settings &s = *settings;
        using namespace Layouting;

        Column {
            Form {
                s.protocols,
                s.username,
                s.expiryDays
            },
            s.copyToClipboard,
            s.displayOutput,
            st
        }.attachTo(widget);
    });
}

} // namespace CodePaster
