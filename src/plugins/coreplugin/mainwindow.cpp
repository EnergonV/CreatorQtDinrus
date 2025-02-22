// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "mainwindow.h"

#include "coreicons.h"
#include "coreplugintr.h"
#include "documentmanager.h"
#include "editormanager/ieditorfactory.h"
#include "editormanager/systemeditor.h"
#include "externaltoolmanager.h"
#include "fancytabwidget.h"
#include "generalsettings.h"
#include "icore.h"
#include "idocumentfactory.h"
#include "jsexpander.h"
#include "loggingviewer.h"
#include "manhattanstyle.h"
#include "messagemanager.h"
#include "mimetypesettings.h"
#include "modemanager.h"
#include "navigationwidget.h"
#include "outputpanemanager.h"
#include "plugindialog.h"
#include "rightpane.h"
#include "statusbarmanager.h"
#include "systemsettings.h"
#include "vcsmanager.h"
#include "versiondialog.h"
#include "windowsupport.h"

#include <app/app_version.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actionmanager_p.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/dialogs/externaltoolconfig.h>
#include <coreplugin/dialogs/shortcutsettings.h>
#include <coreplugin/editormanager/documentmodel_p.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/editormanager_p.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/fileutils.h>
#include <coreplugin/find/basetextfind.h>
#include <coreplugin/findplaceholder.h>
#include <coreplugin/inavigationwidgetfactory.h>
#include <coreplugin/iwizardfactory.h>
#include <coreplugin/progressmanager/progressmanager_p.h>
#include <coreplugin/progressmanager/progressview.h>
#include <coreplugin/settingsdatabase.h>
#include <extensionsystem/pluginmanager.h>
#include <utils/algorithm.h>
#include <utils/fsengine/fileiconprovider.h>
#include <utils/fsengine/fsengine.h>
#include <utils/historycompleter.h>
#include <utils/hostosinfo.h>
#include <utils/mimeutils.h>
#include <utils/proxyaction.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>
#include <utils/stylehelper.h>
#include <utils/theme/theme.h>
#include <utils/touchbar/touchbar.h>
#include <utils/utilsicons.h>

#include <QAbstractProxyModel>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrinter>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QStyleFactory>
#include <QSyntaxHighlighter>
#include <QTextBrowser>
#include <QTextList>
#include <QToolButton>
#include <QUrl>
#include <QVersionNumber>
#include <QWindow>

#ifdef Q_OS_LINUX
#include <malloc.h>
#endif

using namespace ExtensionSystem;
using namespace Utils;

namespace Core {
namespace Internal {

static const char settingsGroup[] = "MainWindow";
static const char colorKey[] = "Color";
static const char askBeforeExitKey[] = "AskBeforeExit";
static const char windowGeometryKey[] = "WindowGeometry";
static const char windowStateKey[] = "WindowState";
static const char modeSelectorLayoutKey[] = "ModeSelectorLayout";
static const char openFromDeviceDialogKey[] = "OpenFromDeviceDialog";

static const bool askBeforeExitDefault = false;


enum { debugMainWindow = 0 };

MainWindow::MainWindow()
    : AppMainWindow()
    , m_coreImpl(new ICore(this))
    , m_lowPrioAdditionalContexts(Constants::C_GLOBAL)
    , m_settingsDatabase(
          new SettingsDatabase(QFileInfo(PluginManager::settings()->fileName()).path(),
                               QLatin1String(Constants::IDE_CASED_ID),
                               this))
    , m_progressManager(new ProgressManagerPrivate)
    , m_jsExpander(JsExpander::createGlobalJsExpander())
    , m_vcsManager(new VcsManager)
    , m_modeStack(new FancyTabWidget(this))
    , m_generalSettings(new GeneralSettings)
    , m_systemSettings(new SystemSettings)
    , m_shortcutSettings(new ShortcutSettings)
    , m_toolSettings(new ToolSettings)
    , m_mimeTypeSettings(new MimeTypeSettings)
    , m_systemEditor(new SystemEditor)
    , m_toggleLeftSideBarButton(new QToolButton)
    , m_toggleRightSideBarButton(new QToolButton)
{
    (void) new DocumentManager(this);

    HistoryCompleter::setSettings(PluginManager::settings());

    setWindowTitle(Constants::IDE_DISPLAY_NAME);
    if (HostOsInfo::isLinuxHost())
        QApplication::setWindowIcon(Icons::QTCREATORLOGO_BIG.icon());
    QString baseName = QApplication::style()->objectName();
    // Sometimes we get the standard windows 95 style as a fallback
    if (HostOsInfo::isAnyUnixHost() && !HostOsInfo::isMacHost()
            && baseName == QLatin1String("windows")) {
        baseName = QLatin1String("fusion");
    }

    // if the user has specified as base style in the theme settings,
    // prefer that
    const QStringList available = QStyleFactory::keys();
    const QStringList styles = Utils::creatorTheme()->preferredStyles();
    for (const QString &s : styles) {
        if (available.contains(s, Qt::CaseInsensitive)) {
            baseName = s;
            break;
        }
    }

    QApplication::setStyle(new ManhattanStyle(baseName));
    m_generalSettings->setShowShortcutsInContextMenu(
        GeneralSettings::showShortcutsInContextMenu());

    setDockNestingEnabled(true);

    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    m_modeManager = new ModeManager(this, m_modeStack);
    connect(m_modeStack, &FancyTabWidget::topAreaClicked, this, [](Qt::MouseButton, Qt::KeyboardModifiers modifiers) {
        if (modifiers & Qt::ShiftModifier) {
            QColor color = QColorDialog::getColor(StyleHelper::requestedBaseColor(), ICore::dialogParent());
            if (color.isValid())
                StyleHelper::setBaseColor(color);
        }
    });

    registerDefaultContainers();
    registerDefaultActions();

    m_leftNavigationWidget = new NavigationWidget(m_toggleLeftSideBarAction, Side::Left);
    m_rightNavigationWidget = new NavigationWidget(m_toggleRightSideBarAction, Side::Right);
    m_rightPaneWidget = new RightPaneWidget();

    m_messageManager = new MessageManager;
    m_editorManager = new EditorManager(this);
    m_externalToolManager = new ExternalToolManager();
    setCentralWidget(m_modeStack);

    m_progressManager->progressView()->setParent(this);

    connect(qApp, &QApplication::focusChanged, this, &MainWindow::updateFocusWidget);

    // Add small Toolbuttons for toggling the navigation widgets
    StatusBarManager::addStatusBarWidget(m_toggleLeftSideBarButton, StatusBarManager::First);
    int childsCount = statusBar()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly).count();
    statusBar()->insertPermanentWidget(childsCount - 1, m_toggleRightSideBarButton); // before QSizeGrip

//    setUnifiedTitleAndToolBarOnMac(true);
    //if (HostOsInfo::isAnyUnixHost())
        //signal(SIGINT, handleSigInt);

    statusBar()->setProperty("p_styled", true);

    auto dropSupport = new DropSupport(this, [](QDropEvent *event, DropSupport *) {
        return event->source() == nullptr; // only accept drops from the "outside" (e.g. file manager)
    });
    connect(dropSupport, &DropSupport::filesDropped,
            this, &MainWindow::openDroppedFiles);

#ifdef Q_OS_LINUX
    m_trimTimer.setSingleShot(true);
    m_trimTimer.setInterval(60000);
    // glibc may not actually free memory in free().
    connect(&m_trimTimer, &QTimer::timeout, this, [] { malloc_trim(0); });
#endif
}

NavigationWidget *MainWindow::navigationWidget(Side side) const
{
    return side == Side::Left ? m_leftNavigationWidget : m_rightNavigationWidget;
}

void MainWindow::setSidebarVisible(bool visible, Side side)
{
    if (NavigationWidgetPlaceHolder::current(side))
        navigationWidget(side)->setShown(visible);
}

bool MainWindow::askConfirmationBeforeExit() const
{
    return m_askConfirmationBeforeExit;
}

void MainWindow::setAskConfirmationBeforeExit(bool ask)
{
    m_askConfirmationBeforeExit = ask;
}

void MainWindow::setOverrideColor(const QColor &color)
{
    m_overrideColor = color;
}

QStringList MainWindow::additionalAboutInformation() const
{
    return m_aboutInformation;
}

void MainWindow::appendAboutInformation(const QString &line)
{
    m_aboutInformation.append(line);
}

void MainWindow::addPreCloseListener(const std::function<bool ()> &listener)
{
    m_preCloseListeners.append(listener);
}

MainWindow::~MainWindow()
{
    // explicitly delete window support, because that calls methods from ICore that call methods
    // from mainwindow, so mainwindow still needs to be alive
    delete m_windowSupport;
    m_windowSupport = nullptr;

    delete m_externalToolManager;
    m_externalToolManager = nullptr;
    delete m_messageManager;
    m_messageManager = nullptr;
    delete m_shortcutSettings;
    m_shortcutSettings = nullptr;
    delete m_generalSettings;
    m_generalSettings = nullptr;
    delete m_systemSettings;
    m_systemSettings = nullptr;
    delete m_toolSettings;
    m_toolSettings = nullptr;
    delete m_mimeTypeSettings;
    m_mimeTypeSettings = nullptr;
    delete m_systemEditor;
    m_systemEditor = nullptr;
    delete m_printer;
    m_printer = nullptr;
    delete m_vcsManager;
    m_vcsManager = nullptr;
    //we need to delete editormanager and statusbarmanager explicitly before the end of the destructor,
    //because they might trigger stuff that tries to access data from editorwindow, like removeContextWidget

    // All modes are now gone
    OutputPaneManager::destroy();

    delete m_leftNavigationWidget;
    delete m_rightNavigationWidget;
    m_leftNavigationWidget = nullptr;
    m_rightNavigationWidget = nullptr;

    delete m_editorManager;
    m_editorManager = nullptr;
    delete m_progressManager;
    m_progressManager = nullptr;

    delete m_coreImpl;
    m_coreImpl = nullptr;

    delete m_rightPaneWidget;
    m_rightPaneWidget = nullptr;

    delete m_modeManager;
    m_modeManager = nullptr;

    delete m_jsExpander;
    m_jsExpander = nullptr;
}

void MainWindow::init()
{
    m_progressManager->init(); // needs the status bar manager
    MessageManager::init();
    OutputPaneManager::create();
}

void MainWindow::extensionsInitialized()
{
    EditorManagerPrivate::extensionsInitialized();
    MimeTypeSettings::restoreSettings();
    m_windowSupport = new WindowSupport(this, Context("Core.MainWindow"));
    m_windowSupport->setCloseActionEnabled(false);
    OutputPaneManager::initialize();
    VcsManager::extensionsInitialized();
    m_leftNavigationWidget->setFactories(INavigationWidgetFactory::allNavigationFactories());
    m_rightNavigationWidget->setFactories(INavigationWidgetFactory::allNavigationFactories());

    ModeManager::extensionsInitialized();

    readSettings();
    updateContext();

    emit m_coreImpl->coreAboutToOpen();
    // Delay restoreWindowState, since it is overridden by LayoutRequest event
    QMetaObject::invokeMethod(this, &MainWindow::restoreWindowState, Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_coreImpl, &ICore::coreOpened, Qt::QueuedConnection);
}

static void setRestart(bool restart)
{
    qApp->setProperty("restart", restart);
}

void MainWindow::restart()
{
    setRestart(true);
    exit();
}

void MainWindow::restartTrimmer()
{
    if (!m_trimTimer.isActive())
        m_trimTimer.start();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto cancelClose = [event] {
        event->ignore();
        setRestart(false);
    };

    // work around QTBUG-43344
    static bool alreadyClosed = false;
    if (alreadyClosed) {
        event->accept();
        return;
    }

    if (m_askConfirmationBeforeExit &&
            (QMessageBox::question(this,
                                   tr("Exit %1?").arg(Constants::IDE_DISPLAY_NAME),
                                   tr("Exit %1?").arg(Constants::IDE_DISPLAY_NAME),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No)
             == QMessageBox::No)) {
        event->ignore();
        return;
    }

    ICore::saveSettings(ICore::MainWindowClosing);

    // Save opened files
    if (!DocumentManager::saveAllModifiedDocuments()) {
        cancelClose();
        return;
    }

    const QList<std::function<bool()>> listeners = m_preCloseListeners;
    for (const std::function<bool()> &listener : listeners) {
        if (!listener()) {
            cancelClose();
            return;
        }
    }

    emit m_coreImpl->coreAboutToClose();

    saveWindowSettings();

    m_leftNavigationWidget->closeSubWidgets();
    m_rightNavigationWidget->closeSubWidgets();

    event->accept();
    alreadyClosed = true;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    restartTrimmer();
    AppMainWindow::keyPressEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    restartTrimmer();
    AppMainWindow::mousePressEvent(event);
}

void MainWindow::openDroppedFiles(const QList<DropSupport::FileSpec> &files)
{
    raiseWindow();
    const FilePaths filePaths = Utils::transform(files, &DropSupport::FileSpec::filePath);
    openFiles(filePaths, ICore::SwitchMode);
}

IContext *MainWindow::currentContextObject() const
{
    return m_activeContext.isEmpty() ? nullptr : m_activeContext.first();
}

QStatusBar *MainWindow::statusBar() const
{
    return m_modeStack->statusBar();
}

InfoBar *MainWindow::infoBar() const
{
    return m_modeStack->infoBar();
}

void MainWindow::registerDefaultContainers()
{
    ActionContainer *menubar = ActionManager::createMenuBar(Constants::MENU_BAR);

    if (!HostOsInfo::isMacHost()) // System menu bar on Mac
        setMenuBar(menubar->menuBar());
    menubar->appendGroup(Constants::G_FILE);
    menubar->appendGroup(Constants::G_EDIT);
    menubar->appendGroup(Constants::G_VIEW);
    menubar->appendGroup(Constants::G_TOOLS);
    menubar->appendGroup(Constants::G_WINDOW);
    menubar->appendGroup(Constants::G_HELP);

    // File Menu
    ActionContainer *filemenu = ActionManager::createMenu(Constants::M_FILE);
    menubar->addMenu(filemenu, Constants::G_FILE);
    filemenu->menu()->setTitle(tr("&File"));
    filemenu->appendGroup(Constants::G_FILE_NEW);
    filemenu->appendGroup(Constants::G_FILE_OPEN);
    filemenu->appendGroup(Constants::G_FILE_PROJECT);
    filemenu->appendGroup(Constants::G_FILE_SAVE);
    filemenu->appendGroup(Constants::G_FILE_EXPORT);
    filemenu->appendGroup(Constants::G_FILE_CLOSE);
    filemenu->appendGroup(Constants::G_FILE_PRINT);
    filemenu->appendGroup(Constants::G_FILE_OTHER);
    connect(filemenu->menu(), &QMenu::aboutToShow, this, &MainWindow::aboutToShowRecentFiles);


    // Edit Menu
    ActionContainer *medit = ActionManager::createMenu(Constants::M_EDIT);
    menubar->addMenu(medit, Constants::G_EDIT);
    medit->menu()->setTitle(tr("&Edit"));
    medit->appendGroup(Constants::G_EDIT_UNDOREDO);
    medit->appendGroup(Constants::G_EDIT_COPYPASTE);
    medit->appendGroup(Constants::G_EDIT_SELECTALL);
    medit->appendGroup(Constants::G_EDIT_ADVANCED);
    medit->appendGroup(Constants::G_EDIT_FIND);
    medit->appendGroup(Constants::G_EDIT_OTHER);

    ActionContainer *mview = ActionManager::createMenu(Constants::M_VIEW);
    menubar->addMenu(mview, Constants::G_VIEW);
    mview->menu()->setTitle(tr("&View"));
    mview->appendGroup(Constants::G_VIEW_VIEWS);
    mview->appendGroup(Constants::G_VIEW_PANES);

    // Tools Menu
    ActionContainer *ac = ActionManager::createMenu(Constants::M_TOOLS);
    menubar->addMenu(ac, Constants::G_TOOLS);
    ac->menu()->setTitle(tr("&Tools"));

    // Window Menu
    ActionContainer *mwindow = ActionManager::createMenu(Constants::M_WINDOW);
    menubar->addMenu(mwindow, Constants::G_WINDOW);
    mwindow->menu()->setTitle(tr("&Window"));
    mwindow->appendGroup(Constants::G_WINDOW_SIZE);
    mwindow->appendGroup(Constants::G_WINDOW_SPLIT);
    mwindow->appendGroup(Constants::G_WINDOW_NAVIGATE);
    mwindow->appendGroup(Constants::G_WINDOW_LIST);
    mwindow->appendGroup(Constants::G_WINDOW_OTHER);

    // Help Menu
    ac = ActionManager::createMenu(Constants::M_HELP);
    menubar->addMenu(ac, Constants::G_HELP);
    ac->menu()->setTitle(tr("&Help"));
    ac->appendGroup(Constants::G_HELP_HELP);
    ac->appendGroup(Constants::G_HELP_SUPPORT);
    ac->appendGroup(Constants::G_HELP_ABOUT);
    ac->appendGroup(Constants::G_HELP_UPDATES);

    // macOS touch bar
    ac = ActionManager::createTouchBar(Constants::TOUCH_BAR,
                                       QIcon(),
                                       "Main TouchBar" /*never visible*/);
    ac->appendGroup(Constants::G_TOUCHBAR_HELP);
    ac->appendGroup(Constants::G_TOUCHBAR_EDITOR);
    ac->appendGroup(Constants::G_TOUCHBAR_NAVIGATION);
    ac->appendGroup(Constants::G_TOUCHBAR_OTHER);
    ac->touchBar()->setApplicationTouchBar();
}

void MainWindow::registerDefaultActions()
{
    ActionContainer *mfile = ActionManager::actionContainer(Constants::M_FILE);
    ActionContainer *medit = ActionManager::actionContainer(Constants::M_EDIT);
    ActionContainer *mview = ActionManager::actionContainer(Constants::M_VIEW);
    ActionContainer *mtools = ActionManager::actionContainer(Constants::M_TOOLS);
    ActionContainer *mwindow = ActionManager::actionContainer(Constants::M_WINDOW);
    ActionContainer *mhelp = ActionManager::actionContainer(Constants::M_HELP);

    // File menu separators
    mfile->addSeparator(Constants::G_FILE_SAVE);
    mfile->addSeparator(Constants::G_FILE_EXPORT);
    mfile->addSeparator(Constants::G_FILE_PRINT);
    mfile->addSeparator(Constants::G_FILE_CLOSE);
    mfile->addSeparator(Constants::G_FILE_OTHER);
    // Edit menu separators
    medit->addSeparator(Constants::G_EDIT_COPYPASTE);
    medit->addSeparator(Constants::G_EDIT_SELECTALL);
    medit->addSeparator(Constants::G_EDIT_FIND);
    medit->addSeparator(Constants::G_EDIT_ADVANCED);

    // Return to editor shortcut: Note this requires Qt to fix up
    // handling of shortcut overrides in menus, item views, combos....
    m_focusToEditor = new QAction(tr("Return to Editor"), this);
    Command *cmd = ActionManager::registerAction(m_focusToEditor, Constants::S_RETURNTOEDITOR);
    cmd->setDefaultKeySequence(QKeySequence(Qt::Key_Escape));
    connect(m_focusToEditor, &QAction::triggered, this, &MainWindow::setFocusToEditor);

    // New File Action
    QIcon icon = QIcon::fromTheme(QLatin1String("document-new"), Utils::Icons::NEWFILE.icon());

    m_newAction = new QAction(icon, tr("&New Project..."), this);
    cmd = ActionManager::registerAction(m_newAction, Constants::NEW);
    cmd->setDefaultKeySequence(QKeySequence("Ctrl+Shift+N"));
    mfile->addAction(cmd, Constants::G_FILE_NEW);
    connect(m_newAction, &QAction::triggered, this, []() {
        if (!ICore::isNewItemDialogRunning()) {
            ICore::showNewItemDialog(
                tr("New Project", "Title of dialog"),
                Utils::filtered(Core::IWizardFactory::allWizardFactories(),
                                Utils::equal(&Core::IWizardFactory::kind,
                                             Core::IWizardFactory::ProjectWizard)),
                FilePath());
        } else {
            ICore::raiseWindow(ICore::newItemDialog());
        }
    });

    auto action = new QAction(icon, tr("New File..."), this);
    cmd = ActionManager::registerAction(action, Constants::NEW_FILE);
    cmd->setDefaultKeySequence(QKeySequence::New);
    mfile->addAction(cmd, Constants::G_FILE_NEW);
    connect(action, &QAction::triggered, this, []() {
        if (!ICore::isNewItemDialogRunning()) {
            ICore::showNewItemDialog(tr("New File", "Title of dialog"),
                                     Utils::filtered(Core::IWizardFactory::allWizardFactories(),
                                                     Utils::equal(&Core::IWizardFactory::kind,
                                                                  Core::IWizardFactory::FileWizard)),
                                     FilePath());
        } else {
            ICore::raiseWindow(ICore::newItemDialog());
        }
    });

    // Open Action
    icon = QIcon::fromTheme(QLatin1String("document-open"), Utils::Icons::OPENFILE.icon());
    m_openAction = new QAction(icon, tr("&Open File or Project..."), this);
    cmd = ActionManager::registerAction(m_openAction, Constants::OPEN);
    cmd->setDefaultKeySequence(QKeySequence::Open);
    mfile->addAction(cmd, Constants::G_FILE_OPEN);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);

    // Open With Action
    m_openWithAction = new QAction(tr("Open File &With..."), this);
    cmd = ActionManager::registerAction(m_openWithAction, Constants::OPEN_WITH);
    mfile->addAction(cmd, Constants::G_FILE_OPEN);
    connect(m_openWithAction, &QAction::triggered, this, &MainWindow::openFileWith);

    if (FSEngine::isAvailable()) {
        // Open From Device Action
        m_openFromDeviceAction = new QAction(Tr::tr("Open From Device..."), this);
        cmd = ActionManager::registerAction(m_openFromDeviceAction, Constants::OPEN_FROM_DEVICE);
        mfile->addAction(cmd, Constants::G_FILE_OPEN);
        connect(m_openFromDeviceAction, &QAction::triggered, this, &MainWindow::openFileFromDevice);
    }

    // File->Recent Files Menu
    ActionContainer *ac = ActionManager::createMenu(Constants::M_FILE_RECENTFILES);
    mfile->addMenu(ac, Constants::G_FILE_OPEN);
    ac->menu()->setTitle(tr("Recent &Files"));
    ac->setOnAllDisabledBehavior(ActionContainer::Show);

    // Save Action
    icon = QIcon::fromTheme(QLatin1String("document-save"), Utils::Icons::SAVEFILE.icon());
    QAction *tmpaction = new QAction(icon, EditorManager::tr("&Save"), this);
    tmpaction->setEnabled(false);
    cmd = ActionManager::registerAction(tmpaction, Constants::SAVE);
    cmd->setDefaultKeySequence(QKeySequence::Save);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(tr("Save"));
    mfile->addAction(cmd, Constants::G_FILE_SAVE);

    // Save As Action
    icon = QIcon::fromTheme(QLatin1String("document-save-as"));
    tmpaction = new QAction(icon, EditorManager::tr("Save &As..."), this);
    tmpaction->setEnabled(false);
    cmd = ActionManager::registerAction(tmpaction, Constants::SAVEAS);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? tr("Ctrl+Shift+S") : QString()));
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(tr("Save As..."));
    mfile->addAction(cmd, Constants::G_FILE_SAVE);

    // SaveAll Action
    DocumentManager::registerSaveAllAction();

    // Print Action
    icon = QIcon::fromTheme(QLatin1String("document-print"));
    tmpaction = new QAction(icon, tr("&Print..."), this);
    tmpaction->setEnabled(false);
    cmd = ActionManager::registerAction(tmpaction, Constants::PRINT);
    cmd->setDefaultKeySequence(QKeySequence::Print);
    mfile->addAction(cmd, Constants::G_FILE_PRINT);

    // Exit Action
    icon = QIcon::fromTheme(QLatin1String("application-exit"));
    m_exitAction = new QAction(icon, tr("E&xit"), this);
    m_exitAction->setMenuRole(QAction::QuitRole);
    cmd = ActionManager::registerAction(m_exitAction, Constants::EXIT);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Q")));
    mfile->addAction(cmd, Constants::G_FILE_OTHER);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::exit);

    // Undo Action
    icon = QIcon::fromTheme(QLatin1String("edit-undo"), Utils::Icons::UNDO.icon());
    tmpaction = new QAction(icon, tr("&Undo"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::UNDO);
    cmd->setDefaultKeySequence(QKeySequence::Undo);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(tr("Undo"));
    medit->addAction(cmd, Constants::G_EDIT_UNDOREDO);
    tmpaction->setEnabled(false);

    // Redo Action
    icon = QIcon::fromTheme(QLatin1String("edit-redo"), Utils::Icons::REDO.icon());
    tmpaction = new QAction(icon, tr("&Redo"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::REDO);
    cmd->setDefaultKeySequence(QKeySequence::Redo);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDescription(tr("Redo"));
    medit->addAction(cmd, Constants::G_EDIT_UNDOREDO);
    tmpaction->setEnabled(false);

    // Cut Action
    icon = QIcon::fromTheme(QLatin1String("edit-cut"), Utils::Icons::CUT.icon());
    tmpaction = new QAction(icon, tr("Cu&t"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::CUT);
    cmd->setDefaultKeySequence(QKeySequence::Cut);
    medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
    tmpaction->setEnabled(false);

    // Copy Action
    icon = QIcon::fromTheme(QLatin1String("edit-copy"), Utils::Icons::COPY.icon());
    tmpaction = new QAction(icon, tr("&Copy"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::COPY);
    cmd->setDefaultKeySequence(QKeySequence::Copy);
    medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
    tmpaction->setEnabled(false);

    // Paste Action
    icon = QIcon::fromTheme(QLatin1String("edit-paste"), Utils::Icons::PASTE.icon());
    tmpaction = new QAction(icon, tr("&Paste"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::PASTE);
    cmd->setDefaultKeySequence(QKeySequence::Paste);
    medit->addAction(cmd, Constants::G_EDIT_COPYPASTE);
    tmpaction->setEnabled(false);

    // Select All
    icon = QIcon::fromTheme(QLatin1String("edit-select-all"));
    tmpaction = new QAction(icon, tr("Select &All"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::SELECTALL);
    cmd->setDefaultKeySequence(QKeySequence::SelectAll);
    medit->addAction(cmd, Constants::G_EDIT_SELECTALL);
    tmpaction->setEnabled(false);

    // Goto Action
    icon = QIcon::fromTheme(QLatin1String("go-jump"));
    tmpaction = new QAction(icon, tr("&Go to Line..."), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::GOTO);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+L")));
    medit->addAction(cmd, Constants::G_EDIT_OTHER);
    tmpaction->setEnabled(false);

    // Zoom In Action
    icon = QIcon::hasThemeIcon("zoom-in") ? QIcon::fromTheme("zoom-in")
                                          : Utils::Icons::ZOOMIN_TOOLBAR.icon();
    tmpaction = new QAction(icon, tr("Zoom In"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_IN);
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl++")));
    tmpaction->setEnabled(false);

    // Zoom Out Action
    icon = QIcon::hasThemeIcon("zoom-out") ? QIcon::fromTheme("zoom-out")
                                           : Utils::Icons::ZOOMOUT_TOOLBAR.icon();
    tmpaction = new QAction(icon, tr("Zoom Out"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_OUT);
    if (useMacShortcuts)
        cmd->setDefaultKeySequences({QKeySequence(tr("Ctrl+-")), QKeySequence(tr("Ctrl+Shift+-"))});
    else
        cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+-")));
    tmpaction->setEnabled(false);

    // Zoom Reset Action
    icon = QIcon::hasThemeIcon("zoom-original") ? QIcon::fromTheme("zoom-original")
                                                : Utils::Icons::EYE_OPEN_TOOLBAR.icon();
    tmpaction = new QAction(icon, tr("Original Size"), this);
    cmd = ActionManager::registerAction(tmpaction, Constants::ZOOM_RESET);
    cmd->setDefaultKeySequence(QKeySequence(Core::useMacShortcuts ? tr("Meta+0") : tr("Ctrl+0")));
    tmpaction->setEnabled(false);

    // Debug Qt Creator menu
    mtools->appendGroup(Constants::G_TOOLS_DEBUG);
    ActionContainer *mtoolsdebug = ActionManager::createMenu(Constants::M_TOOLS_DEBUG);
    mtoolsdebug->menu()->setTitle(tr("Debug %1").arg(Constants::IDE_DISPLAY_NAME));
    mtools->addMenu(mtoolsdebug, Constants::G_TOOLS_DEBUG);

    m_loggerAction = new QAction(tr("Show Logs..."), this);
    cmd = ActionManager::registerAction(m_loggerAction, Constants::LOGGER);
    mtoolsdebug->addAction(cmd);
    connect(m_loggerAction, &QAction::triggered, this, [] { LoggingViewer::showLoggingView(); });

    // Options Action
    medit->appendGroup(Constants::G_EDIT_PREFERENCES);
    medit->addSeparator(Constants::G_EDIT_PREFERENCES);

    m_optionsAction = new QAction(tr("Pr&eferences..."), this);
    m_optionsAction->setMenuRole(QAction::PreferencesRole);
    cmd = ActionManager::registerAction(m_optionsAction, Constants::OPTIONS);
    cmd->setDefaultKeySequence(QKeySequence::Preferences);
    medit->addAction(cmd, Constants::G_EDIT_PREFERENCES);
    connect(m_optionsAction, &QAction::triggered, this, [] { ICore::showOptionsDialog(Id()); });

    mwindow->addSeparator(Constants::G_WINDOW_LIST);

    if (useMacShortcuts) {
        // Minimize Action
        QAction *minimizeAction = new QAction(tr("Minimize"), this);
        minimizeAction->setEnabled(false); // actual implementation in WindowSupport
        cmd = ActionManager::registerAction(minimizeAction, Constants::MINIMIZE_WINDOW);
        cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+M")));
        mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

        // Zoom Action
        QAction *zoomAction = new QAction(tr("Zoom"), this);
        zoomAction->setEnabled(false); // actual implementation in WindowSupport
        cmd = ActionManager::registerAction(zoomAction, Constants::ZOOM_WINDOW);
        mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);
    }

    // Full Screen Action
    QAction *toggleFullScreenAction = new QAction(tr("Full Screen"), this);
    toggleFullScreenAction->setCheckable(!HostOsInfo::isMacHost());
    toggleFullScreenAction->setEnabled(false); // actual implementation in WindowSupport
    cmd = ActionManager::registerAction(toggleFullScreenAction, Constants::TOGGLE_FULLSCREEN);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? tr("Ctrl+Meta+F") : tr("Ctrl+Shift+F11")));
    if (HostOsInfo::isMacHost())
        cmd->setAttribute(Command::CA_UpdateText);
    mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

    if (useMacShortcuts) {
        mwindow->addSeparator(Constants::G_WINDOW_SIZE);

        QAction *closeAction = new QAction(tr("Close Window"), this);
        closeAction->setEnabled(false);
        cmd = ActionManager::registerAction(closeAction, Constants::CLOSE_WINDOW);
        cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Meta+W")));
        mwindow->addAction(cmd, Constants::G_WINDOW_SIZE);

        mwindow->addSeparator(Constants::G_WINDOW_SIZE);
    }

    // Show Left Sidebar Action
    m_toggleLeftSideBarAction = new QAction(Utils::Icons::TOGGLE_LEFT_SIDEBAR.icon(),
                                            Tr::tr(Constants::TR_SHOW_LEFT_SIDEBAR),
                                            this);
    m_toggleLeftSideBarAction->setCheckable(true);
    cmd = ActionManager::registerAction(m_toggleLeftSideBarAction, Constants::TOGGLE_LEFT_SIDEBAR);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? tr("Ctrl+0") : tr("Alt+0")));
    connect(m_toggleLeftSideBarAction, &QAction::triggered,
            this, [this](bool visible) { setSidebarVisible(visible, Side::Left); });
    ProxyAction *toggleLeftSideBarProxyAction =
            ProxyAction::proxyActionWithIcon(cmd->action(), Utils::Icons::TOGGLE_LEFT_SIDEBAR_TOOLBAR.icon());
    m_toggleLeftSideBarButton->setDefaultAction(toggleLeftSideBarProxyAction);
    mview->addAction(cmd, Constants::G_VIEW_VIEWS);
    m_toggleLeftSideBarAction->setEnabled(false);

    // Show Right Sidebar Action
    m_toggleRightSideBarAction = new QAction(Utils::Icons::TOGGLE_RIGHT_SIDEBAR.icon(),
                                             Tr::tr(Constants::TR_SHOW_RIGHT_SIDEBAR),
                                             this);
    m_toggleRightSideBarAction->setCheckable(true);
    cmd = ActionManager::registerAction(m_toggleRightSideBarAction, Constants::TOGGLE_RIGHT_SIDEBAR);
    cmd->setAttribute(Command::CA_UpdateText);
    cmd->setDefaultKeySequence(QKeySequence(useMacShortcuts ? tr("Ctrl+Shift+0") : tr("Alt+Shift+0")));
    connect(m_toggleRightSideBarAction, &QAction::triggered,
            this, [this](bool visible) { setSidebarVisible(visible, Side::Right); });
    ProxyAction *toggleRightSideBarProxyAction =
            ProxyAction::proxyActionWithIcon(cmd->action(), Utils::Icons::TOGGLE_RIGHT_SIDEBAR_TOOLBAR.icon());
    m_toggleRightSideBarButton->setDefaultAction(toggleRightSideBarProxyAction);
    mview->addAction(cmd, Constants::G_VIEW_VIEWS);
    m_toggleRightSideBarButton->setEnabled(false);

    registerModeSelectorStyleActions();

    // Window->Views
    ActionContainer *mviews = ActionManager::createMenu(Constants::M_VIEW_VIEWS);
    mview->addMenu(mviews, Constants::G_VIEW_VIEWS);
    mviews->menu()->setTitle(tr("&Views"));

    // "Help" separators
    mhelp->addSeparator(Constants::G_HELP_SUPPORT);
    if (!HostOsInfo::isMacHost())
        mhelp->addSeparator(Constants::G_HELP_ABOUT);

    // About IDE Action
    icon = QIcon::fromTheme(QLatin1String("help-about"));
    if (HostOsInfo::isMacHost())
        tmpaction = new QAction(icon, tr("About &%1").arg(Constants::IDE_DISPLAY_NAME), this); // it's convention not to add dots to the about menu
    else
        tmpaction = new QAction(icon, tr("About &%1...").arg(Constants::IDE_DISPLAY_NAME), this);
    tmpaction->setMenuRole(QAction::AboutRole);
    cmd = ActionManager::registerAction(tmpaction, Constants::ABOUT_QTCREATOR);
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindow::aboutQtCreator);

    //About Plugins Action
    tmpaction = new QAction(tr("About &Plugins..."), this);
    tmpaction->setMenuRole(QAction::ApplicationSpecificRole);
    cmd = ActionManager::registerAction(tmpaction, Constants::ABOUT_PLUGINS);
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindow::aboutPlugins);
    // About Qt Action
    //    tmpaction = new QAction(tr("About &Qt..."), this);
    //    cmd = ActionManager::registerAction(tmpaction, Constants:: ABOUT_QT);
    //    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    //    tmpaction->setEnabled(true);
    //    connect(tmpaction, &QAction::triggered, qApp, &QApplication::aboutQt);

    // Change Log Action
    tmpaction = new QAction(tr("Change Log..."), this);
    tmpaction->setMenuRole(QAction::ApplicationSpecificRole);
    cmd = ActionManager::registerAction(tmpaction, Constants::CHANGE_LOG);
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindow::changeLog);

    // Contact
    tmpaction = new QAction(tr("Contact..."), this);
    cmd = ActionManager::registerAction(tmpaction, "QtCreator.Contact");
    mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    tmpaction->setEnabled(true);
    connect(tmpaction, &QAction::triggered, this, &MainWindow::contact);

    // About sep
    if (!HostOsInfo::isMacHost()) { // doesn't have the "About" actions in the Help menu
        tmpaction = new QAction(this);
        tmpaction->setSeparator(true);
        cmd = ActionManager::registerAction(tmpaction, "QtCreator.Help.Sep.About");
        mhelp->addAction(cmd, Constants::G_HELP_ABOUT);
    }
}

void MainWindow::registerModeSelectorStyleActions()
{
    ActionContainer *mview = ActionManager::actionContainer(Constants::M_VIEW);

    // Cycle Mode Selector Styles
    m_cycleModeSelectorStyleAction = new QAction(tr("Cycle Mode Selector Styles"), this);
    ActionManager::registerAction(m_cycleModeSelectorStyleAction, Constants::CYCLE_MODE_SELECTOR_STYLE);
    connect(m_cycleModeSelectorStyleAction, &QAction::triggered, this, [this] {
        ModeManager::cycleModeStyle();
        updateModeSelectorStyleMenu();
    });

    // Mode Selector Styles
    ActionContainer *mmodeLayouts = ActionManager::createMenu(Constants::M_VIEW_MODESTYLES);
    mview->addMenu(mmodeLayouts, Constants::G_VIEW_VIEWS);
    QMenu *styleMenu = mmodeLayouts->menu();
    styleMenu->setTitle(tr("Mode Selector Style"));
    auto *stylesGroup = new QActionGroup(styleMenu);
    stylesGroup->setExclusive(true);

    m_setModeSelectorStyleIconsAndTextAction = stylesGroup->addAction(tr("Icons and Text"));
    connect(m_setModeSelectorStyleIconsAndTextAction, &QAction::triggered,
                                 [] { ModeManager::setModeStyle(ModeManager::Style::IconsAndText); });
    m_setModeSelectorStyleIconsAndTextAction->setCheckable(true);
    m_setModeSelectorStyleIconsOnlyAction = stylesGroup->addAction(tr("Icons Only"));
    connect(m_setModeSelectorStyleIconsOnlyAction, &QAction::triggered,
                                 [] { ModeManager::setModeStyle(ModeManager::Style::IconsOnly); });
    m_setModeSelectorStyleIconsOnlyAction->setCheckable(true);
    m_setModeSelectorStyleHiddenAction = stylesGroup->addAction(tr("Hidden"));
    connect(m_setModeSelectorStyleHiddenAction, &QAction::triggered,
                                 [] { ModeManager::setModeStyle(ModeManager::Style::Hidden); });
    m_setModeSelectorStyleHiddenAction->setCheckable(true);

    styleMenu->addActions(stylesGroup->actions());
}

void MainWindow::openFile()
{
    openFiles(EditorManager::getOpenFilePaths(), ICore::SwitchMode);
}

static IDocumentFactory *findDocumentFactory(const QList<IDocumentFactory*> &fileFactories,
                                             const FilePath &filePath)
{
    const QString typeName = Utils::mimeTypeForFile(filePath).name();
    return Utils::findOrDefault(fileFactories, [typeName](IDocumentFactory *f) {
        return f->mimeTypes().contains(typeName);
    });
}

/*!
 * \internal
 * Either opens \a filePaths with editors or loads a project.
 *
 *  \a flags can be used to stop on first failure, indicate that a file name
 *  might include line numbers and/or switch mode to edit mode.
 *
 *  \a workingDirectory is used when files are opened by a remote client, since
 *  the file names are relative to the client working directory.
 *
 *  Returns the first opened document. Required to support the \c -block flag
 *  for client mode.
 *
 *  \sa IPlugin::remoteArguments()
 */
IDocument *MainWindow::openFiles(const FilePaths &filePaths,
                                 ICore::OpenFilesFlags flags,
                                 const QString &workingDirectory)
{
    const QList<IDocumentFactory*> documentFactories = IDocumentFactory::allDocumentFactories();
    IDocument *res = nullptr;

    const QString workingDirBase = workingDirectory.isEmpty() ? QDir::currentPath() : workingDirectory;
    for (const FilePath &filePath : filePaths) {
        const FilePath workingDir = filePath.withNewPath(workingDirBase);
        FilePath absoluteFilePath;
        if (filePath.isAbsolutePath()) {
            absoluteFilePath = filePath;
        } else {
            QTC_CHECK(!filePath.needsDevice());
            absoluteFilePath = FilePath::fromString(workingDirBase).resolvePath(filePath.path());
        }
        if (IDocumentFactory *documentFactory = findDocumentFactory(documentFactories, filePath)) {
            IDocument *document = documentFactory->open(absoluteFilePath);
            if (!document) {
                if (flags & ICore::StopOnLoadFail)
                    return res;
            } else {
                if (!res)
                    res = document;
                if (flags & ICore::SwitchMode)
                    ModeManager::activateMode(Id(Constants::MODE_EDIT));
            }
        } else if (flags & (ICore::SwitchSplitIfAlreadyVisible | ICore::CanContainLineAndColumnNumbers)
                   || !res) {
            QFlags<EditorManager::OpenEditorFlag> emFlags;
            if (flags & ICore::SwitchSplitIfAlreadyVisible)
                emFlags |= EditorManager::SwitchSplitIfAlreadyVisible;
            IEditor *editor = nullptr;
            if (flags & ICore::CanContainLineAndColumnNumbers) {
                const Link &link = Link::fromFilePath(absoluteFilePath, true);
                editor = EditorManager::openEditorAt(link, {}, emFlags);
            } else {
                editor = EditorManager::openEditor(absoluteFilePath, {}, emFlags);
            }
            if (!editor) {
                if (flags & ICore::StopOnLoadFail)
                    return res;
            } else if (!res) {
                res = editor->document();
            }
        } else {
            auto factory = IEditorFactory::preferredEditorFactories(absoluteFilePath).value(0);
            DocumentModelPrivate::addSuspendedDocument(absoluteFilePath, {},
                                                       factory ? factory->id() : Id());
        }
    }
    return res;
}

void MainWindow::setFocusToEditor()
{
    EditorManagerPrivate::doEscapeKeyFocusMoveMagic();
}

static void acceptModalDialogs()
{
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    QList<QDialog *> dialogsToClose;
    for (QWidget *topLevel : topLevels) {
        if (auto dialog = qobject_cast<QDialog *>(topLevel)) {
            if (dialog->isModal())
                dialogsToClose.append(dialog);
        }
    }
    for (QDialog *dialog : dialogsToClose)
        dialog->accept();
}

void MainWindow::exit()
{
    // this function is most likely called from a user action
    // that is from an event handler of an object
    // since on close we are going to delete everything
    // so to prevent the deleting of that object we
    // just append it
    QMetaObject::invokeMethod(
        this,
        [this] {
            // Modal dialogs block the close event. So close them, in case this was triggered from
            // a RestartDialog in the settings dialog.
            acceptModalDialogs();
            close();
        },
        Qt::QueuedConnection);
}

void MainWindow::openFileWith()
{
    const FilePaths filePaths = EditorManager::getOpenFilePaths();
    for (const FilePath &filePath : filePaths) {
        bool isExternal;
        const Id editorId = EditorManagerPrivate::getOpenWithEditorId(filePath, &isExternal);
        if (!editorId.isValid())
            continue;
        if (isExternal)
            EditorManager::openExternalEditor(filePath, editorId);
        else
            EditorManagerPrivate::openEditorWith(filePath, editorId);
    }
}

void MainWindow::openFileFromDevice()
{
    QSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));
    QVariant dialogSettings = settings->value(QLatin1String(openFromDeviceDialogKey));

    QFileDialog dialog;
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    if (!dialogSettings.isNull()) {
        dialog.restoreState(dialogSettings.toByteArray());
    }
    QList<QUrl> sideBarUrls = Utils::transform(Utils::filtered(FSEngine::registeredDeviceRoots(),
                                                               [](const auto &filePath) {
                                                                   return filePath.exists();
                                                               }),
                                               [](const auto &filePath) {
                                                   return QUrl::fromLocalFile(filePath.toFSPathString());
                                               });
    dialog.setSidebarUrls(sideBarUrls);
    dialog.setFileMode(QFileDialog::AnyFile);

    dialog.setIconProvider(FileIconProvider::iconProvider());

    if (dialog.exec()) {
        FilePaths filePaths = Utils::transform(dialog.selectedFiles(), [](const auto &path) {
            return FilePath::fromString(path);
        });

        openFiles(filePaths, ICore::SwitchMode);
    }

    settings->setValue(QLatin1String(openFromDeviceDialogKey), dialog.saveState());
    settings->endGroup();
}

IContext *MainWindow::contextObject(QWidget *widget) const
{
    const auto it = m_contextWidgets.find(widget);
    return it == m_contextWidgets.end() ? nullptr : it->second;
}

void MainWindow::addContextObject(IContext *context)
{
    if (!context)
        return;
    QWidget *widget = context->widget();
    if (m_contextWidgets.find(widget) != m_contextWidgets.end())
        return;

    m_contextWidgets.insert(std::make_pair(widget, context));
    connect(context, &QObject::destroyed, this, [this, context] { removeContextObject(context); });
}

void MainWindow::removeContextObject(IContext *context)
{
    if (!context)
        return;

    disconnect(context, &QObject::destroyed, this, nullptr);

    const auto it = std::find_if(m_contextWidgets.cbegin(),
                                 m_contextWidgets.cend(),
                                 [context](const std::pair<QWidget *, IContext *> &v) {
                                     return v.second == context;
                                 });
    if (it == m_contextWidgets.cend())
        return;

    m_contextWidgets.erase(it);
    if (m_activeContext.removeAll(context) > 0)
        updateContextObject(m_activeContext);
}

void MainWindow::updateFocusWidget(QWidget *old, QWidget *now)
{
    Q_UNUSED(old)

    // Prevent changing the context object just because the menu or a menu item is activated
    if (qobject_cast<QMenuBar*>(now) || qobject_cast<QMenu*>(now))
        return;

    QList<IContext *> newContext;
    if (QWidget *p = QApplication::focusWidget()) {
        IContext *context = nullptr;
        while (p) {
            context = contextObject(p);
            if (context)
                newContext.append(context);
            p = p->parentWidget();
        }
    }

    // ignore toplevels that define no context, like popups without parent
    if (!newContext.isEmpty() || QApplication::focusWidget() == focusWidget())
        updateContextObject(newContext);
}

void MainWindow::updateContextObject(const QList<IContext *> &context)
{
    emit m_coreImpl->contextAboutToChange(context);
    m_activeContext = context;
    updateContext();
    if (debugMainWindow) {
        qDebug() << "new context objects =" << context;
        for (const IContext *c : context)
            qDebug() << (c ? c->widget() : nullptr) << (c ? c->widget()->metaObject()->className() : nullptr);
    }
}

void MainWindow::aboutToShutdown()
{
    disconnect(qApp, &QApplication::focusChanged, this, &MainWindow::updateFocusWidget);
    for (auto contextPair : m_contextWidgets)
        disconnect(contextPair.second, &QObject::destroyed, this, nullptr);
    m_activeContext.clear();
    hide();
}

void MainWindow::readSettings()
{
    QSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));

    if (m_overrideColor.isValid()) {
        StyleHelper::setBaseColor(m_overrideColor);
        // Get adapted base color.
        m_overrideColor = StyleHelper::baseColor();
    } else {
        StyleHelper::setBaseColor(settings->value(QLatin1String(colorKey),
                                  QColor(StyleHelper::DEFAULT_BASE_COLOR)).value<QColor>());
    }

    m_askConfirmationBeforeExit = settings->value(askBeforeExitKey, askBeforeExitDefault).toBool();

    {
        ModeManager::Style modeStyle =
                ModeManager::Style(settings->value(modeSelectorLayoutKey, int(ModeManager::Style::IconsAndText)).toInt());

        // Migrate legacy setting from Qt Creator 4.6 and earlier
        static const char modeSelectorVisibleKey[] = "ModeSelectorVisible";
        if (!settings->contains(modeSelectorLayoutKey) && settings->contains(modeSelectorVisibleKey)) {
            bool visible = settings->value(modeSelectorVisibleKey, true).toBool();
            modeStyle = visible ? ModeManager::Style::IconsAndText : ModeManager::Style::Hidden;
        }

        ModeManager::setModeStyle(modeStyle);
        updateModeSelectorStyleMenu();
    }

    settings->endGroup();

    EditorManagerPrivate::readSettings();
    m_leftNavigationWidget->restoreSettings(settings);
    m_rightNavigationWidget->restoreSettings(settings);
    m_rightPaneWidget->readSettings(settings);
}

void MainWindow::saveSettings()
{
    QtcSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));

    if (!(m_overrideColor.isValid() && StyleHelper::baseColor() == m_overrideColor))
        settings->setValueWithDefault(colorKey,
                                      StyleHelper::requestedBaseColor(),
                                      QColor(StyleHelper::DEFAULT_BASE_COLOR));

    settings->setValueWithDefault(askBeforeExitKey,
                                  m_askConfirmationBeforeExit,
                                  askBeforeExitDefault);

    settings->endGroup();

    DocumentManager::saveSettings();
    ActionManager::saveSettings();
    EditorManagerPrivate::saveSettings();
    m_leftNavigationWidget->saveSettings(settings);
    m_rightNavigationWidget->saveSettings(settings);
}

void MainWindow::saveWindowSettings()
{
    QSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));

    // On OS X applications usually do not restore their full screen state.
    // To be able to restore the correct non-full screen geometry, we have to put
    // the window out of full screen before saving the geometry.
    // Works around QTBUG-45241
    if (Utils::HostOsInfo::isMacHost() && isFullScreen())
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    settings->setValue(QLatin1String(windowGeometryKey), saveGeometry());
    settings->setValue(QLatin1String(windowStateKey), saveState());
    settings->setValue(modeSelectorLayoutKey, int(ModeManager::modeStyle()));

    settings->endGroup();
}

void MainWindow::updateModeSelectorStyleMenu()
{
    switch (ModeManager::modeStyle()) {
    case ModeManager::Style::IconsAndText:
        m_setModeSelectorStyleIconsAndTextAction->setChecked(true);
        break;
    case ModeManager::Style::IconsOnly:
        m_setModeSelectorStyleIconsOnlyAction->setChecked(true);
        break;
    case ModeManager::Style::Hidden:
        m_setModeSelectorStyleHiddenAction->setChecked(true);
        break;
    }
}

void MainWindow::updateAdditionalContexts(const Context &remove, const Context &add,
                                          ICore::ContextPriority priority)
{
    for (const Id id : remove) {
        if (!id.isValid())
            continue;
        int index = m_lowPrioAdditionalContexts.indexOf(id);
        if (index != -1)
            m_lowPrioAdditionalContexts.removeAt(index);
        index = m_highPrioAdditionalContexts.indexOf(id);
        if (index != -1)
            m_highPrioAdditionalContexts.removeAt(index);
    }

    for (const Id id : add) {
        if (!id.isValid())
            continue;
        Context &cref = (priority == ICore::ContextPriority::High ? m_highPrioAdditionalContexts
                                                                  : m_lowPrioAdditionalContexts);
        if (!cref.contains(id))
            cref.prepend(id);
    }

    updateContext();
}

void MainWindow::updateContext()
{
    Context contexts = m_highPrioAdditionalContexts;

    for (IContext *context : qAsConst(m_activeContext))
        contexts.add(context->context());

    contexts.add(m_lowPrioAdditionalContexts);

    Context uniquecontexts;
    for (const Id &id : qAsConst(contexts)) {
        if (!uniquecontexts.contains(id))
            uniquecontexts.add(id);
    }

    ActionManager::setContext(uniquecontexts);
    emit m_coreImpl->contextChanged(uniquecontexts);
}

void MainWindow::aboutToShowRecentFiles()
{
    ActionContainer *aci = ActionManager::actionContainer(Constants::M_FILE_RECENTFILES);
    QMenu *menu = aci->menu();
    menu->clear();

    const QList<DocumentManager::RecentFile> recentFiles = DocumentManager::recentFiles();
    for (int i = 0; i < recentFiles.count(); ++i) {
        const DocumentManager::RecentFile file = recentFiles[i];

        const QString filePath = Utils::quoteAmpersands(file.first.shortNativePath());
        const QString actionText = ActionManager::withNumberAccelerator(filePath, i + 1);
        QAction *action = menu->addAction(actionText);
        connect(action, &QAction::triggered, this, [file] {
            EditorManager::openEditor(file.first, file.second);
        });
    }

    bool hasRecentFiles = !recentFiles.isEmpty();
    menu->setEnabled(hasRecentFiles);

    // add the Clear Menu item
    if (hasRecentFiles) {
        menu->addSeparator();
        QAction *action = menu->addAction(Tr::tr(Constants::TR_CLEAR_MENU));
        connect(action, &QAction::triggered,
                DocumentManager::instance(), &DocumentManager::clearRecentFiles);
    }
}

void MainWindow::aboutQtCreator()
{
    if (!m_versionDialog) {
        m_versionDialog = new VersionDialog(this);
        connect(m_versionDialog, &QDialog::finished,
                this, &MainWindow::destroyVersionDialog);
        ICore::registerWindow(m_versionDialog, Context("Core.VersionDialog"));
        m_versionDialog->show();
    } else {
        ICore::raiseWindow(m_versionDialog);
    }
}

void MainWindow::destroyVersionDialog()
{
    if (m_versionDialog) {
        m_versionDialog->deleteLater();
        m_versionDialog = nullptr;
    }
}

void MainWindow::aboutPlugins()
{
    PluginDialog dialog(this);
    dialog.exec();
}

class LogDialog : public QDialog
{
public:
    LogDialog(QWidget *parent)
        : QDialog(parent)
    {}
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            auto ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
                ke->accept();
                return true;
            }
        }
        return QDialog::event(event);
    }
};

class MarkdownHighlighter : public QSyntaxHighlighter
{
    QBrush h2Brush;
public:
    MarkdownHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent)
        , h2Brush(Qt::NoBrush)
    {
        parent->setIndentWidth(30); // default value is 40
    }

    void highlightBlock(const QString &text)
    {
        if (text.isEmpty())
            return;

        QTextBlockFormat fmt = currentBlock().blockFormat();
        QTextCursor cur(currentBlock());
        if (fmt.hasProperty(QTextFormat::HeadingLevel)) {
            fmt.setTopMargin(10);
            fmt.setBottomMargin(10);

            // Draw an underline for Heading 2, by creating a texture brush
            // with the last pixel visible
            if (fmt.property(QTextFormat::HeadingLevel) == 2) {
                QTextCharFormat charFmt = currentBlock().charFormat();
                charFmt.setBaselineOffset(15);
                setFormat(0, text.length(), charFmt);

                if (h2Brush.style() == Qt::NoBrush) {
                    const int height = QFontMetrics(charFmt.font()).height();
                    QImage image(1, height, QImage::Format_ARGB32);

                    image.fill(QColor(0, 0, 0, 0).rgba());
                    image.setPixel(0,
                                   height - 1,
                                   Utils::creatorTheme()->color(Theme::TextColorDisabled).rgba());

                    h2Brush = QBrush(image);
                }
                fmt.setBackground(h2Brush);
            }
            cur.setBlockFormat(fmt);
        } else if (fmt.hasProperty(QTextFormat::BlockCodeLanguage) && fmt.indent() == 0) {
            // set identation for code blocks
            fmt.setIndent(1);
            cur.setBlockFormat(fmt);
        }

        // Show the bulet points as filled circles
        QTextList *list = cur.currentList();
        if (list) {
            QTextListFormat listFmt = list->format();
            if (listFmt.indent() == 1 && listFmt.style() == QTextListFormat::ListCircle) {
                listFmt.setStyle(QTextListFormat::ListDisc);
                list->setFormat(listFmt);
            }
        }
    }
};

void MainWindow::changeLog()
{
    static QPointer<LogDialog> dialog;
    if (dialog) {
        ICore::raiseWindow(dialog);
        return;
    }
    const FilePaths files =
            ICore::resourcePath("changelog").dirEntries({{"changes-*"}, QDir::Files});
    static const QRegularExpression versionRegex("\\d+[.]\\d+[.]\\d+");
    using VersionFilePair = std::pair<QVersionNumber, FilePath>;
    QList<VersionFilePair> versionedFiles = Utils::transform(files, [](const FilePath &fp) {
        const QRegularExpressionMatch match = versionRegex.match(fp.fileName());
        const QVersionNumber version = match.hasMatch()
                                           ? QVersionNumber::fromString(match.captured())
                                           : QVersionNumber();
        return std::make_pair(version, fp);
    });
    Utils::sort(versionedFiles, [](const VersionFilePair &a, const VersionFilePair &b) {
        return a.first > b.first;
    });

    auto versionCombo = new QComboBox;
    for (const VersionFilePair &f : versionedFiles)
        versionCombo->addItem(f.first.toString());
    dialog = new LogDialog(ICore::dialogParent());
    auto versionLayout = new QHBoxLayout;
    versionLayout->addWidget(new QLabel(tr("Version:")));
    versionLayout->addWidget(versionCombo);
    versionLayout->addStretch(1);
    auto showInExplorer = new QPushButton(FileUtils::msgGraphicalShellAction());
    versionLayout->addWidget(showInExplorer);
    auto textEdit = new QTextBrowser;
    textEdit->setOpenExternalLinks(true);

    auto aggregate = new Aggregation::Aggregate;
    aggregate->add(textEdit);
    aggregate->add(new Core::BaseTextFind(textEdit));

    auto highlighter = new MarkdownHighlighter(textEdit->document());
    (void)highlighter;

    auto textEditWidget = new QFrame;
    textEditWidget->setFrameStyle(QFrame::NoFrame);
    auto findToolBar = new FindToolBarPlaceHolder(dialog);
    findToolBar->setLightColored(true);
    auto textEditLayout = new QVBoxLayout;
    textEditLayout->setContentsMargins(0, 0, 0, 0);
    textEditLayout->setSpacing(0);
    textEditLayout->addWidget(textEdit);
    textEditLayout->addWidget(findToolBar);
    textEditWidget->setLayout(textEditLayout);
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    auto dialogLayout = new QVBoxLayout;
    dialogLayout->addLayout(versionLayout);
    dialogLayout->addWidget(textEditWidget);
    dialogLayout->addWidget(buttonBox);
    dialog->setLayout(dialogLayout);
    dialog->resize(700, 600);
    dialog->setWindowTitle(tr("Change Log"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    ICore::registerWindow(dialog, Context("CorePlugin.VersionDialog"));

    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    QPushButton *closeButton = buttonBox->button(QDialogButtonBox::Close);
    if (QTC_GUARD(closeButton))
        closeButton->setDefault(true); // grab from "Open in Explorer" button

    const auto showLog = [textEdit, versionedFiles](int index) {
        if (index < 0 || index >= versionedFiles.size())
            return;
        const FilePath file = versionedFiles.at(index).second;
        QString contents = QString::fromUtf8(file.fileContents());
        contents.replace(QRegularExpression("(QT(CREATOR)?BUG-[0-9]+)"),
                         "[\\1](https://bugreports.qt.io/browse/\\1)");
        textEdit->setMarkdown(contents);
    };
    connect(versionCombo, &QComboBox::currentIndexChanged, textEdit, showLog);
    showLog(versionCombo->currentIndex());

    connect(showInExplorer, &QPushButton::clicked, [versionCombo, versionedFiles] {
        const int index = versionCombo->currentIndex();
        if (index >= 0 && index < versionedFiles.size())
            FileUtils::showInGraphicalShell(ICore::dialogParent(), versionedFiles.at(index).second);
        else
            FileUtils::showInGraphicalShell(ICore::dialogParent(), ICore::resourcePath("changelog"));
    });

    dialog->show();
}

void MainWindow::contact()
{
    QMessageBox dlg(QMessageBox::Information, tr("Contact"),
           tr("<p>Qt Creator developers can be reached at the Qt Creator mailing list:</p>"
              "%1"
              "<p>or the #qt-creator channel on Libera.Chat IRC:</p>"
              "%2"
              "<p>Our bug tracker is located at %3.</p>"
              "<p>Please use %4 for bigger chunks of text.</p>")
                    .arg("<p>&nbsp;&nbsp;&nbsp;&nbsp;"
                            "<a href=\"https://lists.qt-project.org/listinfo/qt-creator\">"
                            "mailto:qt-creator@qt-project.org"
                         "</a></p>")
                    .arg("<p>&nbsp;&nbsp;&nbsp;&nbsp;"
                            "<a href=\"https://web.libera.chat/#qt-creator\">"
                            "https://web.libera.chat/#qt-creator"
                         "</a></p>")
                    .arg("<a href=\"https://bugreports.qt.io/projects/QTCREATORBUG\">"
                            "https://bugreports.qt.io"
                         "</a>")
                    .arg("<a href=\"https://pastebin.com\">"
                            "https://pastebin.com"
                         "</a>"),
           QMessageBox::Ok, this);
    dlg.exec();
}

QPrinter *MainWindow::printer() const
{
    if (!m_printer)
        m_printer = new QPrinter(QPrinter::HighResolution);
    return m_printer;
}

void MainWindow::restoreWindowState()
{
    QSettings *settings = PluginManager::settings();
    settings->beginGroup(QLatin1String(settingsGroup));
    if (!restoreGeometry(settings->value(QLatin1String(windowGeometryKey)).toByteArray()))
        resize(1260, 700); // size without window decoration
    restoreState(settings->value(QLatin1String(windowStateKey)).toByteArray());
    settings->endGroup();
    show();
    StatusBarManager::restoreSettings();
}

} // namespace Internal
} // namespace Core
