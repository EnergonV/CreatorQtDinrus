// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "clangfileinfo.h"
#include "clangtoolsdiagnostic.h"
#include "clangtoolsdiagnosticmodel.h"
#include "clangtoolslogfilereader.h"

#include <debugger/debuggermainwindow.h>

#include <projectexplorer/runconfiguration.h>
#include <cppeditor/projectinfo.h>

#include <variant>

QT_BEGIN_NAMESPACE
class QFrame;
class QToolButton;
QT_END_NAMESPACE

namespace CppEditor {
class ClangDiagnosticConfig;
}
namespace Debugger {
class DetailedErrorView;
}
namespace ProjectExplorer {
class RunControl;
}
namespace Utils {
class FilePath;
class FancyLineEdit;
} // namespace Utils

namespace ClangTools {
namespace Internal {

class InfoBarWidget;
class ClangToolsDiagnosticModel;
class ClangToolRunWorker;
class Diagnostic;
class DiagnosticFilterModel;
class DiagnosticView;
class RunSettings;
class SelectFixitsCheckBox;

const char ClangTidyClazyPerspectiveId[] = "ClangTidyClazy.Perspective";

class ClangTool : public QObject
{
    Q_OBJECT

public:
    static ClangTool *instance();

    ClangTool();

    void selectPerspective();

    enum class FileSelectionType {
        AllFiles,
        CurrentFile,
        AskUser,
    };

    using FileSelection = std::variant<FileSelectionType, Utils::FilePath>;

    void startTool(FileSelection fileSelection);
    void startTool(FileSelection fileSelection,
                   const RunSettings &runSettings,
                   const CppEditor::ClangDiagnosticConfig &diagnosticConfig);

    Diagnostics read(OutputFileFormat outputFileFormat,
                     const QString &logFilePath,
                     const QSet<Utils::FilePath> &projectFiles,
                     QString *errorMessage) const;

    FileInfos collectFileInfos(ProjectExplorer::Project *project,
                               FileSelection fileSelection);

    // For testing.
    QSet<Diagnostic> diagnostics() const;

    const QString &name() const;

    void onNewDiagnosticsAvailable(const Diagnostics &diagnostics, bool generateMarks);

    QAction *startAction() const { return m_startAction; }
    QAction *startOnCurrentFileAction() const { return m_startOnCurrentFileAction; }

signals:
    void finished(const QString &errorText); // For testing.

private:
    enum class State {
        Initial,
        PreparationStarted,
        PreparationFailed,
        AnalyzerRunning,
        StoppedByUser,
        AnalyzerFinished,
        ImportFinished,
    };
    void setState(State state);
    void update();
    void updateForCurrentState();
    void updateForInitialState();

    void help();

    void filter();
    void clearFilter();
    void filterForCurrentKind();
    void filterOutCurrentKind();
    void setFilterOptions(const OptionalFilterOptions &filterOptions);

    void onBuildFailed();
    void onStartFailed();
    void onStarted();
    void onRunControlStopped();

    void initDiagnosticView();
    void loadDiagnosticsFromFiles();

    DiagnosticItem *diagnosticItem(const QModelIndex &index) const;
    void showOutputPane();

    void reset();

    FileInfoProviders fileInfoProviders(ProjectExplorer::Project *project,
                                        const FileInfos &allFileInfos);

    ClangToolsDiagnosticModel *m_diagnosticModel = nullptr;
    ProjectExplorer::RunControl *m_runControl = nullptr;
    ClangToolRunWorker *m_runWorker = nullptr;

    InfoBarWidget *m_infoBarWidget = nullptr;
    DiagnosticView *m_diagnosticView = nullptr;;

    QAction *m_startAction = nullptr;
    QAction *m_startOnCurrentFileAction = nullptr;
    QAction *m_stopAction = nullptr;

    State m_state = State::Initial;
    int m_filesCount = 0;
    int m_filesSucceeded = 0;
    int m_filesFailed = 0;

    DiagnosticFilterModel *m_diagnosticFilterModel = nullptr;

    QAction *m_showFilter = nullptr;
    SelectFixitsCheckBox *m_selectFixitsCheckBox = nullptr;
    QToolButton *m_applyFixitsButton = nullptr;

    QAction *m_openProjectSettings = nullptr;
    QAction *m_goBack = nullptr;
    QAction *m_goNext = nullptr;
    QAction *m_loadExported = nullptr;
    QAction *m_clear = nullptr;
    QAction *m_expandCollapse = nullptr;

    Utils::Perspective m_perspective{ClangTidyClazyPerspectiveId, tr("Clang-Tidy and Clazy")};

private:
    const QString m_name;
};

} // namespace Internal
} // namespace ClangTools
