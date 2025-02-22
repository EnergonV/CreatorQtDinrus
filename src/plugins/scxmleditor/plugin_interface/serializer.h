// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <QPoint>
#include <QPointF>
#include <QPolygon>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QStringList>

namespace ScxmlEditor {

namespace PluginInterface {

/**
 * @brief The Serializer class provides serialization of item-data to the string.
 *
 * For example items inside the graphicsview uses this class when they need to save ui-properties to the scxmltag-attribute.
 */
class Serializer
{
public:
    Serializer();

    void seek(int pos);
    void clear();

    void append(const QPolygonF &d);
    void append(const QPolygon &d);
    void append(const QRectF &d);
    void append(const QRect &d);
    void append(const QPointF &d);
    void append(const QPoint &d);

    void read(QPolygonF &d);
    void read(QPolygon &d);
    void read(QRectF &d);
    void read(QRect &d);
    void read(QPointF &d);
    void read(QPoint &d);

    void setSeparator(const QString &sep);
    void setData(const QString &d);
    QString data() const;

private:
    void append(double d);
    double readNext();

    int m_index = 0;
    const QString m_separator;
    QStringList m_data;

    template <class T, class P>
    void readPolygon(P &d)
    {
        int count = (m_data.count() - m_index) / 2;
        for (int i = 0; i < count; ++i) {
            T p;
            read(p);
            d << p;
        }
    }

    template <class T>
    void readRect(T &d)
    {
        d.setLeft(readNext());
        d.setTop(readNext());
        d.setWidth(readNext());
        d.setHeight(readNext());
    }

    template <class T>
    void readPoint(T &d)
    {
        d.setX(readNext());
        d.setY(readNext());
    }

    template <class T>
    void appendPolygon(const T &d)
    {
        for (int i = 0; i < d.count(); ++i) {
            append(d[i].x());
            append(d[i].y());
        }
    }

    template <class T>
    void appendPoint(const T &d)
    {
        append(d.x());
        append(d.y());
    }

    template <class T>
    void appendRect(const T &d)
    {
        append(d.left());
        append(d.top());
        append(d.width());
        append(d.height());
    }
};

} // namespace PluginInterface
} // namespace ScxmlEditor
