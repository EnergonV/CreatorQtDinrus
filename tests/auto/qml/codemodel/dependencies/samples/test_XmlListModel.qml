// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick 2.0
import QtQuick.XmlListModel 2.0

XmlListModel {
    id: xmlModel
    source: "http://www.mysite.com/feed.xml"
    query: "/rss/channel/item"

    XmlRole { name: "title"; query: "title/string()" }
    XmlRole { name: "pubDate"; query: "pubDate/string()" }
}
