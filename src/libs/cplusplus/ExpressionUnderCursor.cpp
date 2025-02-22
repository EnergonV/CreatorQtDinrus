// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "ExpressionUnderCursor.h"

#include "SimpleLexer.h"
#include "BackwardsScanner.h"

#include <cplusplus/Token.h>

#include <QTextCursor>
#include <QTextBlock>

using namespace CPlusPlus;

ExpressionUnderCursor::ExpressionUnderCursor(const LanguageFeatures &languageFeatures)
    : _jumpedComma(false)
    , _languageFeatures(languageFeatures)
{ }

int ExpressionUnderCursor::startOfExpression(BackwardsScanner &tk, int index)
{
    if (tk[index - 1].is(T_GREATER)) {
        const int matchingBraceIndex = tk.startOfMatchingBrace(index);

        if (tk[matchingBraceIndex - 1].is(T_IDENTIFIER))
            index = matchingBraceIndex - 1;
    }

    index = startOfExpression_helper(tk, index);

    if (_jumpedComma) {
        const Token &tok = tk[index - 1];

        switch (tok.kind()) {
        case T_COMMA:
        case T_LPAREN:
        case T_LBRACKET:
        case T_LBRACE:
        case T_SEMICOLON:
        case T_COLON:
        case T_QUESTION:
            break;

        default:
            if (tok.isPunctuationOrOperator())
                return startOfExpression(tk, index - 1);

            break;
        }
    }

    return index;
}

int ExpressionUnderCursor::startOfExpression_helper(BackwardsScanner &tk, int index)
{
    if (tk[index - 1].isLiteral()) {
        return index - 1;
    } else if (tk[index - 1].is(T_THIS)) {
        return index - 1;
    } else if (tk[index - 1].is(T_TYPEID)) {
        return index - 1;
    } else if (tk[index - 1].is(T_SIGNAL)) {
        if (tk[index - 2].is(T_COMMA) && !_jumpedComma) {
            _jumpedComma = true;
            return startOfExpression(tk, index - 2);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_SLOT)) {
        if (tk[index - 2].is(T_COMMA) && !_jumpedComma) {
            _jumpedComma = true;
            return startOfExpression(tk, index - 2);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_IDENTIFIER)) {
        if (tk[index - 2].is(T_TILDE)) {
            if (tk[index - 3].is(T_COLON_COLON))
                return startOfExpression(tk, index - 3);
            else if (tk[index - 3].is(T_DOT) || tk[index - 3].is(T_ARROW))
                return startOfExpression(tk, index - 3);
            return index - 2;
        } else if (tk[index - 2].is(T_COLON_COLON)) {
            return startOfExpression(tk, index - 1);
        } else if (tk[index - 2].is(T_DOT) || tk[index - 2].is(T_ARROW)) {
            return startOfExpression(tk, index - 2);
        } else if (tk[index - 2].is(T_DOT_STAR) || tk[index - 2].is(T_ARROW_STAR)) {
            return startOfExpression(tk, index - 2);
        } else if (tk[index - 2].is(T_LBRACKET)) {
            // array subscription:
            //     array[i
            return index - 1;
        } else if (tk[index - 2].is(T_COLON)) {
            // either of:
            //     cond ? expr1 : id
            // or:
            //     [receiver messageParam:id
            // and in both cases, the id (and only the id) is what we want, so:
            return index - 1;
        } else if (tk[index - 2].is(T_IDENTIFIER) && tk[index - 3].is(T_LBRACKET)) {
            // Very common Objective-C case:
            //     [receiver message
            // which we handle immediately:
            return index - 3;
        } else {
#if 0 // see QTCREATORBUG-1501
            // See if we are handling an Objective-C messaging expression in the form of:
            //     [receiver messageParam1:expression messageParam2
            // or:
            //     [receiver messageParam1:expression messageParam2:expression messageParam3
            // ... etc
            int i = index - 1;
            while (i >= 0 && tk[i].isNot(T_EOF_SYMBOL)) {
                if (tk[i].is(T_LBRACKET))
                    break;
                if (tk[i].is(T_LBRACE) || tk[i].is(T_RBRACE))
                    break;
                else if (tk[i].is(T_RBRACKET))
                    i = tk.startOfMatchingBrace(i + 1) - 1;
                else
                    --i;
            }

            if (i >= 0) {
                int j = i;
                while (tk[j].is(T_LBRACKET))
                    ++j;
                if (tk[j].is(T_IDENTIFIER) && tk[j + 1].is(T_IDENTIFIER))
                    return i;
            }
#endif
        }
        return index - 1;
    } else if (tk[index - 1].is(T_RPAREN)) {
        int matchingBraceIndex = tk.startOfMatchingBrace(index);
        if (! matchingBraceIndex)
            return matchingBraceIndex;
        if (matchingBraceIndex != index) {
            if (tk[matchingBraceIndex - 1].is(T_GREATER)) {
                int lessIndex = tk.startOfMatchingBrace(matchingBraceIndex);
                if (lessIndex != matchingBraceIndex - 1) {
                    if (tk[lessIndex - 1].is(T_DYNAMIC_CAST)     ||
                        tk[lessIndex - 1].is(T_STATIC_CAST)      ||
                        tk[lessIndex - 1].is(T_CONST_CAST)       ||
                        tk[lessIndex - 1].is(T_REINTERPRET_CAST))
                        return lessIndex - 1;
                    else if (tk[lessIndex - 1].is(T_IDENTIFIER))
                        return startOfExpression(tk, lessIndex);
                    else if (tk[lessIndex - 1].is(T_SIGNAL))
                        return startOfExpression(tk, lessIndex);
                    else if (tk[lessIndex - 1].is(T_SLOT))
                        return startOfExpression(tk, lessIndex);
                }
            } else if (tk[matchingBraceIndex - 1].is(T_RBRACE)) {
                // lambda: [](){} ()
                int leftBraceIndex = tk.startOfMatchingBrace(matchingBraceIndex);
                if (matchingBraceIndex != leftBraceIndex) {
                    int currentIndex = leftBraceIndex;
                    while (currentIndex >= 0) {
                        if (tk[currentIndex-1].is(T_RPAREN)) {
                            int leftParenIndex = tk.startOfMatchingBrace(currentIndex);
                            if (tk[leftParenIndex-1].is(T_THROW)) {
                                currentIndex = leftParenIndex-1;
                                continue;
                            } else if (tk[leftParenIndex-1].is(T_RBRACKET)) {
                                int leftBracketIndex = tk.startOfMatchingBrace(leftParenIndex);
                                if (leftBracketIndex != leftParenIndex-1)
                                    return leftBracketIndex;
                            }
                        } else if (tk[currentIndex-1].is(T_RBRACKET)) {
                            int leftBracketIndex = tk.startOfMatchingBrace(currentIndex);
                            if (leftBracketIndex != currentIndex-1)
                                return leftBracketIndex;
                        }
                        --currentIndex;
                    }
                }
            }
            return startOfExpression(tk, matchingBraceIndex);
        }
        return index;
    } else if (tk[index - 1].is(T_RBRACKET)) {
        int rbracketIndex = tk.startOfMatchingBrace(index);
        if (rbracketIndex != index)
            return startOfExpression(tk, rbracketIndex);
        return index;
    } else if (tk[index - 1].is(T_COLON_COLON)) {
        if (tk[index - 2].is(T_GREATER)) { // ### not exactly
            int lessIndex = tk.startOfMatchingBrace(index - 1);
            if (lessIndex != index - 1)
                return startOfExpression(tk, lessIndex);
            return index - 1;
        } else if (tk[index - 2].is(T_IDENTIFIER)) {
            return startOfExpression(tk, index - 1);
        }
        return index - 1;
    } else if (tk[index - 1].is(T_DOT) || tk[index - 1].is(T_ARROW)) {
        return startOfExpression(tk, index - 1);
    } else if (tk[index - 1].is(T_DOT_STAR) || tk[index - 1].is(T_ARROW_STAR)) {
        return startOfExpression(tk, index - 1);
    }

    return index;
}

QString ExpressionUnderCursor::operator()(const QTextCursor &cursor)
{
    BackwardsScanner scanner(cursor, _languageFeatures);

    _jumpedComma = false;

    const int initialSize = scanner.startToken();
    const int i = startOfExpression(scanner, initialSize);
    if (i == initialSize)
        return QString();

    return scanner.mid(i);
}

int ExpressionUnderCursor::startOfFunctionCall(const QTextCursor &cursor) const
{
    BackwardsScanner scanner(cursor, _languageFeatures);

    int index = scanner.startToken();

    forever {
        const Token &tk = scanner[index - 1];

        if (tk.is(T_EOF_SYMBOL)) {
            break;
        } else if (tk.is(T_LPAREN) || tk.is(T_LBRACE)) {
            return scanner.startPosition() + tk.utf16charsBegin();
        } else if (tk.is(T_RPAREN) || tk.is(T_RBRACE)) {
            int matchingBrace = scanner.startOfMatchingBrace(index);

            if (matchingBrace == index) // If no matching brace found
                return -1;

            index = matchingBrace;
        } else {
            --index;
        }
    }

    return -1;
}
