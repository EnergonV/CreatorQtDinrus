// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "assetslibraryview.h"
#include "assetslibrarywidget.h"
#include "metainfo.h"
#include <asynchronousimagecache.h>
#include <bindingproperty.h>
#include <coreplugin/icore.h>
#include <imagecache/imagecachecollector.h>
#include <imagecache/imagecacheconnectionmanager.h>
#include <imagecache/imagecachefontcollector.h>
#include <imagecache/imagecachegenerator.h>
#include <imagecache/imagecachestorage.h>
#include <imagecache/timestampprovider.h>
#include <nodelistproperty.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
#include <projectexplorer/target.h>
#include <rewriterview.h>
#include <sqlitedatabase.h>
#include <synchronousimagecache.h>
#include <utils/algorithm.h>
#include <qmldesignerplugin.h>
#include <qmlitemnode.h>
#include <qmldesignerconstants.h>

namespace QmlDesigner {

class AssetsLibraryView::ImageCacheData
{
public:
    Sqlite::Database database{Utils::PathString{
                                  Core::ICore::cacheResourcePath("fontimagecache.db").toString()},
                              Sqlite::JournalMode::Wal,
                              Sqlite::LockingMode::Normal};
    ImageCacheStorage<Sqlite::Database> storage{database};
    ImageCacheFontCollector fontCollector;
    ImageCacheGenerator fontGenerator{fontCollector, storage};
    TimeStampProvider timeStampProvider;
    AsynchronousImageCache asynchronousFontImageCache{storage, fontGenerator, timeStampProvider};
    SynchronousImageCache synchronousFontImageCache{storage, timeStampProvider, fontCollector};
};

AssetsLibraryView::AssetsLibraryView(QObject *parent)
    : AbstractView(parent)
{}

AssetsLibraryView::~AssetsLibraryView()
{}

bool AssetsLibraryView::hasWidget() const
{
    return true;
}

WidgetInfo AssetsLibraryView::widgetInfo()
{
    if (m_widget.isNull()) {
        m_widget = new AssetsLibraryWidget{imageCacheData()->asynchronousFontImageCache,
                                           imageCacheData()->synchronousFontImageCache};
    }

    return createWidgetInfo(m_widget.data(), "Assets", WidgetInfo::LeftPane, 0, tr("Assets"));
}

void AssetsLibraryView::modelAttached(Model *model)
{
    AbstractView::modelAttached(model);

    m_widget->clearSearchFilter();
    m_widget->setModel(model);

    setResourcePath(DocumentManager::currentResourcePath().toFileInfo().absoluteFilePath());
}

void AssetsLibraryView::modelAboutToBeDetached(Model *model)
{
    AbstractView::modelAboutToBeDetached(model);

    m_widget->setModel(nullptr);
}

void AssetsLibraryView::setResourcePath(const QString &resourcePath)
{

    if (resourcePath == m_lastResourcePath)
        return;

    m_lastResourcePath = resourcePath;

    if (m_widget.isNull()) {
        m_widget = new AssetsLibraryWidget{imageCacheData()->asynchronousFontImageCache,
                                           imageCacheData()->synchronousFontImageCache};
    }

    m_widget->setResourcePath(resourcePath);
}

AssetsLibraryView::ImageCacheData *AssetsLibraryView::imageCacheData()
{
    std::call_once(imageCacheFlag,
                   [this]() { m_imageCacheData = std::make_unique<ImageCacheData>(); });
    return m_imageCacheData.get();
}

} // namespace QmlDesigner
