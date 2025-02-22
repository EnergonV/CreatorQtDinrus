// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "newclasswidget.h"
#include "ui_newclasswidget.h"

#include <QFileDialog>
#include <QDebug>

using namespace Utils;

enum { debugNewClassWidget = 0 };

/*! \class Utils::NewClassWidget

    \brief The NewClassWidget class is a utility widget for 'New Class' wizards.

    This widget prompts the user
    to enter a class name (optionally derived from some base class) and file
    names for header, source and form files. Has some smart logic to derive
    the file names from the class name. */

namespace Designer {
namespace Internal {

struct NewClassWidgetPrivate {
    NewClassWidgetPrivate();

    Ui::NewClassWidget m_ui;
    QString m_headerExtension;
    QString m_sourceExtension;
    QString m_formExtension;
    bool m_valid = false;
    bool m_classEdited = false;
};

NewClassWidgetPrivate:: NewClassWidgetPrivate() :
    m_headerExtension(QLatin1Char('h')),
    m_sourceExtension(QLatin1String("cpp")),
    m_formExtension(QLatin1String("ui"))
{
}

// --------------------- NewClassWidget
NewClassWidget::NewClassWidget(QWidget *parent) :
    QWidget(parent),
    d(new NewClassWidgetPrivate)
{
    d->m_ui.setupUi(this);

    d->m_ui.baseClassLabel->setVisible(false);
    d->m_ui.baseClassComboBox->setVisible(false);
    d->m_ui.classTypeLabel->setVisible(false);
    d->m_ui.classTypeComboBox->setVisible(false);

    d->m_ui.classLineEdit->setNamespacesEnabled(true);
    setNamesDelimiter(QLatin1String("::"));

    connect(d->m_ui.classLineEdit, &Utils::ClassNameValidatingLineEdit::updateFileName,
            this, &NewClassWidget::slotUpdateFileNames);
    connect(d->m_ui.classLineEdit, &QLineEdit::textEdited,
            this, &NewClassWidget::classNameEdited);
    connect(d->m_ui.baseClassComboBox, &QComboBox::currentIndexChanged,
            this, &NewClassWidget::suggestClassNameFromBase);
    connect(d->m_ui.baseClassComboBox, &QComboBox::editTextChanged,
            this, &NewClassWidget::slotValidChanged);
    connect(d->m_ui.classLineEdit, &Utils::FancyLineEdit::validChanged,
            this, &NewClassWidget::slotValidChanged);
    connect(d->m_ui.headerFileLineEdit, &Utils::FancyLineEdit::validChanged,
            this, &NewClassWidget::slotValidChanged);
    connect(d->m_ui.sourceFileLineEdit, &Utils::FancyLineEdit::validChanged,
            this, &NewClassWidget::slotValidChanged);
    connect(d->m_ui.formFileLineEdit, &Utils::FancyLineEdit::validChanged,
            this, &NewClassWidget::slotValidChanged);
    connect(d->m_ui.pathChooser, &Utils::PathChooser::validChanged,
            this, &NewClassWidget::slotValidChanged);

    connect(d->m_ui.classLineEdit, &Utils::FancyLineEdit::validReturnPressed,
            this, &NewClassWidget::slotActivated);
    connect(d->m_ui.headerFileLineEdit, &Utils::FancyLineEdit::validReturnPressed,
            this, &NewClassWidget::slotActivated);
    connect(d->m_ui.sourceFileLineEdit, &Utils::FancyLineEdit::validReturnPressed,
            this, &NewClassWidget::slotActivated);
    connect(d->m_ui.formFileLineEdit, &Utils::FancyLineEdit::validReturnPressed,
            this, &NewClassWidget::slotActivated);
    connect(d->m_ui.formFileLineEdit, &Utils::FancyLineEdit::validReturnPressed,
            this, &NewClassWidget::slotActivated);
    connect(d->m_ui.pathChooser, &Utils::PathChooser::returnPressed,
             this, &NewClassWidget::slotActivated);

    setClassType(NoClassType);
}

NewClassWidget::~NewClassWidget()
{
    delete d;
}

void NewClassWidget::classNameEdited()
{
    if (debugNewClassWidget)
        qDebug() << Q_FUNC_INFO << d->m_headerExtension << d->m_sourceExtension;
    d->m_classEdited = true;
}

void NewClassWidget::suggestClassNameFromBase()
{
    if (debugNewClassWidget)
        qDebug() << Q_FUNC_INFO << d->m_headerExtension << d->m_sourceExtension;
    if (d->m_classEdited)
        return;
    // Suggest a class unless edited ("QMainWindow"->"MainWindow")
    QString base = baseClassName();
    if (base.startsWith(QLatin1Char('Q'))) {
        base.remove(0, 1);
        setClassName(base);
    }
}

void NewClassWidget::setClassName(const QString &suggestedName)
{
    if (debugNewClassWidget)
        qDebug() << Q_FUNC_INFO << suggestedName << d->m_headerExtension << d->m_sourceExtension;
    d->m_ui.classLineEdit->setText(
                Utils::ClassNameValidatingLineEdit::createClassName(suggestedName));
}

QString NewClassWidget::className() const
{
    return d->m_ui.classLineEdit->text();
}

QString NewClassWidget::baseClassName() const
{
    return d->m_ui.baseClassComboBox->currentText();
}

QString NewClassWidget::sourceFileName() const
{
    return d->m_ui.sourceFileLineEdit->text();
}

QString NewClassWidget::headerFileName() const
{
    return d->m_ui.headerFileLineEdit->text();
}

QString NewClassWidget::formFileName() const
{
    return d->m_ui.formFileLineEdit->text();
}

FilePath NewClassWidget::filePath() const
{
    return d->m_ui.pathChooser->filePath();
}

void NewClassWidget::setFilePath(const FilePath &path)
{
     d->m_ui.pathChooser->setFilePath(path);
}

QString NewClassWidget::sourceExtension() const
{
    return d->m_sourceExtension;
}

void NewClassWidget::setSourceExtension(const QString &e)
{
    if (debugNewClassWidget)
        qDebug() << Q_FUNC_INFO << e;
    d->m_sourceExtension = fixSuffix(e);
}

QString NewClassWidget::headerExtension() const
{
    return d->m_headerExtension;
}

void NewClassWidget::setHeaderExtension(const QString &e)
{
    if (debugNewClassWidget)
        qDebug() << Q_FUNC_INFO << e;
    d->m_headerExtension = fixSuffix(e);
}

QString NewClassWidget::formExtension() const
{
    return d->m_formExtension;
}


void NewClassWidget::setLowerCaseFiles(bool v)
{
    d->m_ui.classLineEdit->setLowerCaseFileName(v);
}

void NewClassWidget::setClassType(ClassType ct)
{
    d->m_ui.classTypeComboBox->setCurrentIndex(ct);
}

void NewClassWidget::setNamesDelimiter(const QString &delimiter)
{
    d->m_ui.classLineEdit->setNamespaceDelimiter(delimiter);
}

void NewClassWidget::slotValidChanged()
{
    const bool newValid = isValid();
    if (newValid != d->m_valid) {
        d->m_valid = newValid;
        emit validChanged();
    }
}

bool NewClassWidget::isValid(QString *error) const
{
    if (!d->m_ui.classLineEdit->isValid()) {
        if (error)
            *error = d->m_ui.classLineEdit->errorMessage();
        return false;
    }

    if (!d->m_ui.headerFileLineEdit->isValid()) {
        if (error)
            *error = tr("Invalid header file name: \"%1\"").arg(d->m_ui.headerFileLineEdit->errorMessage());
        return false;
    }

    if (!d->m_ui.sourceFileLineEdit->isValid()) {
        if (error)
            *error = tr("Invalid source file name: \"%1\"").arg(d->m_ui.sourceFileLineEdit->errorMessage());
        return false;
    }

    if (!d->m_ui.formFileLineEdit->isValid()) {
        if (error)
            *error = tr("Invalid form file name: \"%1\"").arg(d->m_ui.formFileLineEdit->errorMessage());
        return false;
    }

    if (!d->m_ui.pathChooser->isValid()) {
        if (error)
            *error =  d->m_ui.pathChooser->errorMessage();
        return false;
    }
    return true;
}

void NewClassWidget::slotUpdateFileNames(const QString &baseName)
{
    if (debugNewClassWidget)
        qDebug() << Q_FUNC_INFO << baseName << d->m_headerExtension << d->m_sourceExtension;
    const QChar dot = QLatin1Char('.');
    d->m_ui.sourceFileLineEdit->setText(baseName + dot + d->m_sourceExtension);
    d->m_ui.headerFileLineEdit->setText(baseName + dot + d->m_headerExtension);
    d->m_ui.formFileLineEdit->setText(baseName + dot + d->m_formExtension);
}

void NewClassWidget::slotActivated()
{
    if (d->m_valid)
        emit activated();
}

QString NewClassWidget::fixSuffix(const QString &suffix)
{
    QString s = suffix;
    if (s.startsWith(QLatin1Char('.')))
        s.remove(0, 1);
    return s;
}

// Utility to add a suffix to a file unless the user specified one
static QString ensureSuffix(QString f, const QString &extension)
{
    const QChar dot = QLatin1Char('.');
    if (f.contains(dot))
        return f;
    f += dot;
    f += extension;
    return f;
}

// If a non-empty name was passed, expand to directory and suffix
static FilePath expandFileName(const FilePath &dir, const QString &name, const QString &extension)
{
    if (name.isEmpty())
        return {};
    return dir / ensureSuffix(name, extension);
}

Utils::FilePaths NewClassWidget::files() const
{
    const FilePath dir = filePath();
    return {
        expandFileName(dir, headerFileName(), headerExtension()),
        expandFileName(dir, sourceFileName(), sourceExtension()),
        expandFileName(dir, formFileName(), formExtension()),
    };
}

} // namespace Internal
} // namespace Designer
