// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "filterdialog.h"
#include "ui_filterdialog.h"

#include <utils/algorithm.h>
#include <utils/treemodel.h>

#include <QItemSelectionModel>

namespace ClangTools {
namespace Internal {

enum Columns { CheckName, Count };

class CheckItem : public Utils::TreeItem
{
public:
    CheckItem(const Check &check) : check(check) {}

    QVariant data(int column, int role) const override
    {
        if (role != Qt::DisplayRole)
            return {};
        switch (column) {
        case Columns::CheckName: return check.displayName;
        case Columns::Count: return check.count;
        }
        return {};
    }

    Check check;
};

static QItemSelectionModel::SelectionFlags selectionFlags()
{
    return QItemSelectionModel::Select | QItemSelectionModel::Rows;
}

class FilterChecksModel : public Utils::TreeModel<Utils::TreeItem, CheckItem>
{
    Q_OBJECT

public:
    FilterChecksModel(const Checks &checks)
    {
        Checks sortedChecks = checks;
        Utils::sort(sortedChecks, [](const Check &lhs, const Check &rhs) {
            return lhs.displayName < rhs.displayName;
        });

        setHeader({tr("Check"), "#"});
        setRootItem(new Utils::StaticTreeItem(QString()));
        for (const Check &check : qAsConst(sortedChecks))
            m_root->appendChild(new CheckItem(check));
    }
};

FilterDialog::FilterDialog(const Checks &checks, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::FilterDialog)
{
    m_ui->setupUi(this);

    m_model = new FilterChecksModel(checks);

    // View
    m_ui->view->setModel(m_model);
    m_ui->view->header()->setStretchLastSection(false);
    m_ui->view->header()->setSectionResizeMode(Columns::CheckName, QHeaderView::Stretch);
    m_ui->view->header()->setSectionResizeMode(Columns::Count, QHeaderView::ResizeToContents);
    m_ui->view->setSelectionMode(QAbstractItemView::MultiSelection);
    m_ui->view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->view->setIndentation(0);
    connect(m_ui->view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [&](){
        const bool hasSelection = !m_ui->view->selectionModel()->selectedRows().isEmpty();
        m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasSelection);
    });

    // Buttons
    connect(m_ui->selectNone, &QPushButton::clicked, m_ui->view, &QTreeView::clearSelection);
    connect(m_ui->selectAll, &QPushButton::clicked, m_ui->view, &QTreeView::selectAll);
    connect(m_ui->selectWithFixits, &QPushButton::clicked, m_ui->view, [this] {
        m_ui->view->clearSelection();
        m_model->forItemsAtLevel<1>([&](CheckItem *item) {
            if (item->check.hasFixit)
                m_ui->view->selectionModel()->select(item->index(), selectionFlags());
        });
    });
    m_ui->selectWithFixits->setEnabled(
        Utils::anyOf(checks, [](const Check &c) { return c.hasFixit; }));

    // Select checks that are not filtered out
    m_model->forItemsAtLevel<1>([this](CheckItem *item) {
        if (item->check.isShown)
            m_ui->view->selectionModel()->select(item->index(), selectionFlags());
    });
}

FilterDialog::~FilterDialog()
{
    delete m_ui;
}

QSet<QString> FilterDialog::selectedChecks() const
{
    QSet<QString> checks;
    m_model->forItemsAtLevel<1>([&](CheckItem *item) {
        if (m_ui->view->selectionModel()->isSelected(item->index()))
            checks << item->check.name;
    });
    return checks;
}

} // namespace Internal
} // namespace ClangTools

#include "filterdialog.moc"
