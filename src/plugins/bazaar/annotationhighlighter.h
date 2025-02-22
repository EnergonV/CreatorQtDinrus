// Copyright (C) 2016 Hugues Delorme
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <vcsbase/baseannotationhighlighter.h>
#include <QRegularExpression>

namespace Bazaar {
namespace Internal {

class BazaarAnnotationHighlighter : public VcsBase::BaseAnnotationHighlighter
{
public:
    explicit BazaarAnnotationHighlighter(const ChangeNumbers &changeNumbers,
                                         QTextDocument *document = nullptr);

private:
    QString changeNumber(const QString &block) const override;
    const QRegularExpression m_changeset;
};

} // namespace Internal
} // namespace Bazaar
