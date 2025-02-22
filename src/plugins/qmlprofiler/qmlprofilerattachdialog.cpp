// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qmlprofilerattachdialog.h"

#include <projectexplorer/kitchooser.h>
#include <projectexplorer/kitinformation.h>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

using namespace ProjectExplorer;

namespace QmlProfiler {
namespace Internal {

class QmlProfilerAttachDialogPrivate
{
public:
    QSpinBox *portSpinBox;
    KitChooser *kitChooser;
};

QmlProfilerAttachDialog::QmlProfilerAttachDialog(QWidget *parent) :
    QDialog(parent),
    d(new QmlProfilerAttachDialogPrivate)
{
    setWindowTitle(tr("Start QML Profiler"));

    d->kitChooser = new KitChooser(this);
    d->kitChooser->setKitPredicate([](const Kit *kit) {
        return DeviceKitAspect::device(kit) != nullptr;
    });
    d->kitChooser->populate();

    d->portSpinBox = new QSpinBox(this);
    d->portSpinBox->setMaximum(65535);
    d->portSpinBox->setValue(3768);

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

    auto hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setText(tr("Select an externally started QML-debug enabled application.<p>"
                     "Commonly used command-line arguments are:")
                  + "<p><tt>-qmljsdebugger=port:&lt;port&gt;,block,<br>"
                    "&nbsp;&nbsp;services:CanvasFrameRate,EngineControl,DebugMessages</tt>");

    auto formLayout = new QFormLayout;
    formLayout->addRow(tr("Kit:"), d->kitChooser);
    formLayout->addRow(tr("&Port:"), d->portSpinBox);

    auto verticalLayout = new QVBoxLayout(this);
    verticalLayout->addWidget(hint);
    verticalLayout->addLayout(formLayout);
    verticalLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QmlProfilerAttachDialog::~QmlProfilerAttachDialog()
{
    delete d;
}

int QmlProfilerAttachDialog::port() const
{
    return d->portSpinBox->value();
}

void QmlProfilerAttachDialog::setPort(const int port)
{
    d->portSpinBox->setValue(port);
}

Kit *QmlProfilerAttachDialog::kit() const
{
    return d->kitChooser->currentKit();
}

void QmlProfilerAttachDialog::setKitId(Utils::Id id)
{
    d->kitChooser->setCurrentKitId(id);
}

} // namespace Internal
} // namespace QmlProfiler
