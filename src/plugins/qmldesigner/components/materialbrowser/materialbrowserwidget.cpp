// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "materialbrowserwidget.h"
#include "materialbrowsermodel.h"
#include "materialbrowserview.h"

#include <theme.h>

#include <designeractionmanager.h>
#include <designermcumanager.h>
#include <documentmanager.h>
#include <qmldesignerconstants.h>
#include <qmldesignerplugin.h>

#include <utils/algorithm.h>
#include <utils/stylehelper.h>
#include <utils/qtcassert.h>

#include <QImageReader>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace QmlDesigner {

static QString propertyEditorResourcesPath()
{
#ifdef SHARE_QML_PATH
    if (qEnvironmentVariableIsSet("LOAD_QML_FROM_SOURCE"))
        return QLatin1String(SHARE_QML_PATH) + "/propertyEditorQmlSources";
#endif
    return Core::ICore::resourcePath("qmldesigner/propertyEditorQmlSources").toString();
}

class PreviewImageProvider : public QQuickImageProvider
{
    QHash<qint32, QPixmap> m_pixmaps;

public:
    PreviewImageProvider()
        : QQuickImageProvider(Pixmap) {}

    void setPixmap(const ModelNode &node, const QPixmap &pixmap)
    {
        m_pixmaps.insert(node.internalId(), pixmap);
    }

    QPixmap requestPixmap(const QString &id,
                          QSize *size,
                          [[maybe_unused]] const QSize &requestedSize) override
    {
        static QPixmap defaultPreview = QPixmap::fromImage(QImage(":/materialeditor/images/defaultmaterialpreview.png"));

        QPixmap pixmap{150, 150};

        qint32 internalId = id.toInt();
        if (m_pixmaps.contains(internalId))
            pixmap = m_pixmaps.value(internalId);
        else
            pixmap = defaultPreview;

        if (size)
            *size = pixmap.size();

        return pixmap;
    }
};

bool MaterialBrowserWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FocusOut) {
        if (obj == m_quickWidget.data())
            QMetaObject::invokeMethod(m_quickWidget->rootObject(), "closeContextMenu");
    } else if (event->type() == QMouseEvent::MouseMove) {
        DesignDocument *document = QmlDesignerPlugin::instance()->currentDesignDocument();
        QTC_ASSERT(document, return false);
        Model *model = document->currentModel();
        QTC_ASSERT(model, return false);

        if (m_materialToDrag.isValid()) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if ((me->globalPos() - m_dragStartPoint).manhattanLength() > 10) {
                QByteArray data;
                QMimeData *mimeData = new QMimeData;
                QDataStream stream(&data, QIODevice::WriteOnly);
                stream << m_materialToDrag.internalId();
                mimeData->setData(Constants::MIME_TYPE_MATERIAL, data);
                mimeData->removeFormat("text/plain");

                model->startDrag(mimeData, m_previewImageProvider->requestPixmap(
                                 QString::number(m_materialToDrag.internalId()), nullptr, {128, 128}));
                m_materialToDrag = {};
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

MaterialBrowserWidget::MaterialBrowserWidget(MaterialBrowserView *view)
    : m_materialBrowserView(view)
    , m_materialBrowserModel(new MaterialBrowserModel(this))
    , m_quickWidget(new QQuickWidget(this))
    , m_previewImageProvider(new PreviewImageProvider())
{
    setWindowTitle(tr("Material Browser", "Title of material browser widget"));
    setMinimumWidth(120);

    Core::Context context(Constants::C_QMLMATERIALBROWSER);
    m_context = new Core::IContext(this);
    m_context->setContext(context);
    m_context->setWidget(this);

    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->engine()->addImportPath(propertyEditorResourcesPath() + "/imports");
    m_quickWidget->setClearColor(Theme::getColor(Theme::Color::DSpanelBackground));

    m_quickWidget->rootContext()->setContextProperties({
        {"rootView", QVariant::fromValue(this)},
        {"materialBrowserModel", QVariant::fromValue(m_materialBrowserModel.data())},
    });

    m_quickWidget->engine()->addImageProvider("materialBrowser", m_previewImageProvider);
    Theme::setupTheme(m_quickWidget->engine());
    m_quickWidget->installEventFilter(this);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);
    layout->addWidget(m_quickWidget.data());

    updateSearch();

    setStyleSheet(Theme::replaceCssColors(
        QString::fromUtf8(Utils::FileReader::fetchQrc(":/qmldesigner/stylesheet.css"))));

    m_qmlSourceUpdateShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F8), this);
    connect(m_qmlSourceUpdateShortcut, &QShortcut::activated, this, &MaterialBrowserWidget::reloadQmlSource);

    QmlDesignerPlugin::trackWidgetFocusTime(this, Constants::EVENT_MATERIALBROWSER_TIME);

    reloadQmlSource();
}

void MaterialBrowserWidget::updateMaterialPreview(const ModelNode &node, const QPixmap &pixmap)
{
    m_previewImageProvider->setPixmap(node, pixmap);
    int idx = m_materialBrowserModel->materialIndex(node);
    if (idx != -1)
        QMetaObject::invokeMethod(m_quickWidget->rootObject(), "refreshPreview", Q_ARG(QVariant, idx));
}

QList<QToolButton *> MaterialBrowserWidget::createToolBarWidgets()
{
    return {};
}

void MaterialBrowserWidget::contextHelp(const Core::IContext::HelpCallback &callback) const
{
    if (m_materialBrowserView)
        m_materialBrowserView->contextHelp(callback);
    else
        callback({});
}

void MaterialBrowserWidget::handleSearchfilterChanged(const QString &filterText)
{
    if (filterText != m_filterText) {
        m_filterText = filterText;
        updateSearch();
    }
}

void MaterialBrowserWidget::startDragMaterial(int index, const QPointF &mousePos)
{
    m_materialToDrag = m_materialBrowserModel->materialAt(index);
    m_dragStartPoint = mousePos.toPoint();
}

QString MaterialBrowserWidget::qmlSourcesPath()
{
#ifdef SHARE_QML_PATH
    if (qEnvironmentVariableIsSet("LOAD_QML_FROM_SOURCE"))
        return QLatin1String(SHARE_QML_PATH) + "/materialBrowserQmlSource";
#endif
    return Core::ICore::resourcePath("qmldesigner/materialBrowserQmlSource").toString();
}

void MaterialBrowserWidget::clearSearchFilter()
{
    QMetaObject::invokeMethod(m_quickWidget->rootObject(), "clearSearchFilter");
}

void MaterialBrowserWidget::reloadQmlSource()
{
    const QString materialBrowserQmlPath = qmlSourcesPath() + "/MaterialBrowser.qml";

    QTC_ASSERT(QFileInfo::exists(materialBrowserQmlPath), return);

    m_quickWidget->engine()->clearComponentCache();
    m_quickWidget->setSource(QUrl::fromLocalFile(materialBrowserQmlPath));
}

void MaterialBrowserWidget::updateSearch()
{
    m_materialBrowserModel->setSearchText(m_filterText);
    m_quickWidget->update();
}

QQuickWidget *MaterialBrowserWidget::quickWidget() const
{
    return m_quickWidget.data();
}

QPointer<MaterialBrowserModel> MaterialBrowserWidget::materialBrowserModel() const
{
    return m_materialBrowserModel;
}

} // namespace QmlDesigner
