// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "deviceprocessesdialog.h"
#include "deviceprocesslist.h"
#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/kitchooser.h>
#include <projectexplorer/kitinformation.h>

#include <utils/fancylineedit.h>
#include <utils/itemviews.h>
#include <utils/processinfo.h>
#include <utils/qtcassert.h>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTextBrowser>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// ProcessListFilterModel
//
///////////////////////////////////////////////////////////////////////

class ProcessListFilterModel : public QSortFilterProxyModel
{
public:
    ProcessListFilterModel();
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

ProcessListFilterModel::ProcessListFilterModel()
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
    setFilterKeyColumn(-1);
}

bool ProcessListFilterModel::lessThan(const QModelIndex &left,
    const QModelIndex &right) const
{
    const QString l = sourceModel()->data(left).toString();
    const QString r = sourceModel()->data(right).toString();
    if (left.column() == 0)
        return l.toInt() < r.toInt();
    return l < r;
}

///////////////////////////////////////////////////////////////////////
//
// DeviceProcessesDialogPrivate
//
///////////////////////////////////////////////////////////////////////

class DeviceProcessesDialogPrivate : public QObject
{
    Q_OBJECT

public:
    DeviceProcessesDialogPrivate(KitChooser *chooser, QDialog *parent);

    void setDevice(const IDevice::ConstPtr &device);
    void updateProcessList();
    void updateDevice();
    void killProcess();
    void handleRemoteError(const QString &errorMsg);
    void handleProcessListUpdated();
    void handleProcessKilled();
    void updateButtons();
    ProcessInfo selectedProcess() const;

    QDialog *q;
    std::unique_ptr<DeviceProcessList> processList;
    ProcessListFilterModel proxyModel;
    QLabel *kitLabel;
    KitChooser *kitChooser;

    TreeView *procView;
    QTextBrowser *errorText;
    FancyLineEdit *processFilterLineEdit;
    QPushButton *updateListButton;
    QPushButton *killProcessButton;
    QPushButton *acceptButton;
    QDialogButtonBox *buttonBox;
};

DeviceProcessesDialogPrivate::DeviceProcessesDialogPrivate(KitChooser *chooser, QDialog *parent)
    : q(parent)
    , kitLabel(new QLabel(DeviceProcessesDialog::tr("Kit:"), parent))
    , kitChooser(chooser)
    , acceptButton(nullptr)
    , buttonBox(new QDialogButtonBox(parent))
{
    q->setWindowTitle(DeviceProcessesDialog::tr("List of Processes"));
    q->setMinimumHeight(500);

    processFilterLineEdit = new FancyLineEdit(q);
    processFilterLineEdit->setPlaceholderText(DeviceProcessesDialog::tr("Filter"));
    processFilterLineEdit->setFocus(Qt::TabFocusReason);
    processFilterLineEdit->setHistoryCompleter(QLatin1String("DeviceProcessDialogFilter"),
        true /*restoreLastItemFromHistory*/);
    processFilterLineEdit->setFiltering(true);

    kitChooser->populate();

    procView = new TreeView(q);
    procView->setModel(&proxyModel);
    procView->setSelectionBehavior(QAbstractItemView::SelectRows);
    procView->setSelectionMode(QAbstractItemView::SingleSelection);
    procView->setUniformRowHeights(true);
    procView->setRootIsDecorated(false);
    procView->setAlternatingRowColors(true);
    procView->setSortingEnabled(true);
    procView->header()->setDefaultSectionSize(100);
    procView->header()->setStretchLastSection(true);
    procView->sortByColumn(1, Qt::AscendingOrder);
    procView->setActivationMode(DoubleClickActivation);

    errorText = new QTextBrowser(q);

    updateListButton = new QPushButton(DeviceProcessesDialog::tr("&Update List"), q);
    killProcessButton = new QPushButton(DeviceProcessesDialog::tr("&Kill Process"), q);

    buttonBox->addButton(updateListButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(killProcessButton, QDialogButtonBox::ActionRole);

    auto *leftColumn = new QFormLayout();
    leftColumn->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    leftColumn->addRow(kitLabel, kitChooser);
    leftColumn->addRow(DeviceProcessesDialog::tr("&Filter:"), processFilterLineEdit);

//    QVBoxLayout *rightColumn = new QVBoxLayout();
//    rightColumn->addWidget(updateListButton);
//    rightColumn->addWidget(killProcessButton);
//    rightColumn->addStretch();

//    QHBoxLayout *horizontalLayout = new QHBoxLayout();
//    horizontalLayout->addLayout(leftColumn);
//    horizontalLayout->addLayout(rightColumn);

    auto *mainLayout = new QVBoxLayout(q);
    mainLayout->addLayout(leftColumn);
    mainLayout->addWidget(procView);
    mainLayout->addWidget(errorText);
    mainLayout->addWidget(buttonBox);

//    QFrame *line = new QFrame(this);
//    line->setFrameShape(QFrame::HLine);
//    line->setFrameShadow(QFrame::Sunken);

    proxyModel.setFilterRegularExpression(processFilterLineEdit->text());

    connect(processFilterLineEdit, &FancyLineEdit::textChanged,
            &proxyModel, QOverload<const QString &>::of(&ProcessListFilterModel::setFilterRegularExpression));
    connect(procView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &DeviceProcessesDialogPrivate::updateButtons);
    connect(updateListButton, &QAbstractButton::clicked,
            this, &DeviceProcessesDialogPrivate::updateProcessList);
    connect(kitChooser, &KitChooser::currentIndexChanged,
            this, &DeviceProcessesDialogPrivate::updateDevice);
    connect(killProcessButton, &QAbstractButton::clicked,
            this, &DeviceProcessesDialogPrivate::killProcess);
    connect(&proxyModel, &QAbstractItemModel::layoutChanged,
            this, &DeviceProcessesDialogPrivate::handleProcessListUpdated);
    connect(buttonBox, &QDialogButtonBox::accepted, q, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

    QWidget::setTabOrder(kitChooser, processFilterLineEdit);
    QWidget::setTabOrder(processFilterLineEdit, procView);
    QWidget::setTabOrder(procView, buttonBox);
}

void DeviceProcessesDialogPrivate::setDevice(const IDevice::ConstPtr &device)
{
    processList.reset();
    proxyModel.setSourceModel(nullptr);
    if (!device)
        return;

    processList.reset(device->createProcessListModel());
    QTC_ASSERT(processList, return);
    proxyModel.setSourceModel(processList->model());

    connect(processList.get(), &DeviceProcessList::error,
            this, &DeviceProcessesDialogPrivate::handleRemoteError);
    connect(processList.get(), &DeviceProcessList::processListUpdated,
            this, &DeviceProcessesDialogPrivate::handleProcessListUpdated);
    connect(processList.get(), &DeviceProcessList::processKilled,
            this, &DeviceProcessesDialogPrivate::handleProcessKilled, Qt::QueuedConnection);

    updateButtons();
    updateProcessList();
}

void DeviceProcessesDialogPrivate::handleRemoteError(const QString &errorMsg)
{
    QMessageBox::critical(q, tr("Remote Error"), errorMsg);
    updateListButton->setEnabled(true);
    updateButtons();
}

void DeviceProcessesDialogPrivate::handleProcessListUpdated()
{
    updateListButton->setEnabled(true);
    procView->resizeColumnToContents(0);
    procView->resizeColumnToContents(1);
    updateButtons();
}

void DeviceProcessesDialogPrivate::updateProcessList()
{
    updateListButton->setEnabled(false);
    killProcessButton->setEnabled(false);
    if (processList)
        processList->update();
}

void DeviceProcessesDialogPrivate::killProcess()
{
    const QModelIndexList indexes = procView->selectionModel()->selectedIndexes();
    if (indexes.empty() || !processList)
        return;
    updateListButton->setEnabled(false);
    killProcessButton->setEnabled(false);
    processList->killProcess(proxyModel.mapToSource(indexes.first()).row());
}

void DeviceProcessesDialogPrivate::updateDevice()
{
    setDevice(DeviceKitAspect::device(kitChooser->currentKit()));
}

void DeviceProcessesDialogPrivate::handleProcessKilled()
{
    updateProcessList();
}

void DeviceProcessesDialogPrivate::updateButtons()
{
    const bool hasSelection = procView->selectionModel()->hasSelection();
    if (acceptButton)
        acceptButton->setEnabled(hasSelection);
    killProcessButton->setEnabled(hasSelection);
    errorText->setVisible(!errorText->document()->isEmpty());
}

ProcessInfo DeviceProcessesDialogPrivate::selectedProcess() const
{
    const QModelIndexList indexes = procView->selectionModel()->selectedIndexes();
    if (indexes.empty() || !processList)
        return ProcessInfo();
    return processList->at(proxyModel.mapToSource(indexes.first()).row());
}

} // namespace Internal

///////////////////////////////////////////////////////////////////////
//
// DeviceProcessesDialog
//
///////////////////////////////////////////////////////////////////////

/*!
     \class ProjectExplorer::DeviceProcessesDialog

     \brief The DeviceProcessesDialog class shows a list of processes.

     The dialog can be used as a:
     \list
     \li Non-modal dialog showing a list of processes. Call addCloseButton()
         to add a \gui Close button.
     \li Modal dialog with an \gui Accept button to select a process. Call
         addAcceptButton() passing the label text. This will create a
         \gui Cancel button as well.
     \endlist
*/

DeviceProcessesDialog::DeviceProcessesDialog(QWidget *parent)
    : QDialog(parent), d(std::make_unique<Internal::DeviceProcessesDialogPrivate>(new KitChooser(this), this))
{ }

DeviceProcessesDialog::DeviceProcessesDialog(KitChooser *chooser, QWidget *parent)
    : QDialog(parent), d(std::make_unique<Internal::DeviceProcessesDialogPrivate>(chooser, this))
{ }

DeviceProcessesDialog::~DeviceProcessesDialog() = default;

void DeviceProcessesDialog::addAcceptButton(const QString &label)
{
    d->acceptButton = new QPushButton(label);
    d->buttonBox->addButton(d->acceptButton, QDialogButtonBox::AcceptRole);
    connect(d->procView, &QAbstractItemView::activated,
            d->acceptButton, &QAbstractButton::click);
    d->buttonBox->addButton(QDialogButtonBox::Cancel);
}

void DeviceProcessesDialog::addCloseButton()
{
    d->buttonBox->addButton(QDialogButtonBox::Close);
}

void DeviceProcessesDialog::setKitVisible(bool v)
{
    d->kitLabel->setVisible(v);
    d->kitChooser->setVisible(v);
}

void DeviceProcessesDialog::setDevice(const IDevice::ConstPtr &device)
{
    setKitVisible(false);
    d->setDevice(device);
}

void DeviceProcessesDialog::showAllDevices()
{
    setKitVisible(true);
    d->updateDevice();
}

ProcessInfo DeviceProcessesDialog::currentProcess() const
{
    return d->selectedProcess();
}

KitChooser *DeviceProcessesDialog::kitChooser() const
{
    return d->kitChooser;
}

void DeviceProcessesDialog::logMessage(const QString &line)
{
    d->errorText->setVisible(true);
    d->errorText->append(line);
}

} // namespace ProjectExplorer

#include "deviceprocessesdialog.moc"
