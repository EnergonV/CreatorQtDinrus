// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "inputeventsmodel.h"
#include "qmlprofilermodelmanager.h"
#include "qmlprofilereventtypes.h"

#include <tracing/timelineformattime.h>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QMetaEnum>

namespace QmlProfiler {
namespace Internal {

InputEventsModel::InputEventsModel(QmlProfilerModelManager *manager,
                                   Timeline::TimelineModelAggregator *parent) :
    QmlProfilerTimelineModel(manager, Event, MaximumRangeType, ProfileInputEvents, parent),
    m_keyTypeId(-1), m_mouseTypeId(-1)
{
}

int InputEventsModel::typeId(int index) const
{
    return selectionId(index) == Mouse ? m_mouseTypeId : m_keyTypeId;
}

QRgb InputEventsModel::color(int index) const
{
    return colorBySelectionId(index);
}

QVariantList InputEventsModel::labels() const
{
    QVariantList result;

    QVariantMap element;
    element.insert(QLatin1String("description"), QVariant(tr("Mouse Events")));
    element.insert(QLatin1String("id"), QVariant(Mouse));
    result << element;

    element.clear();
    element.insert(QLatin1String("description"), QVariant(tr("Keyboard Events")));
    element.insert(QLatin1String("id"), QVariant(Key));
    result << element;

    return result;
}

QMetaEnum InputEventsModel::metaEnum(const char *name)
{
    return Qt::staticMetaObject.enumerator(Qt::staticMetaObject.indexOfEnumerator(name));
}

QVariantMap InputEventsModel::details(int index) const
{
    QVariantMap result;
    result.insert(tr("Timestamp"), Timeline::formatTime(startTime(index),
                                                        modelManager()->traceDuration()));
    QString type;
    const Item &event = m_data[index];
    switch (event.type) {
    case InputKeyPress:
        type = tr("Key Press");
        Q_FALLTHROUGH();
    case InputKeyRelease:
        if (type.isEmpty())
            type = tr("Key Release");
        if (event.a != 0) {
            result.insert(tr("Key"), QLatin1String(metaEnum("Key").valueToKey(event.a)));
        }
        if (event.b != 0) {
            result.insert(tr("Modifiers"),
                          QLatin1String(metaEnum("KeyboardModifiers").valueToKeys(event.b)));
        }
        break;
    case InputMouseDoubleClick:
        type = tr("Double Click");
        Q_FALLTHROUGH();
    case InputMousePress:
        if (type.isEmpty())
            type = tr("Mouse Press");
        Q_FALLTHROUGH();
    case InputMouseRelease:
        if (type.isEmpty())
            type = tr("Mouse Release");
        result.insert(tr("Button"), QLatin1String(metaEnum("MouseButtons").valueToKey(event.a)));
        result.insert(tr("Result"), QLatin1String(metaEnum("MouseButtons").valueToKeys(event.b)));
        break;
    case InputMouseMove:
        type = tr("Mouse Move");
        result.insert(tr("X"), QString::number(event.a));
        result.insert(tr("Y"), QString::number(event.b));
        break;
    case InputMouseWheel:
        type = tr("Mouse Wheel");
        result.insert(tr("Angle X"), QString::number(event.a));
        result.insert(tr("Angle Y"), QString::number(event.b));
        break;
    case InputKeyUnknown:
        type = tr("Keyboard Event");
        break;
    case InputMouseUnknown:
        type = tr("Mouse Event");
        break;
    default:
        type = tr("Unknown");
        break;
    }

    result.insert(QLatin1String("displayName"), type);

    return result;
}

int InputEventsModel::expandedRow(int index) const
{
    return selectionId(index) == Mouse ? 1 : 2;
}

int InputEventsModel::collapsedRow(int index) const
{
    Q_UNUSED(index)
    return 1;
}

void InputEventsModel::loadEvent(const QmlEvent &event, const QmlEventType &type)
{
    m_data.insert(insert(event.timestamp(), 0, type.detailType()),
                  Item(static_cast<InputEventType>(event.number<qint32>(0)),
                             event.number<qint32>(1), event.number<qint32>(2)));

    if (type.detailType() == Mouse) {
        if (m_mouseTypeId == -1)
            m_mouseTypeId = event.typeIndex();
    } else if (m_keyTypeId == -1) {
        m_keyTypeId = event.typeIndex();
    }
}

void InputEventsModel::finalize()
{
    setCollapsedRowCount(2);
    setExpandedRowCount(3);
    QmlProfilerTimelineModel::finalize();
}

void InputEventsModel::clear()
{
    m_keyTypeId = m_mouseTypeId = -1;
    m_data.clear();
    QmlProfilerTimelineModel::clear();
}

InputEventsModel::Item::Item(InputEventType type, int a, int b) :
    type(type), a(a), b(b)
{
}

} // namespace Internal
} // namespace QmlProfiler
