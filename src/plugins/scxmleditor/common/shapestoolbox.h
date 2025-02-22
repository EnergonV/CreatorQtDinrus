// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_shapestoolbox.h"

#include <QFrame>
#include <QPointer>

namespace ScxmlEditor {

namespace PluginInterface {
class ScxmlUiFactory;
class ShapeProvider;
} // namespace PluginInterface

namespace Common {

/**
 * @brief The ShapesToolbox class provides all draggable state-items.
 */
class ShapesToolbox : public QFrame
{
    Q_OBJECT

public:
    explicit ShapesToolbox(QWidget *parent = nullptr);

    void setUIFactory(PluginInterface::ScxmlUiFactory *uifactory);
    void initView();

private:
    QPointer<PluginInterface::ShapeProvider> m_shapeProvider;
    QList<QWidget*> m_widgets;
    Ui::ShapesToolbox m_ui;
};

} // namespace Common
} // namespace ScxmlEditor
