// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
#pragma once

#include <QMenu>

namespace QmlDesigner {

class Edit3DVisibilityTogglesMenu : public QMenu
{
    Q_OBJECT

public:
    explicit Edit3DVisibilityTogglesMenu(QWidget *parent = nullptr);

protected:
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
};

} // namespace QmlDesigner
