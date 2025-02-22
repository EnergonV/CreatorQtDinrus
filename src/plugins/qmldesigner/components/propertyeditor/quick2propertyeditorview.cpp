// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "quick2propertyeditorview.h"

#include "aligndistribute.h"
#include "annotationeditor/annotationeditor.h"
#include "bindingeditor/actioneditor.h"
#include "bindingeditor/bindingeditor.h"
#include "colorpalettebackend.h"
#include "fileresourcesmodel.h"
#include "gradientmodel.h"
#include "gradientpresetcustomlistmodel.h"
#include "gradientpresetdefaultlistmodel.h"
#include "itemfiltermodel.h"
#include "propertyeditorcontextobject.h"
#include "propertyeditorimageprovider.h"
#include "propertyeditorqmlbackend.h"
#include "propertyeditorvalue.h"
#include "qmlanchorbindingproxy.h"
#include "richtexteditor/richtexteditorproxy.h"
#include "theme.h"
#include "tooltip.h"

namespace QmlDesigner {

Quick2PropertyEditorView::Quick2PropertyEditorView(AsynchronousImageCache &imageCache)
    : QQuickWidget()
{
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    Theme::setupTheme(engine());
    engine()->addImageProvider("qmldesigner_thumbnails",
                               new PropertyEditorImageProvider(imageCache));
}

void Quick2PropertyEditorView::registerQmlTypes()
{
    static bool declarativeTypesRegistered = false;
    if (!declarativeTypesRegistered) {
        declarativeTypesRegistered = true;
        PropertyEditorValue::registerDeclarativeTypes();
        FileResourcesModel::registerDeclarativeType();
        GradientModel::registerDeclarativeType();
        GradientPresetDefaultListModel::registerDeclarativeType();
        GradientPresetCustomListModel::registerDeclarativeType();
        ItemFilterModel::registerDeclarativeType();
        ColorPaletteBackend::registerDeclarativeType();
        Internal::QmlAnchorBindingProxy::registerDeclarativeType();
        BindingEditor::registerDeclarativeType();
        ActionEditor::registerDeclarativeType();
        AnnotationEditor::registerDeclarativeType();
        AlignDistribute::registerDeclarativeType();
        Tooltip::registerDeclarativeType();
        EasingCurveEditor::registerDeclarativeType();
        RichTextEditorProxy::registerDeclarativeType();

        const QString resourcePath = PropertyEditorQmlBackend::propertyEditorResourcesPath();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QUrl regExpUrl = QUrl::fromLocalFile(resourcePath + "/RegExpValidator.qml");
        qmlRegisterType(regExpUrl, "HelperWidgets", 2, 0, "RegExpValidator");

        const QString qtPrefix = "/Qt6";
#else
        const QString qtPrefix = "/Qt5";
#endif
        qmlRegisterType(QUrl::fromLocalFile(resourcePath + qtPrefix + "HelperWindow.qml"),
                        "HelperWidgets",
                        2,
                        0,
                        "HelperWindow");
    }
}

} //QmlDesigner
