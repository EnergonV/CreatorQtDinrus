// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
#pragma once

#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QWidget>

#include <coreplugin/icontext.h>
#include <modelnode.h>

namespace QmlDesigner {

class Edit3DView;
class Edit3DCanvas;
class ToolBox;

class Edit3DWidget : public QWidget
{
    Q_OBJECT

public:
    Edit3DWidget(Edit3DView *view);

    Edit3DCanvas *canvas() const;
    Edit3DView *view() const;
    void contextHelp(const Core::IContext::HelpCallback &callback) const;

    void showCanvas(bool show);
    QMenu *visibilityTogglesMenu() const;
    void showVisibilityTogglesMenu(bool show, const QPoint &pos);

    QMenu *backgroundColorMenu() const;
    void showBackgroundColorMenu(bool show, const QPoint &pos);

    void showContextMenu(const QPoint &pos, const ModelNode &modelNode);

protected:
    void dragEnterEvent(QDragEnterEvent *dragEnterEvent) override;
    void dropEvent(QDropEvent *dropEvent) override;

private:
    void linkActivated(const QString &link);
    void createContextMenu();

    QPointer<Edit3DView> m_edit3DView;
    QPointer<Edit3DView> m_view;
    QPointer<Edit3DCanvas> m_canvas;
    QPointer<QLabel> m_onboardingLabel;
    QPointer<ToolBox> m_toolBox;
    Core::IContext *m_context = nullptr;
    QPointer<QMenu> m_visibilityTogglesMenu;
    QPointer<QMenu> m_backgroundColorMenu;
    QPointer<QMenu> m_contextMenu;
    QPointer<QAction> m_editMaterialAction;
    QPointer<QAction> m_deleteAction;
    ModelNode m_contextMenuTarget;
};

} // namespace QmlDesigner
