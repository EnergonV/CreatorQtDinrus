// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "moveobjectbeforeobjectvisitor.h"

#include <qmljs/parser/qmljsast_p.h>

using namespace QmlDesigner::Internal;
using namespace QmlDesigner;

MoveObjectBeforeObjectVisitor::MoveObjectBeforeObjectVisitor(TextModifier &modifier,
                                                             quint32 movingObjectLocation,
                                                             bool inDefaultProperty):
    QMLRewriter(modifier),
    movingObjectLocation(movingObjectLocation),
    inDefaultProperty(inDefaultProperty),
    toEnd(true),
    beforeObjectLocation(0)
{}

MoveObjectBeforeObjectVisitor::MoveObjectBeforeObjectVisitor(TextModifier &modifier,
                                                             quint32 movingObjectLocation,
                                                             quint32 beforeObjectLocation,
                                                             bool inDefaultProperty):
    QMLRewriter(modifier),
    movingObjectLocation(movingObjectLocation),
    inDefaultProperty(inDefaultProperty),
    toEnd(false),
    beforeObjectLocation(beforeObjectLocation)
{}

bool MoveObjectBeforeObjectVisitor::operator ()(QmlJS::AST::UiProgram *ast)
{
    movingObject = nullptr;
    beforeObject = nullptr;
    movingObjectParents.clear();

    QMLRewriter::operator ()(ast);

    if (foundEverything())
        doMove();

    return didRewriting();
}

bool MoveObjectBeforeObjectVisitor::preVisit(QmlJS::AST::Node *ast)
{ if (ast) parents.push(ast); return true; }

void MoveObjectBeforeObjectVisitor::postVisit(QmlJS::AST::Node *ast)
{ if (ast) parents.pop(); }

bool MoveObjectBeforeObjectVisitor::visit(QmlJS::AST::UiObjectDefinition *ast)
{
    if (foundEverything())
        return false;

    const quint32 start = ast->firstSourceLocation().offset;
    if (start == movingObjectLocation) {
        movingObject = ast;
        movingObjectParents = parents;
        movingObjectParents.pop();
    } else if (!toEnd && start == beforeObjectLocation) {
        beforeObject = ast;
    }

    if (movingObjectLocation < start)
        return false;
    else if (!toEnd && beforeObjectLocation < start)
        return false;
    else if (foundEverything())
        return false;
    else
        return true;
}

void MoveObjectBeforeObjectVisitor::doMove()
{
    Q_ASSERT(movingObject);
    Q_ASSERT(!movingObjectParents.isEmpty());

    TextModifier::MoveInfo moveInfo;
    QmlJS::AST::Node *parent = movingObjectParent();
    QmlJS::AST::UiArrayMemberList *arrayMember = nullptr, *otherArrayMember = nullptr;
    QString separator;

    if (!inDefaultProperty) {
        auto initializer = QmlJS::AST::cast<QmlJS::AST::UiArrayBinding*>(parent);
        Q_ASSERT(initializer);

        otherArrayMember = nullptr;
        for (QmlJS::AST::UiArrayMemberList *cur = initializer->members; cur; cur = cur->next) {
            if (cur->member == movingObject) {
                arrayMember = cur;
                if (cur->next)
                    otherArrayMember = cur->next;
                break;
            }
            otherArrayMember = cur;
        }
        Q_ASSERT(arrayMember && otherArrayMember);
        separator = QStringLiteral(",");
    }

    moveInfo.objectStart = movingObject->firstSourceLocation().offset;
    moveInfo.objectEnd = movingObject->lastSourceLocation().end();

    int start = moveInfo.objectStart;
    int end = moveInfo.objectEnd;
    if (!inDefaultProperty) {
        if (arrayMember->commaToken.isValid())
            start = arrayMember->commaToken.begin();
        else
            end = otherArrayMember->commaToken.end();
    }

    includeSurroundingWhitespace(start, end);
    moveInfo.leadingCharsToRemove = moveInfo.objectStart - start;
    moveInfo.trailingCharsToRemove = end - moveInfo.objectEnd;

    if (beforeObject) {
        moveInfo.destination = beforeObject->firstSourceLocation().offset;
        int dummy = -1;
        includeSurroundingWhitespace(moveInfo.destination, dummy);

        moveInfo.prefixToInsert = QString(moveInfo.leadingCharsToRemove, QLatin1Char(' '));
        moveInfo.suffixToInsert = separator + QStringLiteral("\n\n");
    } else {
        const QmlJS::SourceLocation insertionPoint = lastParentLocation();
        Q_ASSERT(insertionPoint.isValid());
        moveInfo.destination = insertionPoint.offset;
        int dummy = -1;
        includeSurroundingWhitespace(moveInfo.destination, dummy);

        moveInfo.prefixToInsert = separator + QString(moveInfo.leadingCharsToRemove, QLatin1Char(' '));
        moveInfo.suffixToInsert = QStringLiteral("\n");
    }

    move(moveInfo);
    setDidRewriting(true);
}

QmlJS::AST::Node *MoveObjectBeforeObjectVisitor::movingObjectParent() const
{
    if (movingObjectParents.size() > 1)
        return movingObjectParents.at(movingObjectParents.size() - 2);
    else
        return nullptr;
}

QmlJS::SourceLocation MoveObjectBeforeObjectVisitor::lastParentLocation() const
{
    dump(movingObjectParents);

    QmlJS::AST::Node *parent = movingObjectParent();
    if (auto initializer = QmlJS::AST::cast<QmlJS::AST::UiObjectInitializer*>(parent))
        return initializer->rbraceToken;
    else if (auto initializer = QmlJS::AST::cast<QmlJS::AST::UiArrayBinding*>(parent))
        return initializer->rbracketToken;
    else
        return QmlJS::SourceLocation();
}
