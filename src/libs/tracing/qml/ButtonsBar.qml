// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QtCreator.Tracing

ToolBar {
    id: buttons

    signal jumpToPrev()
    signal jumpToNext()
    signal zoomControlChanged()
    signal rangeSelectChanged()
    signal lockChanged()

    function updateLockButton(locked: bool) {
        lockButton.checked = !locked;
    }

    function lockButtonChecked(): bool {
        return lockButton.checked;
    }

    function updateRangeButton(rangeMode: bool) {
        rangeButton.checked = rangeMode;
    }

    function rangeButtonChecked() : bool {
        return rangeButton.checked
    }

    background: Rectangle {
        anchors.fill: parent
        color: Theme.color(Theme.PanelStatusBarBackgroundColor)
    }


    RowLayout {
        spacing: 0
        anchors.fill: parent

        ImageToolButton {
            id: jumpToPrevButton
            Layout.fillHeight: true

            imageSource: "image://icons/prev"
            ToolTip.text: qsTr("Jump to previous event.")
            onClicked: buttons.jumpToPrev()
        }

        ImageToolButton {
            id: jumpToNextButton
            Layout.fillHeight: true

            imageSource: "image://icons/next"
            ToolTip.text: qsTr("Jump to next event.")
            onClicked: buttons.jumpToNext()
        }

        ImageToolButton {
            id: zoomControlButton
            Layout.fillHeight: true

            imageSource: "image://icons/zoom"
            ToolTip.text: qsTr("Show zoom slider.")
            checkable: true
            checked: false
            onCheckedChanged: buttons.zoomControlChanged()
        }

        ImageToolButton {
            id: rangeButton
            Layout.fillHeight: true

            imageSource: "image://icons/" + (checked ? "rangeselected" : "rangeselection");
            ToolTip.text: qsTr("Select range.")
            checkable: true
            checked: false
            onCheckedChanged: buttons.rangeSelectChanged()
        }

        ImageToolButton {
            id: lockButton
            Layout.fillHeight: true

            imageSource: "image://icons/selectionmode"
            ToolTip.text: qsTr("View event information on mouseover.")
            checkable: true
            checked: false
            onCheckedChanged: buttons.lockChanged()
        }
    }
}
