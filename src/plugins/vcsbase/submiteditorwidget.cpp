// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "submiteditorwidget.h"
#include "submitfieldwidget.h"
#include "submitfilemodel.h"
#include "ui_submiteditorwidget.h"

#include <utils/algorithm.h>
#include <utils/guard.h>
#include <utils/theme/theme.h>

#include <QDebug>
#include <QPointer>
#include <QTextBlock>
#include <QTimer>
#include <QScopedPointer>

#include <QMenu>
#include <QHBoxLayout>
#include <QToolButton>
#include <QSpacerItem>
#include <QShortcut>

using namespace Utils;

enum { debug = 0 };
enum { defaultLineWidth = 72 };

/*!
    \class VcsBase::SubmitEditorWidget

    \brief The SubmitEditorWidget class presents a VCS commit message in a text
    editor and a
     checkable list of modified files in a list window.

    The user can delete files from the list by unchecking them or diff the selection
    by doubleclicking. A list model which contains state and file columns should be
    set using setFileModel().

    Additionally, standard creator actions  can be registered:
    Undo/redo will be set up to work with the description editor.
    Submit will be set up to be enabled according to checkstate.
    Diff will be set up to trigger diffSelected().

    Note that the actions are connected by signals; in the rare event that there
    are several instances of the SubmitEditorWidget belonging to the same
    context active, the actions must be registered/unregistered in the editor
    change event.
    Care should be taken to ensure the widget is deleted properly when the
    editor closes.
*/

namespace VcsBase {

// QActionPushButton: A push button tied to an action
// (similar to a QToolButton)
class QActionPushButton : public QToolButton
{
    Q_OBJECT
public:
    explicit QActionPushButton(QAction *a);
};

QActionPushButton::QActionPushButton(QAction *a) :
     QToolButton()
{
    setIcon(a->icon());
    setText(a->text());
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(a, &QAction::changed, this, [this, a] {
        setEnabled(a->isEnabled());
        setText(a->text());
    });
    connect(this, &QAbstractButton::clicked, a, &QAction::trigger);
    setEnabled(a->isEnabled());
}

// Helpers to retrieve model data
// Convenience to extract a list of selected indexes
// -----------  SubmitEditorWidgetPrivate

struct SubmitEditorWidgetPrivate
{
    // A pair of position/action to extend context menus
    typedef QPair<int, QPointer<QAction> > AdditionalContextMenuAction;

    Ui::SubmitEditorWidget m_ui;

    QList<AdditionalContextMenuAction> descriptionEditContextMenuActions;
    QVBoxLayout *m_fieldLayout = nullptr;
    QList<SubmitFieldWidget *> m_fieldWidgets;
    QShortcut *m_submitShortcut = nullptr;
    QActionPushButton *m_submitButton = nullptr;
    QString m_description;

    int m_lineWidth = defaultLineWidth;
    int m_activatedRow = -1;

    bool m_filesSelected = false;
    bool m_emptyFileListEnabled = false;
    bool m_commitEnabled = false;
    bool m_descriptionMandatory = true;
    bool m_updateInProgress = false;
    Guard m_ignoreChanges;
};

SubmitEditorWidget::SubmitEditorWidget() :
    d(new SubmitEditorWidgetPrivate)
{
    d->m_ui.setupUi(this);
    d->m_ui.description->setContextMenuPolicy(Qt::CustomContextMenu);
    d->m_ui.description->setLineWrapMode(QTextEdit::NoWrap);
    d->m_ui.description->setWordWrapMode(QTextOption::WordWrap);
    connect(d->m_ui.description, &QWidget::customContextMenuRequested,
            this, &SubmitEditorWidget::editorCustomContextMenuRequested);
    connect(d->m_ui.description, &QTextEdit::textChanged,
            this, &SubmitEditorWidget::descriptionTextChanged);

    // File List
    d->m_ui.fileView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d->m_ui.fileView, &QWidget::customContextMenuRequested,
            this, &SubmitEditorWidget::fileListCustomContextMenuRequested);
    d->m_ui.fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    d->m_ui.fileView->setRootIsDecorated(false);
    connect(d->m_ui.fileView, &QAbstractItemView::doubleClicked,
            this, &SubmitEditorWidget::diffActivated);

    connect(d->m_ui.checkAllCheckBox, &QCheckBox::stateChanged,
            this, &SubmitEditorWidget::checkAllToggled);

    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(d->m_ui.description);
}

SubmitEditorWidget::~SubmitEditorWidget()
{
    delete d;
}

void SubmitEditorWidget::registerActions(QAction *editorUndoAction, QAction *editorRedoAction,
                         QAction *submitAction, QAction *diffAction)
{
    if (editorUndoAction) {
        editorUndoAction->setEnabled(d->m_ui.description->document()->isUndoAvailable());
        connect(d->m_ui.description, &QTextEdit::undoAvailable,
                editorUndoAction, &QAction::setEnabled);
        connect(editorUndoAction, &QAction::triggered, d->m_ui.description, &QTextEdit::undo);
    }
    if (editorRedoAction) {
        editorRedoAction->setEnabled(d->m_ui.description->document()->isRedoAvailable());
        connect(d->m_ui.description, &QTextEdit::redoAvailable,
                editorRedoAction, &QAction::setEnabled);
        connect(editorRedoAction, &QAction::triggered, d->m_ui.description, &QTextEdit::redo);
    }

    if (submitAction) {
        if (debug) {
            const SubmitFileModel *model = fileModel();
            int count = model ? model->rowCount() : 0;
            qDebug() << Q_FUNC_INFO << submitAction << count << "items";
        }
        d->m_commitEnabled = !canSubmit();
        connect(this, &SubmitEditorWidget::submitActionEnabledChanged,
                submitAction, &QAction::setEnabled);
        connect(this, &SubmitEditorWidget::submitActionTextChanged,
                submitAction, &QAction::setText);
        d->m_submitButton = new QActionPushButton(submitAction);
        d->m_ui.buttonLayout->addWidget(d->m_submitButton);
        if (!d->m_submitShortcut)
            d->m_submitShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this);
        connect(d->m_submitShortcut, &QShortcut::activated,
                submitAction, [submitAction] {
            if (submitAction->isEnabled())
                submitAction->trigger();
        });
    }
    if (diffAction) {
        if (debug)
            qDebug() << diffAction << d->m_filesSelected;
        diffAction->setEnabled(d->m_filesSelected);
        connect(this, &SubmitEditorWidget::fileSelectionChanged, diffAction, &QAction::setEnabled);
        connect(diffAction, &QAction::triggered, this, &SubmitEditorWidget::triggerDiffSelected);
        d->m_ui.buttonLayout->addWidget(new QActionPushButton(diffAction));
    }
}

// Make sure we have one terminating NL. Do not trim front as leading space might be
// required for some formattings.
void SubmitEditorWidget::trimDescription()
{
    if (d->m_description.isEmpty())
        return;
    // Trim back of string.
    const int last = d->m_description.size() - 1;
    int lastWordCharacter = last;
    for ( ; lastWordCharacter >= 0 && d->m_description.at(lastWordCharacter).isSpace() ;
          lastWordCharacter--)
    { }
    if (lastWordCharacter != last)
        d->m_description.truncate(lastWordCharacter + 1);
    d->m_description += QLatin1Char('\n');
}

// Extract the wrapped text from a text edit, which performs
// the wrapping only optically.
void SubmitEditorWidget::wrapDescription()
{
    if (!lineWrap())
        return;
    const QChar newLine = QLatin1Char('\n');
    QTextEdit e;
    e.setVisible(false);
    e.setMinimumWidth(1000);
    e.setFontPointSize(1.0);
    e.setLineWrapColumnOrWidth(d->m_ui.description->lineWrapColumnOrWidth());
    e.setLineWrapMode(d->m_ui.description->lineWrapMode());
    e.setWordWrapMode(d->m_ui.description->wordWrapMode());
    e.setPlainText(d->m_description);
    d->m_description.clear();
    QTextCursor cursor(e.document());
    cursor.movePosition(QTextCursor::Start);
    while (!cursor.atEnd()) {
        const QString block = cursor.block().text();
        if (block.startsWith(QLatin1Char('\t'))) { // Don't wrap
            d->m_description += block + newLine;
            cursor.movePosition(QTextCursor::EndOfBlock);
        } else {
            forever {
                cursor.select(QTextCursor::LineUnderCursor);
                d->m_description += cursor.selectedText();
                d->m_description += newLine;
                cursor.clearSelection();
                if (cursor.atBlockEnd())
                    break;
                cursor.movePosition(QTextCursor::NextCharacter);
            }
        }
        cursor.movePosition(QTextCursor::NextBlock);
    }
}

QString SubmitEditorWidget::descriptionText() const
{
    return d->m_description;
}

void SubmitEditorWidget::setDescriptionText(const QString &text)
{
    d->m_ui.description->setPlainText(text);
}

bool SubmitEditorWidget::lineWrap() const
{
    return d->m_ui.description->lineWrapMode() != QTextEdit::NoWrap;
}

void SubmitEditorWidget::setLineWrap(bool v)
{
    if (debug)
        qDebug() << Q_FUNC_INFO << v;
    if (v) {
        d->m_ui.description->setLineWrapColumnOrWidth(d->m_lineWidth);
        d->m_ui.description->setLineWrapMode(QTextEdit::FixedColumnWidth);
    } else {
        d->m_ui.description->setLineWrapMode(QTextEdit::NoWrap);
    }
    descriptionTextChanged();
}

int SubmitEditorWidget::lineWrapWidth() const
{
    return d->m_lineWidth;
}

void SubmitEditorWidget::setLineWrapWidth(int v)
{
    if (debug)
        qDebug() << Q_FUNC_INFO << v << lineWrap();
    if (d->m_lineWidth == v)
        return;
    d->m_lineWidth = v;
    if (lineWrap())
        d->m_ui.description->setLineWrapColumnOrWidth(v);
    descriptionTextChanged();
}

bool SubmitEditorWidget::isDescriptionMandatory() const
{
    return d->m_descriptionMandatory;
}

void SubmitEditorWidget::setDescriptionMandatory(bool v)
{
    d->m_descriptionMandatory = v;
}

QAbstractItemView::SelectionMode SubmitEditorWidget::fileListSelectionMode() const
{
    return d->m_ui.fileView->selectionMode();
}

void SubmitEditorWidget::setFileListSelectionMode(QAbstractItemView::SelectionMode sm)
{
    d->m_ui.fileView->setSelectionMode(sm);
}

void SubmitEditorWidget::setFileModel(SubmitFileModel *model)
{
    d->m_ui.fileView->clearSelection(); // trigger the change signals

    d->m_ui.fileView->setModel(model);

    if (model->rowCount()) {
        const int columnCount = model->columnCount();
        for (int c = 0;  c < columnCount; c++)
            d->m_ui.fileView->resizeColumnToContents(c);
    }

    connect(model, &QAbstractItemModel::dataChanged,
            this, &SubmitEditorWidget::updateSubmitAction);
    connect(model, &QAbstractItemModel::modelReset,
            this, &SubmitEditorWidget::updateSubmitAction);
    connect(model, &QAbstractItemModel::dataChanged,
            this, &SubmitEditorWidget::updateCheckAllComboBox);
    connect(model, &QAbstractItemModel::modelReset,
            this, &SubmitEditorWidget::updateCheckAllComboBox);
    connect(model, &QAbstractItemModel::rowsInserted,
            this, &SubmitEditorWidget::updateSubmitAction);
    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &SubmitEditorWidget::updateSubmitAction);
    connect(d->m_ui.fileView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SubmitEditorWidget::updateDiffAction);
    updateActions();
}

SubmitFileModel *SubmitEditorWidget::fileModel() const
{
    return static_cast<SubmitFileModel *>(d->m_ui.fileView->model());
}

QStringList SubmitEditorWidget::checkedFiles() const
{
    QStringList rc;
    const SubmitFileModel *model = fileModel();
    if (!model)
        return rc;
    const int count = model->rowCount();
    for (int i = 0; i < count; i++)
        if (model->checked(i))
            rc.push_back(model->file(i));
    return rc;
}

CompletingTextEdit *SubmitEditorWidget::descriptionEdit() const
{
    return d->m_ui.description;
}

void SubmitEditorWidget::triggerDiffSelected()
{
    const QList<int> sel = selectedRows();
    if (!sel.empty())
        emit diffSelected(sel);
}

void SubmitEditorWidget::diffActivatedDelayed()
{
    emit diffSelected(QList<int>() << d->m_activatedRow);
}

void SubmitEditorWidget::diffActivated(const QModelIndex &index)
{
    // We need to delay the signal, otherwise, the diff editor will not
    // be in the foreground.
    d->m_activatedRow = index.row();
    QTimer::singleShot(0, this, &SubmitEditorWidget::diffActivatedDelayed);
}

void SubmitEditorWidget::updateActions()
{
    updateSubmitAction();
    updateDiffAction();
    updateCheckAllComboBox();
}

// Enable submit depending on having checked files
void SubmitEditorWidget::updateSubmitAction()
{
    const unsigned checkedCount = checkedFilesCount();
    const bool newCommitState = canSubmit();
    // Emit signal to update action
    if (d->m_commitEnabled != newCommitState) {
        d->m_commitEnabled = newCommitState;
        emit submitActionEnabledChanged(d->m_commitEnabled);
    }
    if (d->m_ui.fileView && d->m_ui.fileView->model()) {
        // Update button text.
        const int fileCount = d->m_ui.fileView->model()->rowCount();
        const QString msg = checkedCount ?
                            tr("%1 %2/%n File(s)", nullptr, fileCount)
                            .arg(commitName()).arg(checkedCount) :
                            commitName();
        emit submitActionTextChanged(msg);
    }
}

void SubmitEditorWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange)
        verifyDescription();
}

// Enable diff depending on selected files
void SubmitEditorWidget::updateDiffAction()
{
    const bool filesSelected = hasSelection();
    if (d->m_filesSelected != filesSelected) {
        d->m_filesSelected = filesSelected;
        emit fileSelectionChanged(d->m_filesSelected);
    }
}

void SubmitEditorWidget::updateCheckAllComboBox()
{
    const GuardLocker locker(d->m_ignoreChanges);
    int checkedCount = checkedFilesCount();
    if (checkedCount == 0)
        d->m_ui.checkAllCheckBox->setCheckState(Qt::Unchecked);
    else if (checkedCount == d->m_ui.fileView->model()->rowCount())
        d->m_ui.checkAllCheckBox->setCheckState(Qt::Checked);
    else
        d->m_ui.checkAllCheckBox->setCheckState(Qt::PartiallyChecked);
}

bool SubmitEditorWidget::hasSelection() const
{
    // Not present until model is set
    if (const QItemSelectionModel *sm = d->m_ui.fileView->selectionModel())
        return sm->hasSelection();
    return false;
}

int SubmitEditorWidget::checkedFilesCount() const
{
    int checkedCount = 0;
    if (const SubmitFileModel *model = fileModel()) {
        const int count = model->rowCount();
        for (int i = 0; i < count; ++i)
            if (model->checked(i))
                ++checkedCount;
    }
    return checkedCount;
}

QString SubmitEditorWidget::cleanupDescription(const QString &input) const
{
    return input;
}

void SubmitEditorWidget::insertTopWidget(QWidget *w)
{
    d->m_ui.vboxLayout->insertWidget(0, w);
}

void SubmitEditorWidget::insertLeftWidget(QWidget *w)
{
    d->m_ui.splitter->insertWidget(0, w);
}

void SubmitEditorWidget::addSubmitButtonMenu(QMenu *menu)
{
    d->m_submitButton->setMenu(menu);
}

void SubmitEditorWidget::hideDescription()
{
    d->m_ui.descriptionBox->hide();
    setDescriptionMandatory(false);
}

void SubmitEditorWidget::verifyDescription()
{
    if (!isEnabled()) {
        d->m_ui.descriptionHint->setText(QString());
        d->m_ui.descriptionHint->setToolTip(QString());
        return;
    }

    auto fontColor = [](Utils::Theme::Color color) {
        return QString("<font color=\"%1\">")
                .arg(Utils::creatorTheme()->color(color).name());
    };
    const QString hint = fontColor(Utils::Theme::OutputPanes_TestWarnTextColor);
    const QString warning = fontColor(Utils::Theme::TextColorError);

    const QChar newLine = '\n';
    const int descriptionLength = d->m_description.length();
    int subjectLength = d->m_description.indexOf(newLine);
    int secondLineLength = 0;
    if (subjectLength >= 0) {
        const int secondLineStart = subjectLength + 1;
        int secondLineEnd = d->m_description.indexOf(newLine, secondLineStart);
        if (secondLineEnd == -1)
            secondLineEnd = descriptionLength;
        secondLineLength = secondLineEnd - secondLineStart;
    } else {
        subjectLength = descriptionLength;
    }

    enum { MinSubjectLength = 20, MaxSubjectLength = 72, WarningSubjectLength = 55 };
    QStringList hints;
    if (subjectLength < MinSubjectLength)
        hints.append(warning + tr("Warning: The commit subject is very short."));

    if (subjectLength > MaxSubjectLength)
        hints.append(warning + tr("Warning: The commit subject is too long."));
    else if (subjectLength > WarningSubjectLength)
        hints.append(hint + tr("Hint: Aim for a shorter commit subject."));

    if (secondLineLength > 0)
        hints.append(hint + tr("Hint: The second line of a commit message should be empty."));

    d->m_ui.descriptionHint->setText(hints.join("<br>"));
    if (!d->m_ui.descriptionHint->text().isEmpty()) {
        d->m_ui.descriptionHint->setToolTip(
                    tr("<p>Writing good commit messages</p>"
                       "<ul>"
                       "<li>Avoid very short commit messages.</li>"
                       "<li>Consider the first line as subject (like in email) "
                       "and keep it shorter than %n characters.</li>"
                       "<li>After an empty second line, a longer description can be added.</li>"
                       "<li>Describe why the change was done, not how it was done.</li>"
                       "</ul>", nullptr, MaxSubjectLength));
        }
}

void SubmitEditorWidget::descriptionTextChanged()
{
    d->m_description = cleanupDescription(d->m_ui.description->toPlainText());
    verifyDescription();
    wrapDescription();
    trimDescription();
    // append field entries
    for (const SubmitFieldWidget *fw : qAsConst(d->m_fieldWidgets))
        d->m_description += fw->fieldValues();
    updateSubmitAction();
}

bool SubmitEditorWidget::canSubmit(QString *whyNot) const
{
    if (d->m_updateInProgress) {
        if (whyNot)
            *whyNot = tr("Update in progress");
        return false;
    }
    if (isDescriptionMandatory() && d->m_description.trimmed().isEmpty()) {
        if (whyNot)
            *whyNot = tr("Description is empty");
        return false;
    }
    const unsigned checkedCount = checkedFilesCount();
    const bool res = d->m_emptyFileListEnabled || checkedCount > 0;
    if (!res && whyNot)
        *whyNot = tr("No files checked");
    return res;
}

void SubmitEditorWidget::setUpdateInProgress(bool value)
{
    d->m_updateInProgress = value;
    updateSubmitAction();
}

bool SubmitEditorWidget::updateInProgress() const
{
    return d->m_updateInProgress;
}

QList<int> SubmitEditorWidget::selectedRows() const
{
    return Utils::transform(d->m_ui.fileView->selectionModel()->selectedRows(0), &QModelIndex::row);
}

void SubmitEditorWidget::setSelectedRows(const QList<int> &rows)
{
    if (const SubmitFileModel *model = fileModel()) {
        QItemSelectionModel *selectionModel = d->m_ui.fileView->selectionModel();
        for (int row : rows) {
            selectionModel->select(model->index(row, 0),
                                   QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
}

QString SubmitEditorWidget::commitName() const
{
    return tr("&Commit");
}

void SubmitEditorWidget::addSubmitFieldWidget(SubmitFieldWidget *f)
{
    if (!d->m_fieldLayout) {
        // VBox with horizontal, expanding spacer
        d->m_fieldLayout = new QVBoxLayout;
        auto outerLayout = new QHBoxLayout;
        outerLayout->addLayout(d->m_fieldLayout);
        outerLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Ignored));
        d->m_ui.descriptionLayout->addLayout(outerLayout);
    }
    d->m_fieldLayout->addWidget(f);
    d->m_fieldWidgets.push_back(f);
}

QList<SubmitFieldWidget *> SubmitEditorWidget::submitFieldWidgets() const
{
    return d->m_fieldWidgets;
}

void SubmitEditorWidget::addDescriptionEditContextMenuAction(QAction *a)
{
    d->descriptionEditContextMenuActions
            .push_back(SubmitEditorWidgetPrivate::AdditionalContextMenuAction(-1, a));
}

void SubmitEditorWidget::insertDescriptionEditContextMenuAction(int pos, QAction *a)
{
    d->descriptionEditContextMenuActions
            .push_back(SubmitEditorWidgetPrivate::AdditionalContextMenuAction(pos, a));
}

void SubmitEditorWidget::editorCustomContextMenuRequested(const QPoint &pos)
{
    QScopedPointer<QMenu> menu(d->m_ui.description->createStandardContextMenu());
    // Extend
    for (const SubmitEditorWidgetPrivate::AdditionalContextMenuAction &a :
         qAsConst(d->descriptionEditContextMenuActions)) {
        if (a.second) {
            if (a.first >= 0)
                menu->insertAction(menu->actions().at(a.first), a.second);
            else
                menu->addAction(a.second);
        }
    }
    menu->exec(d->m_ui.description->mapToGlobal(pos));
}

void SubmitEditorWidget::checkAllToggled()
{
    if (d->m_ignoreChanges.isLocked())
        return;
    Qt::CheckState checkState = d->m_ui.checkAllCheckBox->checkState();
    fileModel()->setAllChecked(checkState == Qt::Checked || checkState == Qt::PartiallyChecked);
    // Reset that again, so that the user can't do it
    d->m_ui.checkAllCheckBox->setTristate(false);
}

void SubmitEditorWidget::fileListCustomContextMenuRequested(const QPoint & pos)
{
    // Execute menu offering to check/uncheck all
    QMenu menu;
    //: Check all for submit
    QAction *checkAllAction = menu.addAction(tr("Select All"));
    //: Uncheck all for submit
    QAction *uncheckAllAction = menu.addAction(tr("Unselect All"));
    QAction *action = menu.exec(d->m_ui.fileView->mapToGlobal(pos));
    if (action == checkAllAction) {
        fileModel()->setAllChecked(true);;
        return;
    }
    if (action == uncheckAllAction) {
        fileModel()->setAllChecked(false);
        return;
    }
}

bool SubmitEditorWidget::isEmptyFileListEnabled() const
{
    return d->m_emptyFileListEnabled;
}

void SubmitEditorWidget::setEmptyFileListEnabled(bool e)
{
    if (e != d->m_emptyFileListEnabled) {
        d->m_emptyFileListEnabled = e;
        updateSubmitAction();
    }
}

} // namespace VcsBase

#include "submiteditorwidget.moc"
