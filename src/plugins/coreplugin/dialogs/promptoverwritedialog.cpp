// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "promptoverwritedialog.h"

#include <utils/stringutils.h>

#include <QTreeView>
#include <QLabel>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include <QDir>

enum { FileNameRole = Qt::UserRole + 1 };

/*!
    \class Core::PromptOverwriteDialog
    \inmodule QtCreator
    \internal
    \brief The PromptOverwriteDialog class implements a dialog that asks
    users whether they want to overwrite files.

    The dialog displays the common folder and the files in a list where users
    can select the files to overwrite.
*/

static inline QString fileNameOfItem(const QStandardItem *item)
{
    return item->data(FileNameRole).toString();
}

namespace Core {

PromptOverwriteDialog::PromptOverwriteDialog(QWidget *parent) :
    QDialog(parent),
    m_label(new QLabel),
    m_view(new QTreeView),
    m_model(new QStandardItemModel(0, 1, this))
{
    setWindowTitle(tr("Overwrite Existing Files"));
    setModal(true);
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_label);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setHeaderHidden(true);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setModel(m_model);
    mainLayout->addWidget(m_view);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(bb);
}

void PromptOverwriteDialog::setFiles(const QStringList &l)
{
    // Format checkable list excluding common path
    const QString nativeCommonPath = QDir::toNativeSeparators(Utils::commonPath(l));
    for (const QString &fileName : l) {
        const QString nativeFileName = QDir::toNativeSeparators(fileName);
        const int length = nativeFileName.size() - nativeCommonPath.size() - 1;
        QStandardItem *item = new QStandardItem(nativeFileName.right(length));
        item->setData(QVariant(fileName), FileNameRole);
        item->setFlags(Qt::ItemIsEnabled);
        item->setCheckable(true);
        item->setCheckState(Qt::Checked);
        m_model->appendRow(item);
    }
    const QString message =
        tr("The following files already exist in the folder\n%1.\n"
           "Would you like to overwrite them?").arg(nativeCommonPath);
    m_label->setText(message);
}

QStandardItem *PromptOverwriteDialog::itemForFile(const QString &f) const
{
    const int rowCount = m_model->rowCount();
    for (int r = 0; r < rowCount; ++r) {
        QStandardItem *item = m_model->item(r, 0);
        if (fileNameOfItem(item) == f)
            return item;
    }
    return nullptr;
}

QStringList PromptOverwriteDialog::files(Qt::CheckState cs) const
{
    QStringList result;
    const int rowCount = m_model->rowCount();
    for (int r = 0; r < rowCount; ++r) {
        const QStandardItem *item = m_model->item(r, 0);
        if (item->checkState() == cs)
            result.push_back(fileNameOfItem(item));
    }
    return result;
}

void PromptOverwriteDialog::setFileEnabled(const QString &f, bool e)
{
    if (QStandardItem *item = itemForFile(f)) {
        Qt::ItemFlags flags = item->flags();
        if (e)
            flags |= Qt::ItemIsEnabled;
        else
            flags &= ~Qt::ItemIsEnabled;
        item->setFlags(flags);
    }
}

bool PromptOverwriteDialog::isFileEnabled(const QString &f) const
{
    if (const QStandardItem *item = itemForFile(f))
        return (item->flags() & Qt::ItemIsEnabled);
    return false;
}

void PromptOverwriteDialog::setFileChecked(const QString &f, bool e)
{
    if (QStandardItem *item = itemForFile(f))
        item->setCheckState(e ? Qt::Checked : Qt::Unchecked);
}

bool PromptOverwriteDialog::isFileChecked(const QString &f) const
{
    if (const QStandardItem *item = itemForFile(f))
        return item->checkState() == Qt::Checked;
    return false;
}

} // namespace Core
