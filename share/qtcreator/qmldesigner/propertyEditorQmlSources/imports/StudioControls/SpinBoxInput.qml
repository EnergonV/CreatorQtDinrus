// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick 2.15
import QtQuick.Templates 2.15 as T
import StudioTheme 1.0 as StudioTheme

TextInput {
    id: textInput

    property T.Control myControl

    property bool edit: textInput.activeFocus
    property bool drag: false
    property bool hover: mouseArea.containsMouse && textInput.enabled

    z: 2
    font: myControl.font
    color: StudioTheme.Values.themeTextColor
    selectionColor: StudioTheme.Values.themeTextSelectionColor
    selectedTextColor: StudioTheme.Values.themeTextSelectedTextColor

    horizontalAlignment: Qt.AlignRight
    verticalAlignment: Qt.AlignVCenter
    leftPadding: StudioTheme.Values.inputHorizontalPadding
    rightPadding: StudioTheme.Values.inputHorizontalPadding

    readOnly: !myControl.editable
    validator: myControl.validator
    inputMethodHints: myControl.inputMethodHints
    selectByMouse: false
    activeFocusOnPress: false
    clip: true

    // TextInput focus needs to be set to activeFocus whenever it changes,
    // otherwise TextInput will get activeFocus whenever the parent SpinBox gets
    // activeFocus. This will lead to weird side effects.
    onActiveFocusChanged: textInput.focus = textInput.activeFocus

    Rectangle {
        id: textInputBackground
        x: 0
        y: StudioTheme.Values.border
        z: -1
        width: textInput.width
        height: StudioTheme.Values.height - (StudioTheme.Values.border * 2)
        color: StudioTheme.Values.themeControlBackground
        border.width: 0
    }

    DragHandler {
        id: dragHandler
        target: null
        acceptedDevices: PointerDevice.Mouse
        enabled: true

        property int initialValue: 0

        onActiveChanged: {
            if (active) {
                initialValue = myControl.value
                mouseArea.cursorShape = Qt.ClosedHandCursor
                myControl.drag = true
            } else {
                mouseArea.cursorShape = Qt.PointingHandCursor
                myControl.drag = false
            }
        }
        onTranslationChanged: {
            var currValue = myControl.value
            myControl.value = initialValue + translation.x

            if (currValue !== myControl.value)
                myControl.valueModified()
        }
    }

    TapHandler {
        id: tapHandler
        acceptedDevices: PointerDevice.Mouse
        enabled: true
        onTapped: {
            textInput.forceActiveFocus()
            textInput.deselect() // QTBUG-75862
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: true
        hoverEnabled: true
        propagateComposedEvents: true
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.PointingHandCursor
        onPressed: function(mouse) { mouse.accepted = false }
        onWheel: function(wheel) {
            if (!myControl.__wheelEnabled)
                return

            var val = myControl.valueFromText(textInput.text, myControl.locale)
            if (myControl.value !== val)
                myControl.value = val

            var currValue = myControl.value
            myControl.value += wheel.angleDelta.y / 120

            if (currValue !== myControl.value)
                myControl.valueModified()
        }
    }

    states: [
        State {
            name: "default"
            when: myControl.enabled && !textInput.edit && !textInput.hover && !myControl.hover
                  && !myControl.drag && !myControl.sliderDrag
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackground
            }
            PropertyChanges {
                target: dragHandler
                enabled: true
            }
            PropertyChanges {
                target: tapHandler
                enabled: true
            }
            PropertyChanges {
                target: mouseArea
                cursorShape: Qt.PointingHandCursor
            }
        },
        State {
            name: "globalHover"
            when: myControl.hover && !textInput.hover && !textInput.edit && !myControl.drag
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackgroundGlobalHover
            }
        },
        State {
            name: "hover"
            when: textInput.hover && myControl.hover
                  && !textInput.edit && !myControl.drag
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackgroundHover
            }
        },
        State {
            name: "edit"
            when: textInput.edit && !myControl.drag
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackgroundInteraction
            }
            PropertyChanges {
                target: dragHandler
                enabled: false
            }
            PropertyChanges {
                target: tapHandler
                enabled: false
            }
            PropertyChanges {
                target: mouseArea
                cursorShape: Qt.IBeamCursor
            }
        },
        State {
            name: "drag"
            when: myControl.drag
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackgroundInteraction
            }
            PropertyChanges {
                target: textInput
                color: StudioTheme.Values.themeInteraction
            }
        },
        State {
            name: "sliderDrag"
            when: myControl.sliderDrag
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackground
            }
            PropertyChanges {
                target: textInput
                color: StudioTheme.Values.themeInteraction
            }
        },
        State {
            name: "disable"
            when: !myControl.enabled
            PropertyChanges {
                target: textInputBackground
                color: StudioTheme.Values.themeControlBackgroundDisabled
            }
            PropertyChanges {
                target: textInput
                color: StudioTheme.Values.themeTextColorDisabled
            }
        }
    ]
}
