// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qmljsquickfix.h"
#include "qmljscomponentfromobjectdef.h"
#include "qmljswrapinloader.h"
#include "qmljseditor.h"
#include "qmljsquickfixassist.h"

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>
#include <qmljs/parser/qmljsast_p.h>
#include <qmljstools/qmljsrefactoringchanges.h>
#include <texteditor/codeassist/assistinterface.h>

#include <QApplication>

using namespace QmlJS;
using namespace QmlJS::AST;
using namespace QmlJSTools;
using namespace TextEditor;

namespace QmlJSEditor {

using namespace Internal;

namespace {

/*
  Reformats a one-line object into a multi-line one, i.e.
    Item { x: 10; y: 20; width: 10 }
  into
    Item {
        x: 10;
        y: 20;
        width: 10
    }
*/
class SplitInitializerOperation: public QmlJSQuickFixOperation
{
    UiObjectInitializer *_objectInitializer;

public:
    SplitInitializerOperation(const QSharedPointer<const QmlJSQuickFixAssistInterface> &interface,
              UiObjectInitializer *objectInitializer)
        : QmlJSQuickFixOperation(interface, 0)
        , _objectInitializer(objectInitializer)
    {
        setDescription(QApplication::translate("QmlJSEditor::QuickFix",
                                               "Split Initializer"));
    }

    void performChanges(QmlJSRefactoringFilePtr currentFile,
                        const QmlJSRefactoringChanges &) override
    {
        Q_ASSERT(_objectInitializer);

        Utils::ChangeSet changes;

        for (UiObjectMemberList *it = _objectInitializer->members; it; it = it->next) {
            if (UiObjectMember *member = it->member) {
                const SourceLocation loc = member->firstSourceLocation();

                // insert a newline at the beginning of this binding
                changes.insert(currentFile->startOf(loc), QLatin1String("\n"));
            }
        }

        // insert a newline before the closing brace
        changes.insert(currentFile->startOf(_objectInitializer->rbraceToken),
                       QLatin1String("\n"));

        currentFile->setChangeSet(changes);
        currentFile->appendIndentRange(Range(currentFile->startOf(_objectInitializer->lbraceToken),
                                             currentFile->startOf(_objectInitializer->rbraceToken)));
        currentFile->apply();
    }
};

void matchSplitInitializerQuickFix(const QmlJSQuickFixInterface &interface, QuickFixOperations &result)
{
    UiObjectInitializer *objectInitializer = nullptr;

    const int pos = interface->currentFile()->cursor().position();

    if (Node *member = interface->semanticInfo().rangeAt(pos)) {
        if (auto b = AST::cast<const UiObjectBinding *>(member)) {
            if (b->initializer->lbraceToken.startLine == b->initializer->rbraceToken.startLine)
                objectInitializer = b->initializer;

        } else if (auto b = AST::cast<const UiObjectDefinition *>(member)) {
            if (b->initializer->lbraceToken.startLine == b->initializer->rbraceToken.startLine)
                objectInitializer = b->initializer;
        }
    }

    if (objectInitializer)
        result << new SplitInitializerOperation(interface, objectInitializer);
}

/*
  Adds a comment to suppress a static analysis message
*/
class AnalysizeMessageSuppressionOperation: public QmlJSQuickFixOperation
{
    StaticAnalysis::Message _message;

    Q_DECLARE_TR_FUNCTIONS(AddAnalysisMessageSuppressionComment)

public:
    AnalysizeMessageSuppressionOperation(const QSharedPointer<const QmlJSQuickFixAssistInterface> &interface,
                                         const StaticAnalysis::Message &message)
        : QmlJSQuickFixOperation(interface, 0)
        , _message(message)
    {
        setDescription(tr("Add a Comment to Suppress This Message"));
    }

    void performChanges(QmlJSRefactoringFilePtr currentFile,
                        const QmlJSRefactoringChanges &) override
    {
        Utils::ChangeSet changes;
        const int insertLoc = _message.location.begin() - _message.location.startColumn + 1;
        changes.insert(insertLoc, QString::fromLatin1("// %1\n").arg(_message.suppressionString()));
        currentFile->setChangeSet(changes);
        currentFile->appendIndentRange(Range(insertLoc, insertLoc + 1));
        currentFile->apply();
    }
};

void matchAddAnalysisMessageSuppressionCommentQuickFix(const QmlJSQuickFixInterface &interface, QuickFixOperations &result)
{
    const QList<StaticAnalysis::Message> &messages = interface->semanticInfo().staticAnalysisMessages;

    for (const StaticAnalysis::Message &message : messages) {
        if (interface->currentFile()->isCursorOn(message.location)) {
            result << new AnalysizeMessageSuppressionOperation(interface, message);
            return;
        }
    }
}

} // end of anonymous namespace

QuickFixOperations findQmlJSQuickFixes(const AssistInterface *interface)
{
    QSharedPointer<const AssistInterface> assistInterface(interface);
    auto qmlJSInterface = assistInterface.staticCast<const QmlJSQuickFixAssistInterface>();

    QuickFixOperations quickFixes;

    matchSplitInitializerQuickFix(qmlJSInterface, quickFixes);
    matchComponentFromObjectDefQuickFix(qmlJSInterface, quickFixes);
    matchWrapInLoaderQuickFix(qmlJSInterface, quickFixes);
    matchAddAnalysisMessageSuppressionCommentQuickFix(qmlJSInterface, quickFixes);

    return quickFixes;
}

} // namespace QmlJSEditor
