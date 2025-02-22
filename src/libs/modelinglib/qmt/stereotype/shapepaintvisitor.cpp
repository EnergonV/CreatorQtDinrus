// Copyright (C) 2016 Jochen Becher
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "shapepaintvisitor.h"

#include "shapes.h"

#include <QPainterPath>

namespace qmt {

ShapePaintVisitor::ShapePaintVisitor(QPainter *painter, const QPointF &scaledOrigin, const QSizeF &originalSize,
                                     const QSizeF &baseSize, const QSizeF &size)
    : m_painter(painter),
      m_scaledOrigin(scaledOrigin),
      m_originalSize(originalSize),
      m_baseSize(baseSize),
      m_size(size)
{
}

void ShapePaintVisitor::visitLine(const LineShape *shapeLine)
{
    QPointF p1 = shapeLine->pos1().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    QPointF p2 = shapeLine->pos2().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    m_painter->drawLine(p1, p2);
}

void ShapePaintVisitor::visitRect(const RectShape *shapeRect)
{
    m_painter->drawRect(QRectF(shapeRect->pos().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                              shapeRect->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size)));
}

void ShapePaintVisitor::visitRoundedRect(const RoundedRectShape *shapeRoundedRect)
{
    qreal radiusX = shapeRoundedRect->radius().mapScaledTo(0, m_originalSize.width(), m_baseSize.width(), m_size.width());
    qreal radiusY = shapeRoundedRect->radius().mapScaledTo(0, m_originalSize.height(), m_baseSize.height(), m_size.height());
    m_painter->drawRoundedRect(QRectF(shapeRoundedRect->pos().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                                     shapeRoundedRect->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size)),
                              radiusX, radiusY);
}

void ShapePaintVisitor::visitCircle(const CircleShape *shapeCircle)
{
    m_painter->drawEllipse(shapeCircle->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                          shapeCircle->radius().mapScaledTo(m_scaledOrigin.x(), m_originalSize.width(), m_baseSize.width(), m_size.width()),
                          shapeCircle->radius().mapScaledTo(m_scaledOrigin.y(), m_originalSize.height(), m_baseSize.height(), m_size.height()));
}

void ShapePaintVisitor::visitEllipse(const EllipseShape *shapeEllipse)
{
    QSizeF radius = shapeEllipse->radius().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    m_painter->drawEllipse(shapeEllipse->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                          radius.width(), radius.height());
}

void ShapePaintVisitor::visitDiamond(const DiamondShape *shapeDiamond)
{
    m_painter->save();
    m_painter->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    QPointF center = shapeDiamond->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    QSizeF size = shapeDiamond->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    path.moveTo(center + QPointF(0.0, size.height() / 2.0));
    path.lineTo(center + QPointF(-size.width() / 2.0, 0.0));
    path.lineTo(center + QPointF(0.0, -size.height() / 2.0));
    path.lineTo(center + QPointF(size.width() / 2.0, 0.0));
    path.closeSubpath();
    m_painter->drawPath(path);
    m_painter->restore();
}

void ShapePaintVisitor::visitTriangle(const TriangleShape *shapeTriangle)
{
    m_painter->save();
    m_painter->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    QPointF center = shapeTriangle->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    QSizeF size = shapeTriangle->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    path.moveTo(center + QPointF(size.width() / 2.0, size.height() / 2.0));
    path.lineTo(center + QPointF(-size.width() / 2.0, size.height() / 2.0));
    path.lineTo(center + QPointF(0.0, -size.height() / 2.0));
    path.closeSubpath();
    m_painter->drawPath(path);
    m_painter->restore();
}

void ShapePaintVisitor::visitArc(const ArcShape *shapeArc)
{
    QSizeF radius = shapeArc->radius().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    m_painter->drawArc(QRectF(shapeArc->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()), radius * 2.0),
                      shapeArc->startAngle() * 16, shapeArc->spanAngle() * 16);
}

void ShapePaintVisitor::visitPath(const PathShape *shapePath)
{
    QPainterPath path;
    foreach (const PathShape::Element &element, shapePath->elements()) {
        switch (element.m_elementType) {
        case PathShape::TypeNone:
            // nothing to do
            break;
        case PathShape::TypeMoveto:
            path.moveTo(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
            break;
        case PathShape::TypeLineto:
            path.lineTo(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
            break;
        case PathShape::TypeArcmoveto:
        {
            QSizeF radius = element.m_size.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
            path.arcMoveTo(QRectF(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()),
                                  radius * 2.0),
                           element.m_angle1);
            break;
        }
        case PathShape::TypeArcto:
        {
            QSizeF radius = element.m_size.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
            path.arcTo(QRectF(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()),
                              radius * 2.0),
                       element.m_angle1, element.m_angle2);
            break;
        }
        case PathShape::TypeClose:
            path.closeSubpath();
            break;
        }
    }
    m_painter->drawPath(path);
}

ShapeSizeVisitor::ShapeSizeVisitor(const QPointF &scaledOrigin, const QSizeF &originalSize, const QSizeF &baseSize,
                                   const QSizeF &size)
    : m_scaledOrigin(scaledOrigin),
      m_originalSize(originalSize),
      m_baseSize(baseSize),
      m_size(size)
{
}

void ShapeSizeVisitor::visitLine(const LineShape *shapeLine)
{
    m_boundingRect |= QRectF(shapeLine->pos1().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                             shapeLine->pos2().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
}

void ShapeSizeVisitor::visitRect(const RectShape *shapeRect)
{
    m_boundingRect |= QRectF(shapeRect->pos().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                             shapeRect->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
}

void ShapeSizeVisitor::visitRoundedRect(const RoundedRectShape *shapeRoundedRect)
{
    m_boundingRect |= QRectF(shapeRoundedRect->pos().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size),
                             shapeRoundedRect->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
}

void ShapeSizeVisitor::visitCircle(const CircleShape *shapeCircle)
{
    QSizeF radius = QSizeF(shapeCircle->radius().mapScaledTo(m_scaledOrigin.x(), m_originalSize.width(), m_baseSize.width(), m_size.width()),
                           shapeCircle->radius().mapScaledTo(m_scaledOrigin.y(), m_originalSize.height(), m_baseSize.height(), m_size.height()));
    m_boundingRect |= QRectF(shapeCircle->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()), radius * 2.0);
}

void ShapeSizeVisitor::visitEllipse(const EllipseShape *shapeEllipse)
{
    QSizeF radius = shapeEllipse->radius().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    m_boundingRect |= QRectF(shapeEllipse->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()), radius * 2.0);
}

void ShapeSizeVisitor::visitDiamond(const DiamondShape *shapeDiamond)
{
    QPainterPath path;
    QPointF center = shapeDiamond->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    QSizeF size = shapeDiamond->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    path.moveTo(center + QPointF(0.0, size.height() / 2.0));
    path.lineTo(center + QPointF(-size.width() / 2.0, 0.0));
    path.lineTo(center + QPointF(0.0, -size.height() / 2.0));
    path.lineTo(center + QPointF(size.width() / 2.0, 0.0));
    path.closeSubpath();
    m_boundingRect |= path.boundingRect();
}

void ShapeSizeVisitor::visitTriangle(const TriangleShape *shapeTriangle)
{
    QPainterPath path;
    QPointF center = shapeTriangle->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    QSizeF size = shapeTriangle->size().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    path.moveTo(center + QPointF(size.width() / 2.0, size.height() / 2.0));
    path.lineTo(center + QPointF(-size.width() / 2.0, size.height() / 2.0));
    path.lineTo(center + QPointF(0.0, -size.height() / 2.0));
    path.closeSubpath();
    m_boundingRect |= path.boundingRect();
}

void ShapeSizeVisitor::visitArc(const ArcShape *shapeArc)
{
    // TODO this is the max bound rect; not the minimal one
    QSizeF radius = shapeArc->radius().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    m_boundingRect |= QRectF(shapeArc->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()), radius * 2.0);
}

void ShapeSizeVisitor::visitPath(const PathShape *shapePath)
{
    QPainterPath path;
    foreach (const PathShape::Element &element, shapePath->elements()) {
        switch (element.m_elementType) {
        case PathShape::TypeNone:
            // nothing to do
            break;
        case PathShape::TypeMoveto:
            path.moveTo(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
            break;
        case PathShape::TypeLineto:
            path.lineTo(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size));
            break;
        case PathShape::TypeArcmoveto:
        {
            QSizeF radius = element.m_size.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
            path.arcMoveTo(QRectF(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()),
                                  radius * 2.0),
                           element.m_angle1);
            break;
        }
        case PathShape::TypeArcto:
        {
            QSizeF radius = element.m_size.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
            path.arcTo(QRectF(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size) - QPointF(radius.width(), radius.height()),
                              radius * 2.0),
                       element.m_angle1, element.m_angle2);
            break;
        }
        case PathShape::TypeClose:
            path.closeSubpath();
            break;
        }
    }
    m_boundingRect |= path.boundingRect();
}

ShapePolygonVisitor::ShapePolygonVisitor(const QPointF &scaledOrigin, const QSizeF &originalSize,
                                         const QSizeF &baseSize, const QSizeF &size)
    : m_scaledOrigin(scaledOrigin),
      m_originalSize(originalSize),
      m_baseSize(baseSize),
      m_size(size)
{
    m_path.setFillRule(Qt::WindingFill);
}

QList<QPolygonF> ShapePolygonVisitor::toPolygons() const
{
    return m_path.toSubpathPolygons();
}

void ShapePolygonVisitor::visitLine(const LineShape *shapeLine)
{
    QPointF p1 = shapeLine->pos1().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    QPointF p2 = shapeLine->pos2().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size);
    m_path.moveTo(p1);
    m_path.lineTo(p2);
}

void ShapePolygonVisitor::visitRect(const RectShape *shapeRect)
{
    m_path.addRect(QRectF(shapeRect->pos().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                       m_baseSize, m_size),
                          shapeRect->size().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                        m_baseSize, m_size)));
}

void ShapePolygonVisitor::visitRoundedRect(const RoundedRectShape *shapeRoundedRect)
{
    qreal radiusX = shapeRoundedRect->radius().mapScaledTo(0, m_originalSize.width(),
                                                           m_baseSize.width(), m_size.width());
    qreal radiusY = shapeRoundedRect->radius().mapScaledTo(0, m_originalSize.height(),
                                                           m_baseSize.height(), m_size.height());
    m_path.addRoundedRect(QRectF(shapeRoundedRect->pos().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                                     m_baseSize, m_size),
                                 shapeRoundedRect->size().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                                      m_baseSize, m_size)),
                          radiusX, radiusY);
}

void ShapePolygonVisitor::visitCircle(const CircleShape *shapeCircle)
{
    m_path.addEllipse(shapeCircle->center().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                        m_baseSize, m_size),
                      shapeCircle->radius().mapScaledTo(m_scaledOrigin.x(), m_originalSize.width(),
                                                        m_baseSize.width(), m_size.width()),
                      shapeCircle->radius().mapScaledTo(m_scaledOrigin.y(), m_originalSize.height(),
                                                        m_baseSize.height(), m_size.height()));
}

void ShapePolygonVisitor::visitEllipse(const EllipseShape *shapeEllipse)
{
    QSizeF radius = shapeEllipse->radius().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                       m_baseSize, m_size);
    m_path.addEllipse(shapeEllipse->center().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                         m_baseSize, m_size),
                      radius.width(), radius.height());
}

void ShapePolygonVisitor::visitDiamond(const DiamondShape *shapeDiamond)
{
    QPainterPath path;
    QPointF center = shapeDiamond->center().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                        m_baseSize, m_size);
    QSizeF size = shapeDiamond->size().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                   m_baseSize, m_size);
    path.moveTo(center + QPointF(0.0, size.height() / 2.0));
    path.lineTo(center + QPointF(-size.width() / 2.0, 0.0));
    path.lineTo(center + QPointF(0.0, -size.height() / 2.0));
    path.lineTo(center + QPointF(size.width() / 2.0, 0.0));
    path.closeSubpath();
    m_path.addPath(path);
}

void ShapePolygonVisitor::visitTriangle(const TriangleShape *shapeTriangle)
{
    QPainterPath path;
    QPointF center = shapeTriangle->center().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                         m_baseSize, m_size);
    QSizeF size = shapeTriangle->size().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                    m_baseSize, m_size);
    path.moveTo(center + QPointF(size.width() / 2.0, size.height() / 2.0));
    path.lineTo(center + QPointF(-size.width() / 2.0, size.height() / 2.0));
    path.lineTo(center + QPointF(0.0, -size.height() / 2.0));
    path.closeSubpath();
    m_path.addPath(path);
}

void ShapePolygonVisitor::visitArc(const ArcShape *shapeArc)
{
    QSizeF radius = shapeArc->radius().mapScaledTo(m_scaledOrigin, m_originalSize,
                                                   m_baseSize, m_size);
    QRectF rect(shapeArc->center().mapScaledTo(m_scaledOrigin, m_originalSize, m_baseSize, m_size)
                - QPointF(radius.width(), radius.height()), radius * 2.0);
    m_path.arcMoveTo(rect, shapeArc->startAngle());
    m_path.arcTo(rect, shapeArc->startAngle(), shapeArc->spanAngle());
}

void ShapePolygonVisitor::visitPath(const PathShape *shapePath)
{
    QPainterPath path;
    for (const PathShape::Element &element: shapePath->elements()) {
        switch (element.m_elementType) {
        case PathShape::TypeNone:
            // nothing to do
            break;
        case PathShape::TypeMoveto:
            path.moveTo(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize,
                                                       m_baseSize, m_size));
            break;
        case PathShape::TypeLineto:
            path.lineTo(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize,
                                                       m_baseSize, m_size));
            break;
        case PathShape::TypeArcmoveto:
        {
            QSizeF radius = element.m_size.mapScaledTo(m_scaledOrigin, m_originalSize,
                                                       m_baseSize, m_size);
            path.arcMoveTo(QRectF(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize,
                                                                 m_baseSize, m_size)
                                  - QPointF(radius.width(), radius.height()),
                                  radius * 2.0),
                           element.m_angle1);
            break;
        }
        case PathShape::TypeArcto:
        {
            QSizeF radius = element.m_size.mapScaledTo(m_scaledOrigin, m_originalSize,
                                                       m_baseSize, m_size);
            path.arcTo(QRectF(element.m_position.mapScaledTo(m_scaledOrigin, m_originalSize,
                                                             m_baseSize, m_size)
                              - QPointF(radius.width(), radius.height()),
                              radius * 2.0),
                       element.m_angle1, element.m_angle2);
            break;
        }
        case PathShape::TypeClose:
            path.closeSubpath();
            break;
        }
    }
    m_path.addPath(path);
}

} // namespace qmt
