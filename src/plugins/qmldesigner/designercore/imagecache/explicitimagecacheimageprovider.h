// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>

namespace QmlDesigner {

class AsynchronousExplicitImageCache;

class ExplicitImageCacheImageProvider : public QQuickAsyncImageProvider
{
public:
    ExplicitImageCacheImageProvider(AsynchronousExplicitImageCache &imageCache,
                                    const QImage &defaultImage)
        : m_cache(imageCache)
        , m_defaultImage(defaultImage)
    {}

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    AsynchronousExplicitImageCache &m_cache;
    QImage m_defaultImage;
};

} // namespace QmlDesigner
