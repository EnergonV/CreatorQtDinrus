// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qrceditor.h"

#include "../resourceeditortr.h"
#include "undocommands_p.h"

#include <aggregation/aggregate.h>
#include <coreplugin/find/itemviewfind.h>
#include <utils/layoutbuilder.h>

#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedPointer>

namespace ResourceEditor::Internal {

QrcEditor::QrcEditor(RelativeResourceModel *model, QWidget *parent)
  : Core::MiniSplitter(Qt::Vertical, parent),
    m_treeview(new ResourceView(model, &m_history))
{
    addWidget(m_treeview);
    auto widget = new QWidget;
    addWidget(widget);
    m_treeview->setFrameStyle(QFrame::NoFrame);
    m_treeview->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    auto addPrefixButton = new QPushButton(Tr::tr("Add Prefix"));
    m_addFilesButton = new QPushButton(Tr::tr("Add Files"));
    m_removeButton = new QPushButton(Tr::tr("Remove"));
    m_removeNonExistingButton = new QPushButton(Tr::tr("Remove Missing Files"));

    m_aliasLabel = new QLabel(Tr::tr("Alias:"));
    m_aliasText = new QLineEdit;
    m_prefixLabel = new QLabel(Tr::tr("Prefix:"));
    m_prefixText = new QLineEdit;
    m_languageLabel = new QLabel(Tr::tr("Language:"));
    m_languageText = new QLineEdit;

    using namespace Utils::Layouting;
    Column {
        Row {
            addPrefixButton,
            m_addFilesButton,
            m_removeButton,
            m_removeNonExistingButton,
            st,
        },
        Group {
            title(Tr::tr("Properties")),
            Form {
                m_aliasLabel, m_aliasText, br,
                m_prefixLabel, m_prefixText, br,
                m_languageLabel, m_languageText, br,
            }
        },
        st,
    }.attachTo(widget);

    connect(addPrefixButton, &QAbstractButton::clicked, this, &QrcEditor::onAddPrefix);
    connect(m_addFilesButton, &QAbstractButton::clicked, this, &QrcEditor::onAddFiles);
    connect(m_removeButton, &QAbstractButton::clicked, this, &QrcEditor::onRemove);
    connect(m_removeNonExistingButton, &QPushButton::clicked,
            this, &QrcEditor::onRemoveNonExisting);

    connect(m_treeview, &ResourceView::removeItem, this, &QrcEditor::onRemove);
    connect(m_treeview->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &QrcEditor::updateCurrent);
    connect(m_treeview, &ResourceView::itemActivated, this, &QrcEditor::itemActivated);
    connect(m_treeview, &ResourceView::contextMenuShown, this, &QrcEditor::showContextMenu);
    m_treeview->setFocus();

    connect(m_aliasText, &QLineEdit::textEdited, this, &QrcEditor::onAliasChanged);
    connect(m_prefixText, &QLineEdit::textEdited, this, &QrcEditor::onPrefixChanged);
    connect(m_languageText, &QLineEdit::textEdited, this, &QrcEditor::onLanguageChanged);

    // Prevent undo command merging after a switch of focus:
    // (0) The initial text is "Green".
    // (1) The user appends " is a color." --> text is "Green is a color."
    // (2) The user clicks into some other line edit --> loss of focus
    // (3) The user gives focuse again and substitutes "Green" with "Red"
    //     --> text now is "Red is a color."
    // (4) The user hits undo --> text now is "Green is a color."
    //     Without calling advanceMergeId() it would have been "Green", instead.
    connect(m_aliasText, &QLineEdit::editingFinished,
            m_treeview, &ResourceView::advanceMergeId);
    connect(m_prefixText, &QLineEdit::editingFinished,
            m_treeview, &ResourceView::advanceMergeId);
    connect(m_languageText, &QLineEdit::editingFinished,
            m_treeview, &ResourceView::advanceMergeId);

    connect(&m_history, &QUndoStack::canRedoChanged, this, &QrcEditor::updateHistoryControls);
    connect(&m_history, &QUndoStack::canUndoChanged, this, &QrcEditor::updateHistoryControls);

    auto agg = new Aggregation::Aggregate;
    agg->add(m_treeview);
    agg->add(new Core::ItemViewFind(m_treeview));

    updateHistoryControls();
    updateCurrent();
}

QrcEditor::~QrcEditor() = default;

void QrcEditor::loaded(bool success)
{
    if (!success)
        return;
    // Set "focus"
    m_treeview->setCurrentIndex(m_treeview->model()->index(0,0));
    // Expand prefix nodes
    m_treeview->expandAll();
}

void QrcEditor::refresh()
{
    m_treeview->refresh();
}

// Propagates a change of selection in the tree
// to the alias/prefix/language edit controls
void QrcEditor::updateCurrent()
{
    const bool isValid = m_treeview->currentIndex().isValid();
    const bool isPrefix = m_treeview->isPrefix(m_treeview->currentIndex()) && isValid;
    const bool isFile = !isPrefix && isValid;

    m_aliasLabel->setEnabled(isFile);
    m_aliasText->setEnabled(isFile);
    m_currentAlias = m_treeview->currentAlias();
    m_aliasText->setText(m_currentAlias);

    m_prefixLabel->setEnabled(isPrefix);
    m_prefixText->setEnabled(isPrefix);
    m_currentPrefix = m_treeview->currentPrefix();
    m_prefixText->setText(m_currentPrefix);

    m_languageLabel->setEnabled(isPrefix);
    m_languageText->setEnabled(isPrefix);
    m_currentLanguage = m_treeview->currentLanguage();
    m_languageText->setText(m_currentLanguage);

    m_addFilesButton->setEnabled(isValid);
    m_removeButton->setEnabled(isValid);
}

void QrcEditor::updateHistoryControls()
{
    emit undoStackChanged(m_history.canUndo(), m_history.canRedo());
}

// Helper for resolveLocationIssues():
// For code clarity, a context with convenience functions to execute
// the dialogs required for checking the image file paths
// (and keep them around for file dialog execution speed).
// Basically, resolveLocationIssues() checks the paths of the images
// and asks the user to copy them into the resource file location.
// When the user does a multiselection of files, this requires popping
// up the dialog several times in a row.
struct ResolveLocationContext {
    QAbstractButton *execLocationMessageBox(QWidget *parent,
                                        const QString &file,
                                        bool wantSkipButton);
    QString execCopyFileDialog(QWidget *parent,
                               const QDir &dir,
                               const QString &targetPath);

    QScopedPointer<QMessageBox> messageBox;
    QScopedPointer<QFileDialog> copyFileDialog;

    QPushButton *copyButton = nullptr;
    QPushButton *skipButton = nullptr;
    QPushButton *abortButton = nullptr;
};

QAbstractButton *ResolveLocationContext::execLocationMessageBox(QWidget *parent,
                                                            const QString &file,
                                                            bool wantSkipButton)
{
    if (messageBox.isNull()) {
        messageBox.reset(new QMessageBox(QMessageBox::Warning,
                                         Tr::tr("Invalid file location"),
                                         QString(), QMessageBox::NoButton, parent));
        copyButton = messageBox->addButton(Tr::tr("Copy"), QMessageBox::ActionRole);
        abortButton = messageBox->addButton(Tr::tr("Abort"), QMessageBox::RejectRole);
        messageBox->setDefaultButton(copyButton);
    }
    if (wantSkipButton && !skipButton) {
        skipButton = messageBox->addButton(Tr::tr("Skip"), QMessageBox::DestructiveRole);
        messageBox->setEscapeButton(skipButton);
    }
    messageBox->setText(Tr::tr("The file %1 is not in a subdirectory of the resource file. "
                               "You now have the option to copy this file to a valid location.")
                    .arg(QDir::toNativeSeparators(file)));
    messageBox->exec();
    return messageBox->clickedButton();
}

QString ResolveLocationContext::execCopyFileDialog(QWidget *parent, const QDir &dir, const QString &targetPath)
{
    // Delayed creation of file dialog.
    if (copyFileDialog.isNull()) {
        copyFileDialog.reset(new QFileDialog(parent, Tr::tr("Choose Copy Location")));
        copyFileDialog->setFileMode(QFileDialog::AnyFile);
        copyFileDialog->setAcceptMode(QFileDialog::AcceptSave);
    }
    copyFileDialog->selectFile(targetPath);
    // Repeat until the path entered is no longer above 'dir'
    // (relative is not "../").
    while (true) {
        if (copyFileDialog->exec() != QDialog::Accepted)
            return QString();
        const QStringList files = copyFileDialog->selectedFiles();
        if (files.isEmpty())
            return QString();
        const QString relPath = dir.relativeFilePath(files.front());
        if (!relPath.startsWith(QLatin1String("../")))
            return files.front();
    }
    return QString();
}

// Helper to copy a file with message boxes
static inline bool copyFile(const QString &file, const QString &copyName, QWidget *parent)
{
    if (QFile::exists(copyName)) {
        if (!QFile::remove(copyName)) {
            QMessageBox::critical(parent, Tr::tr("Overwriting Failed"),
                                  Tr::tr("Could not overwrite file %1.")
                                  .arg(QDir::toNativeSeparators(copyName)));
            return false;
        }
    }
    if (!QFile::copy(file, copyName)) {
        QMessageBox::critical(parent, Tr::tr("Copying Failed"),
                              Tr::tr("Could not copy the file to %1.")
                              .arg(QDir::toNativeSeparators(copyName)));
        return false;
    }
    return true;
}

void QrcEditor::resolveLocationIssues(QStringList &files)
{
    const QDir dir = m_treeview->filePath().toFileInfo().absoluteDir();
    const QString dotdotSlash = QLatin1String("../");
    int i = 0;
    const int count = files.count();
    const int initialCount = files.count();

    // Find first troublesome file
    for (; i < count; i++) {
        QString const &file = files.at(i);
        const QString relativePath = dir.relativeFilePath(file);
        if (relativePath.startsWith(dotdotSlash))
            break;
    }

    // All paths fine -> no interaction needed
    if (i == count)
        return;

    // Interact with user from now on
    ResolveLocationContext context;
    bool abort = false;
    for (QStringList::iterator it = files.begin(); it != files.end(); ) {
        // Path fine -> skip file
        QString const &file = *it;
        QString const relativePath = dir.relativeFilePath(file);
        if (!relativePath.startsWith(dotdotSlash))
            continue;
        // Path troublesome and aborted -> remove file
        bool ok = false;
        if (!abort) {
            // Path troublesome -> query user "Do you want copy/abort/skip".
            QAbstractButton *clickedButton = context.execLocationMessageBox(this, file, initialCount > 1);
            if (clickedButton == context.copyButton) {
                const QFileInfo fi(file);
                QFileInfo suggestion;
                QDir tmpTarget(dir.path() + QLatin1Char('/') + QLatin1String("Resources"));
                if (tmpTarget.exists())
                    suggestion.setFile(tmpTarget, fi.fileName());
                else
                    suggestion.setFile(dir, fi.fileName());
                // Prompt for copy location, copy and replace name.
                const QString copyName = context.execCopyFileDialog(this, dir, suggestion.absoluteFilePath());
                ok = !copyName.isEmpty() && copyFile(file, copyName, this);
                if (ok)
                    *it = copyName;
            } else if (clickedButton == context.abortButton) {
                abort = true;
            }
        } // !abort
        if (ok) { // Remove files where user canceled or failures occurred.
            ++it;
        } else {
            it = files.erase(it);
        }
    } // for files
}

void QrcEditor::setResourceDragEnabled(bool e)
{
    m_treeview->setResourceDragEnabled(e);
}

bool QrcEditor::resourceDragEnabled() const
{
    return m_treeview->resourceDragEnabled();
}

void QrcEditor::editCurrentItem()
{
    if (m_treeview->selectionModel()->currentIndex().isValid())
        m_treeview->edit(m_treeview->selectionModel()->currentIndex());
}

QString QrcEditor::currentResourcePath() const
{
    return m_treeview->currentResourcePath();
}

// Slot for change of line edit content 'alias'
void QrcEditor::onAliasChanged(const QString &alias)
{
    const QString &before = m_currentAlias;
    const QString &after = alias;
    m_treeview->setCurrentAlias(before, after);
    m_currentAlias = alias;
    updateHistoryControls();
}

// Slot for change of line edit content 'prefix'
void QrcEditor::onPrefixChanged(const QString &prefix)
{
    const QString &before = m_currentPrefix;
    const QString &after = prefix;
    m_treeview->setCurrentPrefix(before, after);
    m_currentPrefix = prefix;
    updateHistoryControls();
}

// Slot for change of line edit content 'language'
void QrcEditor::onLanguageChanged(const QString &language)
{
    const QString &before = m_currentLanguage;
    const QString &after = language;
    m_treeview->setCurrentLanguage(before, after);
    m_currentLanguage = language;
    updateHistoryControls();
}

// Slot for 'Remove' button
void QrcEditor::onRemove()
{
    // Find current item, push and execute command
    const QModelIndex current = m_treeview->currentIndex();
    int afterDeletionArrayIndex = current.row();
    QModelIndex afterDeletionParent = current.parent();
    m_treeview->findSamePlacePostDeletionModelIndex(afterDeletionArrayIndex, afterDeletionParent);
    QUndoCommand * const removeCommand = new RemoveEntryCommand(m_treeview, current);
    m_history.push(removeCommand);
    const QModelIndex afterDeletionModelIndex
            = m_treeview->model()->index(afterDeletionArrayIndex, 0, afterDeletionParent);
    m_treeview->setCurrentIndex(afterDeletionModelIndex);
    updateHistoryControls();
}

// Slot for 'Remove missing files' button
void QrcEditor::onRemoveNonExisting()
{
    QList<QModelIndex> toRemove = m_treeview->nonExistingFiles();

    QUndoCommand * const removeCommand = new RemoveMultipleEntryCommand(m_treeview, toRemove);
    m_history.push(removeCommand);
    updateHistoryControls();
}

// Slot for 'Add File' button
void QrcEditor::onAddFiles()
{
    QModelIndex const current = m_treeview->currentIndex();
    int const currentIsPrefixNode = m_treeview->isPrefix(current);
    int const prefixArrayIndex = currentIsPrefixNode ? current.row()
            : m_treeview->model()->parent(current).row();
    int const cursorFileArrayIndex = currentIsPrefixNode ? 0 : current.row();
    QStringList fileNames = m_treeview->fileNamesToAdd();
    fileNames = m_treeview->existingFilesSubtracted(prefixArrayIndex, fileNames);
    resolveLocationIssues(fileNames);
    if (fileNames.isEmpty())
        return;
    QUndoCommand * const addFilesCommand = new AddFilesCommand(
            m_treeview, prefixArrayIndex, cursorFileArrayIndex, fileNames);
    m_history.push(addFilesCommand);
    updateHistoryControls();
}

// Slot for 'Add Prefix' button
void QrcEditor::onAddPrefix()
{
    QUndoCommand * const addEmptyPrefixCommand = new AddEmptyPrefixCommand(m_treeview);
    m_history.push(addEmptyPrefixCommand);
    updateHistoryControls();
    m_prefixText->selectAll();
    m_prefixText->setFocus();
}

// Slot for 'Undo' button
void QrcEditor::onUndo()
{
    m_history.undo();
    updateCurrent();
    updateHistoryControls();
}

// Slot for 'Redo' button
void QrcEditor::onRedo()
{
    m_history.redo();
    updateCurrent();
    updateHistoryControls();
}

} // ResourceEditor::Internal
