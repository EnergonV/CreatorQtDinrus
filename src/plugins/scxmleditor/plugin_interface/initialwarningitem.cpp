// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "initialwarningitem.h"
#include "initialstateitem.h"
#include "sceneutils.h"

using namespace ScxmlEditor::PluginInterface;

InitialWarningItem::InitialWarningItem(InitialStateItem *parent)
    : WarningItem(parent)
    , m_parentItem(parent)
{
    setSeverity(OutputPane::Warning::ErrorType);
    setTypeName(tr("Initial"));
    setDescription(tr("One level can contain only one initial state."));
    setReason(tr("Too many initial states at the same level."));
}

void InitialWarningItem::updatePos()
{
    setPos(m_parentItem->boundingRect().topLeft());
}

void InitialWarningItem::check()
{
    if (m_parentItem)
        setWarningActive(SceneUtils::hasSiblingStates(m_parentItem));
}
