// Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/abi.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/toolchainconfigwidget.h>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTextEdit;
QT_END_NAMESPACE

namespace Utils {
class FilePath;
class PathChooser;
}

namespace ProjectExplorer { class AbiWidget; }

namespace BareMetal {
namespace Internal {

// IarToolChain

class IarToolChain final : public ProjectExplorer::ToolChain
{
    Q_DECLARE_TR_FUNCTIONS(IarToolChain)

public:
    MacroInspectionRunner createMacroInspectionRunner() const final;

    Utils::LanguageExtensions languageExtensions(const QStringList &cxxflags) const final;
    Utils::WarningFlags warningFlags(const QStringList &cxxflags) const final;

    BuiltInHeaderPathsRunner createBuiltInHeaderPathsRunner(const Utils::Environment &) const final;
    void addToEnvironment(Utils::Environment &env) const final;
    QList<Utils::OutputLineParser *> createOutputParsers() const final;

    QVariantMap toMap() const final;
    bool fromMap(const QVariantMap &data) final;

    std::unique_ptr<ProjectExplorer::ToolChainConfigWidget> createConfigurationWidget() final;

    bool operator ==(const ToolChain &other) const final;

    void setExtraCodeModelFlags(const QStringList &flags);
    QStringList extraCodeModelFlags() const final;

    Utils::FilePath makeCommand(const Utils::Environment &env) const final;

private:
    IarToolChain();

    QStringList m_extraCodeModelFlags;

    friend class IarToolChainFactory;
    friend class IarToolChainConfigWidget;
};

// IarToolChainFactory

class IarToolChainFactory final : public ProjectExplorer::ToolChainFactory
{
public:
    IarToolChainFactory();

    ProjectExplorer::Toolchains autoDetect(
            const ProjectExplorer::ToolchainDetector &detector) const final;
    ProjectExplorer::Toolchains detectForImport(
            const ProjectExplorer::ToolChainDescription &tcd) const final;

private:
    ProjectExplorer::Toolchains autoDetectToolchains(const Candidates &candidates,
            const ProjectExplorer::Toolchains &alreadyKnown) const;
    ProjectExplorer::Toolchains autoDetectToolchain(
            const Candidate &candidate, Utils::Id languageId) const;
};

// IarToolChainConfigWidget

class IarToolChainConfigWidget final : public ProjectExplorer::ToolChainConfigWidget
{
    Q_OBJECT

public:
    explicit IarToolChainConfigWidget(IarToolChain *tc);

private:
    void applyImpl() final;
    void discardImpl() final { setFromToolchain(); }
    bool isDirtyImpl() const final;
    void makeReadOnlyImpl() final;

    void setFromToolchain();
    void handleCompilerCommandChange();
    void handlePlatformCodeGenFlagsChange();

    Utils::PathChooser *m_compilerCommand = nullptr;
    ProjectExplorer::AbiWidget *m_abiWidget = nullptr;
    QLineEdit *m_platformCodeGenFlagsLineEdit = nullptr;
    ProjectExplorer::Macros m_macros;
};

} // namespace Internal
} // namespace BareMetal
