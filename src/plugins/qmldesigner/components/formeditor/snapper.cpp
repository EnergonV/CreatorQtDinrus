// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "snapper.h"

#include <QDebug>
#include <QLineF>
#include <QPen>
#include <QApplication>

#include <limits>
#include <qmlanchors.h>

#include <utils/algorithm.h>

namespace QmlDesigner {

Snapper::Snapper()
    : m_containerFormEditorItem(nullptr),
    m_transformtionSpaceFormEditorItem(nullptr),
    m_snappingDistance(5.0)
{
}

void Snapper::updateSnappingLines(const QList<FormEditorItem*> &exceptionList)
{
    if (m_containerFormEditorItem)
        m_containerFormEditorItem->updateSnappingLines(exceptionList, m_transformtionSpaceFormEditorItem);
}

void Snapper::updateSnappingLines(FormEditorItem* exceptionItem)
{
    QList<FormEditorItem*> exceptionList;
    exceptionList.append(exceptionItem);
    updateSnappingLines(exceptionList);
}


void Snapper::setContainerFormEditorItem(FormEditorItem *formEditorItem)
{
    m_containerFormEditorItem = formEditorItem;
}


void Snapper::setTransformtionSpaceFormEditorItem(FormEditorItem *formEditorItem)
{
    m_transformtionSpaceFormEditorItem = formEditorItem;
}

FormEditorItem *Snapper::transformtionSpaceFormEditorItem() const
{
    return m_transformtionSpaceFormEditorItem;
}

double Snapper::snappedVerticalOffset(const QRectF &boundingRect) const
{
    double offset = std::numeric_limits<double>::max();

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->leftSnappingLines(),
                                                boundingRect.left()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->rightSnappingOffsets(),
                                                      Qt::Vertical,
                                                      boundingRect.left(),
                                                      boundingRect.top(),
                                                      boundingRect.bottom()));

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->rightSnappingLines(),
                                                boundingRect.right()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->leftSnappingOffsets(),
                                                      Qt::Vertical,
                                                      boundingRect.right(),
                                                      boundingRect.top(),
                                                      boundingRect.bottom()));

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->verticalCenterSnappingLines(),
                                                boundingRect.center().x()));

    return offset;
}

double Snapper::snappedHorizontalOffset(const QRectF &boundingRect) const
{
    double offset = std::numeric_limits<double>::max();

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->topSnappingLines(),
                                                boundingRect.top()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->bottomSnappingOffsets(),
                                                      Qt::Horizontal,
                                                      boundingRect.top(),
                                                      boundingRect.left(),
                                                      boundingRect.right()));

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->bottomSnappingLines(),
                                                boundingRect.bottom()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->topSnappingOffsets(),
                                                      Qt::Horizontal,
                                                      boundingRect.bottom(),
                                                      boundingRect.left(),
                                                      boundingRect.right()));

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->horizontalCenterSnappingLines(),
                                                boundingRect.center().y()));
    return offset;
}


double Snapper::snapTopOffset(const QRectF &boundingRect) const
{
    double offset = std::numeric_limits<double>::max();

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->topSnappingLines(),
                                                boundingRect.top()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->bottomSnappingOffsets(),
                                                      Qt::Horizontal,
                                                      boundingRect.top(),
                                                      boundingRect.left(),
                                                      boundingRect.right()));
    return offset;
}

double Snapper::snapRightOffset(const QRectF &boundingRect) const
{
    double offset = std::numeric_limits<double>::max();

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->rightSnappingLines(),
                                                boundingRect.right()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->leftSnappingOffsets(),
                                                      Qt::Vertical,
                                                      boundingRect.right(),
                                                      boundingRect.top(),
                                                      boundingRect.bottom()));
    return offset;
}

double Snapper::snapLeftOffset(const QRectF &boundingRect) const
{
    double offset = std::numeric_limits<double>::max();

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->leftSnappingLines(),
                                                boundingRect.left()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->rightSnappingOffsets(),
                                                      Qt::Vertical,
                                                      boundingRect.left(),
                                                      boundingRect.top(),
                                                      boundingRect.bottom()));
    return offset;
}

double Snapper::snapBottomOffset(const QRectF &boundingRect) const
{
    double offset = std::numeric_limits<double>::max();

    offset = qMin(offset, snappedOffsetForLines(containerFormEditorItem()->bottomSnappingLines(),
                                                boundingRect.bottom()));

    offset = qMin(offset, snappedOffsetForOffsetLines(containerFormEditorItem()->topSnappingOffsets(),
                                                      Qt::Horizontal,
                                                      boundingRect.bottom(),
                                                      boundingRect.left(),
                                                      boundingRect.right()));
    return offset;

}


QList<QLineF> Snapper::verticalSnappedLines(const QRectF &boundingRect, QList<QRectF> *boundingRects) const
{
    QList<QLineF> lineList = findSnappingLines(containerFormEditorItem()->leftSnappingLines(),
                                               Qt::Vertical,
                                               boundingRect.left(),
                                               boundingRect.top(),
                                               boundingRect.bottom(),
                                               boundingRects);

    lineList += findSnappingOffsetLines(containerFormEditorItem()->rightSnappingOffsets(),
                                        Qt::Vertical,
                                        boundingRect.left(),
                                        boundingRect.top(),
                                        boundingRect.bottom(),
                                        boundingRects);


    lineList += findSnappingLines(containerFormEditorItem()->rightSnappingLines(),
                                  Qt::Vertical,
                                  boundingRect.right(),
                                  boundingRect.top(),
                                  boundingRect.bottom(),
                                  boundingRects);

    lineList += findSnappingOffsetLines(containerFormEditorItem()->leftSnappingOffsets(),
                                        Qt::Vertical,
                                        boundingRect.right(),
                                        boundingRect.top(),
                                        boundingRect.bottom(),
                                        boundingRects);

    lineList += findSnappingLines(containerFormEditorItem()->verticalCenterSnappingLines(),
                                  Qt::Vertical,
                                  boundingRect.center().x(),
                                  boundingRect.top(),
                                  boundingRect.bottom(),
                                  boundingRects);

    return lineList;
}

QList<QLineF> Snapper::horizontalSnappedLines(const QRectF &boundingRect, QList<QRectF> *boundingRects) const
{
    QList<QLineF> lineList =  findSnappingLines(containerFormEditorItem()->topSnappingLines(),
                                                Qt::Horizontal,
                                                boundingRect.top(),
                                                boundingRect.left(),
                                                boundingRect.right());

    lineList +=  findSnappingOffsetLines(containerFormEditorItem()->bottomSnappingOffsets(),
                                         Qt::Horizontal,
                                         boundingRect.top(),
                                         boundingRect.left(),
                                         boundingRect.right(),
                                         boundingRects);


    lineList +=  findSnappingLines(containerFormEditorItem()->bottomSnappingLines(),
                                   Qt::Horizontal,
                                   boundingRect.bottom(),
                                   boundingRect.left(),
                                   boundingRect.right(),
                                   boundingRects);

    lineList +=  findSnappingOffsetLines(containerFormEditorItem()->topSnappingOffsets(),
                                         Qt::Horizontal,
                                         boundingRect.bottom(),
                                         boundingRect.left(),
                                         boundingRect.right(),
                                         boundingRects);

    lineList += findSnappingLines(containerFormEditorItem()->horizontalCenterSnappingLines(),
                                  Qt::Horizontal,
                                  boundingRect.center().y(),
                                  boundingRect.left(),
                                  boundingRect.right(),
                                  boundingRects);

    return lineList;
}



FormEditorItem *Snapper::containerFormEditorItem() const
{
    return m_containerFormEditorItem;
}

QLineF Snapper::createSnapLine(Qt::Orientation orientation,
                               double snapLine,
                               double lowerLimit,
                               double upperLimit,
                               const QRectF &itemRect) const
{
    if (orientation == Qt::Horizontal) {
        double lowerX(qMin(lowerLimit, double(itemRect.left())));
        double upperX(qMax(upperLimit, double(itemRect.right())));
        return {lowerX, snapLine, upperX, snapLine};
    } else {
        double lowerY(qMin(lowerLimit, double(itemRect.top())));
        double upperY(qMax(upperLimit, double(itemRect.bottom())));
        return QLineF(snapLine, lowerY, snapLine, upperY);
    }
}

static bool  compareLines(double snapLine, double lineToSnap)
{
    return qFuzzyCompare(snapLine, lineToSnap);
}

QList<QLineF> Snapper::findSnappingLines(const SnapLineMap &snappingLineMap,
                                         Qt::Orientation orientation,
                                         double snapLine,
                                         double lowerLimit,
                                         double upperLimit,
                                         QList<QRectF> *boundingRects) const
{
    QList<QLineF> lineList;

    for (auto snappingLineIterator = snappingLineMap.cbegin(), end = snappingLineMap.cend();
              snappingLineIterator != end;
              ++snappingLineIterator) {

        if (compareLines(snapLine, snappingLineIterator.key())) { // near distance snapping lines
            lineList += createSnapLine(orientation,
                                        snappingLineIterator.key(),
                                        lowerLimit,
                                        upperLimit,
                                        snappingLineIterator.value().first);
            if (boundingRects != nullptr)
                boundingRects->append(snappingLineIterator.value().first);
        }
    }

    return lineList;
}


QList<QLineF> Snapper::findSnappingOffsetLines(const SnapLineMap &snappingOffsetMap,
                                         Qt::Orientation orientation,
                                         double snapLine,
                                         double lowerLimit,
                                         double upperLimit,
                                         QList<QRectF> *boundingRects) const
{
    QList<QLineF> lineList;

    for (auto snappingOffsetIterator = snappingOffsetMap.cbegin(), end = snappingOffsetMap.cend();
              snappingOffsetIterator != end;
              ++snappingOffsetIterator) {

        const QRectF &formEditorItemRect(snappingOffsetIterator.value().first);
        double formEditorItemRectLowerLimit;
        double formEditorItemRectUpperLimit;
        if (orientation == Qt::Horizontal) {
            formEditorItemRectLowerLimit = formEditorItemRect.left();
            formEditorItemRectUpperLimit = formEditorItemRect.right();
        } else {
            formEditorItemRectLowerLimit = formEditorItemRect.top();
            formEditorItemRectUpperLimit = formEditorItemRect.bottom();
        }


        if (qFuzzyCompare(snapLine, snappingOffsetIterator.key()) &&
           !(lowerLimit > formEditorItemRectUpperLimit ||
             upperLimit < formEditorItemRectLowerLimit)) {
            lineList += createSnapLine(orientation,
                                       snapLine,
                                       lowerLimit,
                                       upperLimit,
                                       formEditorItemRect);
            if (boundingRects != nullptr)
                boundingRects->append(snappingOffsetIterator.value().first);
        }
    }


    return lineList;
}

double Snapper::snappedOffsetForLines(const SnapLineMap &snappingLineMap,
                              double value) const
{
    QMultiMap<double, double> minimumSnappingLineMap;

    for (auto snappingLineIterator = snappingLineMap.cbegin(), end = snappingLineMap.cend();
              snappingLineIterator != end;
              ++snappingLineIterator) {
        double snapLine = snappingLineIterator.key();
        double offset = value - snapLine;
        double distance = qAbs(offset);

        if (distance < snappingDistance())
            minimumSnappingLineMap.insert(distance, offset);
    }

    if (!minimumSnappingLineMap.isEmpty())
        return  minimumSnappingLineMap.begin().value();

    return std::numeric_limits<double>::max();
}


double Snapper::snappedOffsetForOffsetLines(const SnapLineMap &snappingOffsetMap,
                              Qt::Orientation orientation,
                              double value,
                              double lowerLimit,
                              double upperLimit) const
{
    QMultiMap<double, double> minimumSnappingLineMap;

    for (auto snappingOffsetIterator = snappingOffsetMap.cbegin(), end = snappingOffsetMap.cend();
              snappingOffsetIterator != end;
              ++snappingOffsetIterator) {
        double snapLine = snappingOffsetIterator.key();
        const QRectF &formEditorItemRect(snappingOffsetIterator.value().first);
        double formEditorItemRectLowerLimit;
        double formEditorItemRectUpperLimit;
        if (orientation == Qt::Horizontal) {
            formEditorItemRectLowerLimit = formEditorItemRect.left();
            formEditorItemRectUpperLimit = formEditorItemRect.right();
        } else {
            formEditorItemRectLowerLimit = formEditorItemRect.top();
            formEditorItemRectUpperLimit = formEditorItemRect.bottom();
        }

        double offset = value - snapLine;
        double distance = qAbs(offset);

        if (distance < snappingDistance() &&
           !(lowerLimit > formEditorItemRectUpperLimit ||
             upperLimit < formEditorItemRectLowerLimit)) {

            minimumSnappingLineMap.insert(distance, offset);
        }
    }

    if (!minimumSnappingLineMap.isEmpty())
        return  minimumSnappingLineMap.begin().value();

    return std::numeric_limits<double>::max();
}


void Snapper::setSnappingDistance(double snappingDistance)
{
    m_snappingDistance = snappingDistance;
}

double Snapper::snappingDistance() const
{
    return m_snappingDistance;
}

static QLineF mergedHorizontalLine(const QList<QLineF> &lineList)
{
    if (lineList.count() == 1)
        return lineList.constFirst();

    double minimumX =  std::numeric_limits<double>::max();
    double maximumX =  std::numeric_limits<double>::min();
    for (const QLineF &line : lineList) {
        minimumX = qMin(minimumX, double(line.x1()));
        minimumX = qMin(minimumX, double(line.x2()));
        maximumX = qMax(maximumX, double(line.x1()));
        maximumX = qMax(maximumX, double(line.x2()));
    }

    double y(lineList.constFirst().y1());
    return {minimumX, y, maximumX, y};
}

static QLineF mergedVerticalLine(const QList<QLineF> &lineList)
{
    if (lineList.count() == 1)
        return lineList.constFirst();

    double minimumY =  std::numeric_limits<double>::max();
    double maximumY =  std::numeric_limits<double>::min();
    for (const QLineF &line : lineList) {
        minimumY = qMin(minimumY, double(line.y1()));
        minimumY = qMin(minimumY, double(line.y2()));
        maximumY = qMax(maximumY, double(line.y1()));
        maximumY = qMax(maximumY, double(line.y2()));
    }

    double x(lineList.constFirst().x1());
    return {x, minimumY, x, maximumY};
}

static QList<QLineF> mergedHorizontalLines(const QList<QLineF> &lineList)
{
    QList<QLineF> mergedLineList;

    QList<QLineF> sortedLineList(lineList);
    Utils::sort(sortedLineList, [](const QLineF &firstLine, const QLineF &secondLine) {
        return firstLine.y1() < secondLine.y2();
    });

    QList<QLineF> lineWithTheSameY;
    for (int i = 0, n = sortedLineList.size(); i < n; ++i) {
        QLineF line = sortedLineList.at(i);
        lineWithTheSameY.append(line);

        if (i + 1 < n) {
            QLineF nextLine = sortedLineList.at(i + 1);
            if (!qFuzzyCompare(line.y1(), nextLine.y1())) {
                mergedLineList.append(mergedHorizontalLine(lineWithTheSameY));
                lineWithTheSameY.clear();
            }
        } else {
            mergedLineList.append(mergedHorizontalLine(lineWithTheSameY));
        }
    }

    return mergedLineList;
}

static QList<QLineF> mergedVerticalLines(const QList<QLineF> &lineList)
{
    QList<QLineF> mergedLineList;

    QList<QLineF> sortedLineList(lineList);
    Utils::sort(sortedLineList, [](const QLineF &firstLine, const QLineF &secondLine) {
        return firstLine.x1() < secondLine.x2();
    });

    QList<QLineF> lineWithTheSameX;
    for (int i = 0, n = sortedLineList.size(); i < n; ++i) {
        QLineF line = sortedLineList.at(i);
        lineWithTheSameX.append(line);

        if (i + 1 < n) {
            QLineF nextLine = sortedLineList.at(i + 1);
            if (!qFuzzyCompare(line.x1(), nextLine.x1())) {
                mergedLineList.append(mergedVerticalLine(lineWithTheSameX));
                lineWithTheSameX.clear();
            }
        } else {
            mergedLineList.append(mergedVerticalLine(lineWithTheSameX));
        }
    }

    return mergedLineList;
}

QList<QGraphicsItem*> Snapper::generateSnappingLines(const QRectF &boundingRect,
                                                     QGraphicsItem *layerItem,
                                                     const QTransform &transform)
{
    QList<QRectF> boundingRectList;
    boundingRectList.append(boundingRect);

    return generateSnappingLines(boundingRectList, layerItem, transform);
}

static QmlItemNode findItemOnSnappingLine(const QmlItemNode &sourceQmlItemNode, const SnapLineMap &snappingLines, double anchorLine, AnchorLineType anchorLineType)
{
    QmlItemNode targetQmlItemNode;
    double targetAnchorLine =  0.0;

    targetAnchorLine = std::numeric_limits<double>::max();

    AnchorLineType compareAnchorLineType;

    if (anchorLineType == AnchorLineLeft
            || anchorLineType == AnchorLineRight)
        compareAnchorLineType = AnchorLineTop;
    else
        compareAnchorLineType = AnchorLineLeft;

    for (auto snapLineIterator = snappingLines.cbegin(), end = snappingLines.cend();
              snapLineIterator != end;
              ++snapLineIterator) {
        double snapLine = snapLineIterator.key();

        if (qAbs(snapLine - anchorLine ) < 1.0) {
            QmlItemNode possibleAchorItemNode = snapLineIterator.value().second->qmlItemNode();

            double currentToAnchorLine = possibleAchorItemNode.anchors().instanceAnchorLine(compareAnchorLineType);

            if (possibleAchorItemNode != sourceQmlItemNode) {
                if (sourceQmlItemNode.instanceParent() == possibleAchorItemNode) {
                    targetQmlItemNode = possibleAchorItemNode;
                    break;
                } else if (currentToAnchorLine < targetAnchorLine) {
                    targetQmlItemNode = possibleAchorItemNode;
                    targetAnchorLine = currentToAnchorLine;
                }
            }
        }
    }

    return targetQmlItemNode;
}

static void adjustAnchorLine(const QmlItemNode &sourceQmlItemNode,
                             const QmlItemNode &containerQmlItemNode,
                             const SnapLineMap &snappingLines,
                             const SnapLineMap &snappingOffsets,
                             AnchorLineType lineAnchorLineType,
                             AnchorLineType offsetAnchorLineType)
{
    QmlAnchors qmlAnchors = sourceQmlItemNode.anchors();

    double fromAnchorLine = sourceQmlItemNode.anchors().instanceAnchorLine(lineAnchorLineType);
    QmlItemNode targetQmlItemNode = findItemOnSnappingLine(sourceQmlItemNode, snappingLines, fromAnchorLine, lineAnchorLineType);

    if (targetQmlItemNode.isValid() && !targetQmlItemNode.anchors().checkForCycle(lineAnchorLineType, sourceQmlItemNode)) {
        double margin = 0.0;

        QRectF boundingRect = targetQmlItemNode.instanceContentItemBoundingRect();
        if (boundingRect.isNull())
             boundingRect = targetQmlItemNode.instanceBoundingRect();

        if (targetQmlItemNode == containerQmlItemNode) {
            if (lineAnchorLineType == AnchorLineLeft)
                margin = fromAnchorLine - boundingRect.left();
            else if (lineAnchorLineType == AnchorLineTop)
                margin =  fromAnchorLine - boundingRect.top();
            else if (lineAnchorLineType == AnchorLineRight)
                margin = boundingRect.right() - fromAnchorLine;
            else if (lineAnchorLineType == AnchorLineBottom)
                margin = boundingRect.bottom() - fromAnchorLine;

        }

        if (!qFuzzyIsNull(margin) || !qFuzzyIsNull(qmlAnchors.instanceMargin(lineAnchorLineType)))
            qmlAnchors.setMargin(lineAnchorLineType, margin);

        qmlAnchors.setAnchor(lineAnchorLineType, targetQmlItemNode, lineAnchorLineType);
    } else if (!snappingOffsets.isEmpty()) {
        targetQmlItemNode = findItemOnSnappingLine(sourceQmlItemNode, snappingOffsets, fromAnchorLine, lineAnchorLineType);
        if (targetQmlItemNode.isValid() && !targetQmlItemNode.anchors().checkForCycle(lineAnchorLineType, sourceQmlItemNode)) {
            double margin = fromAnchorLine - targetQmlItemNode.anchors().instanceAnchorLine(offsetAnchorLineType);

            if (lineAnchorLineType == AnchorLineRight
                    || lineAnchorLineType == AnchorLineBottom)
                    margin *= -1.;


            if (!qFuzzyIsNull(margin) || !qFuzzyIsNull(qmlAnchors.instanceMargin(lineAnchorLineType)))
                qmlAnchors.setMargin(lineAnchorLineType, margin);

            qmlAnchors.setAnchor(lineAnchorLineType, targetQmlItemNode, offsetAnchorLineType);
        }
    }
}

void Snapper::adjustAnchoringOfItem(FormEditorItem *formEditorItem)
{
    const QmlItemNode qmlItemNode = formEditorItem->qmlItemNode();
    const QmlAnchors qmlAnchors = qmlItemNode.anchors();

    if (!qmlAnchors.instanceHasAnchor(AnchorLineHorizontalCenter)) {
        adjustAnchorLine(qmlItemNode,
                         containerFormEditorItem()->qmlItemNode(),
                         containerFormEditorItem()->leftSnappingLines(),
                         containerFormEditorItem()->rightSnappingOffsets(),
                         AnchorLineLeft,
                         AnchorLineRight);
    }

    if (!qmlAnchors.instanceHasAnchor(AnchorLineVerticalCenter)) {
        adjustAnchorLine(qmlItemNode,
                         containerFormEditorItem()->qmlItemNode(),
                         containerFormEditorItem()->topSnappingLines(),
                         containerFormEditorItem()->bottomSnappingOffsets(),
                         AnchorLineTop,
                         AnchorLineBottom);

        adjustAnchorLine(qmlItemNode,
                         containerFormEditorItem()->qmlItemNode(),
                         containerFormEditorItem()->bottomSnappingLines(),
                         containerFormEditorItem()->topSnappingOffsets(),
                         AnchorLineBottom,
                         AnchorLineTop);
    }

    if (!qmlAnchors.instanceHasAnchor(AnchorLineHorizontalCenter)) {
        adjustAnchorLine(qmlItemNode,
                         containerFormEditorItem()->qmlItemNode(),
                         containerFormEditorItem()->rightSnappingLines(),
                         containerFormEditorItem()->leftSnappingOffsets(),
                         AnchorLineRight,
                         AnchorLineLeft);
    }

    if (!qmlAnchors.instanceHasAnchor(AnchorLineLeft) && !qmlAnchors.instanceHasAnchor(AnchorLineRight)) {
        adjustAnchorLine(qmlItemNode,
                         containerFormEditorItem()->qmlItemNode(),
                         containerFormEditorItem()->verticalCenterSnappingLines(),
                         SnapLineMap(),
                         AnchorLineHorizontalCenter,
                         AnchorLineHorizontalCenter);
    }

    if (!qmlAnchors.instanceHasAnchor(AnchorLineTop) && !qmlAnchors.instanceHasAnchor(AnchorLineBottom)) {
        adjustAnchorLine(qmlItemNode,
                         containerFormEditorItem()->qmlItemNode(),
                         containerFormEditorItem()->horizontalCenterSnappingLines(),
                         SnapLineMap(),
                         AnchorLineVerticalCenter,
                         AnchorLineVerticalCenter);
    }
}

QList<QGraphicsItem*> Snapper::generateSnappingLines(const QList<QRectF> &boundingRectList,
                                                     QGraphicsItem *layerItem,
                                                     const QTransform &transform)
{
    QList<QGraphicsItem*> graphicsItemList;
    QList<QLineF> lineList;
    for (const QRectF &boundingRect : boundingRectList) {
        QList<QRectF> snappedBoundingRectList;
        lineList += mergedHorizontalLines(horizontalSnappedLines(boundingRect, &snappedBoundingRectList));
        lineList += mergedVerticalLines(verticalSnappedLines(boundingRect, &snappedBoundingRectList));
    }

    for (const QLineF &line : qAsConst(lineList)) {
        QLineF lineInTransformationSpace = transform.map(line);
        auto lineItem = new QGraphicsLineItem(lineInTransformationSpace, layerItem);
        lineItem->setZValue(40);
        QPen linePen;
        linePen.setCosmetic(true);
        linePen.setColor(QColor(0x5d, 0x2d, 0xd7));
        lineItem->setPen(linePen);

        graphicsItemList.append(lineItem);
    }

    return graphicsItemList;
}

}
