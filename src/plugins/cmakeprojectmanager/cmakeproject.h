// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.h"

#include <projectexplorer/project.h>

namespace CMakeProjectManager {

namespace Internal { class CMakeProjectImporter; }

class CMAKE_EXPORT CMakeProject final : public ProjectExplorer::Project
{
    Q_OBJECT

public:
    explicit CMakeProject(const Utils::FilePath &filename);
    ~CMakeProject() final;

    ProjectExplorer::Tasks projectIssues(const ProjectExplorer::Kit *k) const final;

    ProjectExplorer::ProjectImporter *projectImporter() const final;

    using IssueType = ProjectExplorer::Task::TaskType;
    void addIssue(IssueType type, const QString &text);
    void clearIssues();

protected:
    bool setupTarget(ProjectExplorer::Target *t) final;

private:
    ProjectExplorer::DeploymentKnowledge deploymentKnowledge() const override;

    mutable Internal::CMakeProjectImporter *m_projectImporter = nullptr;

    ProjectExplorer::Tasks m_issues;
};

} // namespace CMakeProjectManager
