// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <coreplugin/minisplitter.h>
#include <utils/faketooltip.h>

#include <designdocument.h>
#include <modelnode.h>

#include <QWidget>
#include <QMainWindow>
#include <QScopedPointer>

#include <advanceddockingsystem/dockmanager.h>
#include <annotationeditor/globalannotationeditor.h>

namespace Core {
    class SideBar;
    class SideBarItem;
    class EditorToolBar;
    class OutputPanePlaceHolder;
}

namespace QmlDesigner {

class ItemLibraryWidget;
class CrumbleBar;
class DocumentWarningWidget;

namespace Internal {

class DesignMode;
class DocumentWidget;

class DesignModeWidget : public QMainWindow
{
    Q_OBJECT

public:
    DesignModeWidget();
    ~DesignModeWidget() override;

    void contextHelp(const Core::IContext::HelpCallback &callback) const;

    void initialize();

    void readSettings();
    void saveSettings();

    DesignDocument *currentDesignDocument() const;
    ViewManager &viewManager();

    void setupNavigatorHistory(Core::IEditor *editor);

    void enableWidgets();
    void disableWidgets();

    CrumbleBar *crumbleBar() const;
    void showDockWidget(const QString &objectName, bool focus = false);

    void determineWorkspaceToRestoreAtStartup();

    static QWidget *createProjectExplorerWidget(QWidget *parent);

private:
    enum InitializeStatus { NotInitialized, Initializing, Initialized };

    void toolBarOnGoBackClicked();
    void toolBarOnGoForwardClicked();

    void setup();
    bool isInNodeDefinition(int nodeOffset, int nodeLength, int cursorPos) const;
    QmlDesigner::ModelNode nodeForPosition(int cursorPos) const;
    void addNavigatorHistoryEntry(const Utils::FilePath &fileName);
    QWidget *createCenterWidget();
    QWidget *createCrumbleBarFrame();

    void aboutToShowWorkspaces();

    QPointer<QWidget> m_bottomSideBar;
    Core::EditorToolBar *m_toolBar;
    CrumbleBar *m_crumbleBar;
    bool m_isDisabled = false;
    bool m_showSidebars = true;

    InitializeStatus m_initStatus = NotInitialized;

    QStringList m_navigatorHistory;
    int m_navigatorHistoryCounter = -1;
    bool m_keepNavigatorHistory = false;

    QList<QPointer<QWidget> >m_viewWidgets;

    ADS::DockManager *m_dockManager = nullptr;
    ADS::DockWidget *m_outputPaneDockWidget = nullptr;
    GlobalAnnotationEditor m_globalAnnotationEditor;
};

} // namespace Internal
} // namespace Designer
