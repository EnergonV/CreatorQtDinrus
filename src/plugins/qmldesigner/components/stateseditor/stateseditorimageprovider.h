// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include"abstractview.h"

#include <QQuickImageProvider>
#include <QPointer>

namespace QmlDesigner {
namespace Internal {

class StatesEditorView;

class StatesEditorImageProvider : public QQuickImageProvider
{
public:
    StatesEditorImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setNodeInstanceView(NodeInstanceView *nodeInstanceView);

private:
    QPointer<NodeInstanceView> m_nodeInstanceView;
};

} // namespace Internal
} // namespace QmlDesigner
