// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "remotelinux_export.h"

#include "abstractremotelinuxdeploystep.h"

namespace RemoteLinux {

class REMOTELINUX_EXPORT RsyncDeployStep : public AbstractRemoteLinuxDeployStep
{
    Q_OBJECT

public:
    RsyncDeployStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
    ~RsyncDeployStep() override;

    static Utils::Id stepId();
    static QString displayName();
};

} // RemoteLinux
