// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "findtoolwindow.h"
#include "ifindfilter.h"
#include "findplugin.h"

#include <coreplugin/icore.h>
#include <utils/qtcassert.h>
#include <utils/algorithm.h>

#include <QSettings>
#include <QStringListModel>
#include <QCompleter>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollArea>

using namespace Core;
using namespace Core::Internal;

static FindToolWindow *m_instance = nullptr;

static bool validateRegExp(Utils::FancyLineEdit *edit, QString *errorMessage)
{
    if (edit->text().isEmpty()) {
        if (errorMessage)
            *errorMessage = FindToolWindow::tr("Empty search term.");
        return false;
    }
    if (Find::hasFindFlag(FindRegularExpression)) {
        QRegularExpression regexp(edit->text());
        bool regexpValid = regexp.isValid();
        if (!regexpValid && errorMessage)
            *errorMessage = regexp.errorString();
        return regexpValid;
    }
    return true;
}

FindToolWindow::FindToolWindow(QWidget *parent)
    : QWidget(parent),
    m_findCompleter(new QCompleter(this)),
    m_currentFilter(nullptr),
    m_configWidget(nullptr)
{
    m_instance = this;
    m_ui.setupUi(this);
    m_ui.searchTerm->setFiltering(true);
    m_ui.searchTerm->setPlaceholderText(QString());
    setFocusProxy(m_ui.searchTerm);

    connect(m_ui.searchButton, &QAbstractButton::clicked, this, &FindToolWindow::search);
    connect(m_ui.replaceButton, &QAbstractButton::clicked, this, &FindToolWindow::replace);
    connect(m_ui.matchCase, &QAbstractButton::toggled, Find::instance(), &Find::setCaseSensitive);
    connect(m_ui.wholeWords, &QAbstractButton::toggled, Find::instance(), &Find::setWholeWord);
    connect(m_ui.regExp, &QAbstractButton::toggled, Find::instance(), &Find::setRegularExpression);
    connect(m_ui.filterList, &QComboBox::activated,
            this, QOverload<int>::of(&FindToolWindow::setCurrentFilter));

    m_findCompleter->setModel(Find::findCompletionModel());
    m_ui.searchTerm->setSpecialCompleter(m_findCompleter);
    m_ui.searchTerm->installEventFilter(this);
    connect(m_findCompleter, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, &FindToolWindow::findCompleterActivated);

    m_ui.searchTerm->setValidationFunction(validateRegExp);
    connect(Find::instance(), &Find::findFlagsChanged,
            m_ui.searchTerm, &Utils::FancyLineEdit::validate);
    connect(m_ui.searchTerm, &Utils::FancyLineEdit::validChanged,
            this, &FindToolWindow::updateButtonStates);

    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_ui.configWidget->setLayout(layout);
    updateButtonStates();

    connect(Find::instance(), &Find::findFlagsChanged, this, &FindToolWindow::updateFindFlags);
}

FindToolWindow::~FindToolWindow()
{
    qDeleteAll(m_configWidgets);
}

FindToolWindow *FindToolWindow::instance()
{
    return m_instance;
}

bool FindToolWindow::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto ke = static_cast<QKeyEvent *>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
                && (ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::KeypadModifier)) {
            ke->accept();
            if (m_ui.searchButton->isEnabled())
                search();
            return true;
        }
    }
    return QWidget::event(event);
}

bool FindToolWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_ui.searchTerm && event->type() == QEvent::KeyPress) {
        auto ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Down) {
            if (m_ui.searchTerm->text().isEmpty())
                m_findCompleter->setCompletionPrefix(QString());
            m_findCompleter->complete();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void FindToolWindow::updateButtonStates()
{
    bool filterEnabled = m_currentFilter && m_currentFilter->isEnabled();
    bool enabled = filterEnabled && (!m_currentFilter->showSearchTermInput()
                                     || m_ui.searchTerm->isValid()) && m_currentFilter->isValid();
    m_ui.searchButton->setEnabled(enabled);
    m_ui.replaceButton->setEnabled(m_currentFilter
                                   && m_currentFilter->isReplaceSupported() && enabled);
    if (m_configWidget)
        m_configWidget->setEnabled(filterEnabled);

    if (m_currentFilter) {
        m_ui.searchTerm->setVisible(m_currentFilter->showSearchTermInput());
        m_ui.searchLabel->setVisible(m_currentFilter->showSearchTermInput());
        m_ui.optionsWidget->setVisible(m_currentFilter->supportedFindFlags()
                                       & (FindCaseSensitively | FindWholeWords | FindRegularExpression));
    }

    m_ui.matchCase->setEnabled(filterEnabled
                               && (m_currentFilter->supportedFindFlags() & FindCaseSensitively));
    m_ui.wholeWords->setEnabled(filterEnabled
                                && (m_currentFilter->supportedFindFlags() & FindWholeWords));
    m_ui.regExp->setEnabled(filterEnabled
                            && (m_currentFilter->supportedFindFlags() & FindRegularExpression));
    m_ui.searchTerm->setEnabled(filterEnabled);
}

void FindToolWindow::updateFindFlags()
{
    m_ui.matchCase->setChecked(Find::hasFindFlag(FindCaseSensitively));
    m_ui.wholeWords->setChecked(Find::hasFindFlag(FindWholeWords));
    m_ui.regExp->setChecked(Find::hasFindFlag(FindRegularExpression));
}


void FindToolWindow::setFindFilters(const QList<IFindFilter *> &filters)
{
    qDeleteAll(m_configWidgets);
    m_configWidgets.clear();
    for (IFindFilter *filter : qAsConst(m_filters))
        filter->disconnect(this);
    m_filters = filters;
    m_ui.filterList->clear();
    QStringList names;
    for (IFindFilter *filter : filters) {
        names << filter->displayName();
        m_configWidgets.append(filter->createConfigWidget());
        connect(filter, &IFindFilter::displayNameChanged,
                this, [this, filter]() { updateFindFilterName(filter); });
    }
    m_ui.filterList->addItems(names);
    if (m_filters.size() > 0)
        setCurrentFilter(0);
}

QList<IFindFilter *> FindToolWindow::findFilters() const
{
    return m_filters;
}

void FindToolWindow::updateFindFilterName(IFindFilter *filter)
{
    int index = m_filters.indexOf(filter);
    if (QTC_GUARD(index >= 0))
        m_ui.filterList->setItemText(index, filter->displayName());
}

void FindToolWindow::setFindText(const QString &text)
{
    m_ui.searchTerm->setText(text);
}

void FindToolWindow::setCurrentFilter(IFindFilter *filter)
{
    if (!filter)
        filter = m_currentFilter;
    int index = m_filters.indexOf(filter);
    if (index >= 0)
        setCurrentFilter(index);
    updateFindFlags();
    m_ui.searchTerm->setFocus();
    m_ui.searchTerm->selectAll();
}

void FindToolWindow::setCurrentFilter(int index)
{
    m_ui.filterList->setCurrentIndex(index);
    for (int i = 0; i < m_configWidgets.size(); ++i) {
        QWidget *configWidget = m_configWidgets.at(i);
        if (i == index) {
            m_configWidget = configWidget;
            if (m_currentFilter) {
                disconnect(m_currentFilter, &IFindFilter::enabledChanged,
                           this, &FindToolWindow::updateButtonStates);
                disconnect(m_currentFilter, &IFindFilter::validChanged,
                           this, &FindToolWindow::updateButtonStates);
            }
            m_currentFilter = m_filters.at(i);
            connect(m_currentFilter, &IFindFilter::enabledChanged,
                    this, &FindToolWindow::updateButtonStates);
            connect(m_currentFilter, &IFindFilter::validChanged,
                    this, &FindToolWindow::updateButtonStates);
            updateButtonStates();
            if (m_configWidget)
                m_ui.configWidget->layout()->addWidget(m_configWidget);
        } else {
            if (configWidget)
                configWidget->setParent(nullptr);
        }
    }
    QWidget *w = m_ui.configWidget;
    while (w) {
        auto sa = qobject_cast<QScrollArea *>(w);
        if (sa) {
            sa->updateGeometry();
            break;
        }
        w = w->parentWidget();
    }
    for (w = m_configWidget ? m_configWidget : m_ui.configWidget; w; w = w->parentWidget()) {
        if (w->layout())
            w->layout()->activate();
    }
}

void FindToolWindow::acceptAndGetParameters(QString *term, IFindFilter **filter)
{
    QTC_ASSERT(filter, return);
    *filter = nullptr;
    Find::updateFindCompletion(m_ui.searchTerm->text(), Find::findFlags());
    int index = m_ui.filterList->currentIndex();
    QString searchTerm = m_ui.searchTerm->text();
    if (index >= 0)
        *filter = m_filters.at(index);
    if (term)
        *term = searchTerm;
    if (searchTerm.isEmpty() && *filter && !(*filter)->isValid())
        *filter = nullptr;
}

void FindToolWindow::search()
{
    QString term;
    IFindFilter *filter = nullptr;
    acceptAndGetParameters(&term, &filter);
    QTC_ASSERT(filter, return);
    filter->findAll(term, Find::findFlags());
}

void FindToolWindow::replace()
{
    QString term;
    IFindFilter *filter = nullptr;
    acceptAndGetParameters(&term, &filter);
    QTC_ASSERT(filter, return);
    filter->replaceAll(term, Find::findFlags());
}

void FindToolWindow::writeSettings()
{
    Utils::QtcSettings *settings = ICore::settings();
    settings->beginGroup("Find");
    settings->setValueWithDefault("CurrentFilter",
                                  m_currentFilter ? m_currentFilter->id() : QString());
    for (IFindFilter *filter : qAsConst(m_filters))
        filter->writeSettings(settings);
    settings->endGroup();
}

void FindToolWindow::readSettings()
{
    QSettings *settings = ICore::settings();
    settings->beginGroup(QLatin1String("Find"));
    const QString currentFilter = settings->value(QLatin1String("CurrentFilter")).toString();
    for (int i = 0; i < m_filters.size(); ++i) {
        IFindFilter *filter = m_filters.at(i);
        filter->readSettings(settings);
        if (filter->id() == currentFilter)
            setCurrentFilter(i);
    }
    settings->endGroup();
}

void FindToolWindow::findCompleterActivated(const QModelIndex &index)
{
    const int findFlagsI = index.data(Find::CompletionModelFindFlagsRole).toInt();
    const FindFlags findFlags(findFlagsI);
    Find::setCaseSensitive(findFlags.testFlag(FindCaseSensitively));
    Find::setBackward(findFlags.testFlag(FindBackward));
    Find::setWholeWord(findFlags.testFlag(FindWholeWords));
    Find::setRegularExpression(findFlags.testFlag(FindRegularExpression));
    Find::setPreserveCase(findFlags.testFlag(FindPreserveCase));
}
