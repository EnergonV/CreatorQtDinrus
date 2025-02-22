// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

import QtQuick 6.0
import QtQuick3D 6.0
import MouseArea3D 1.0

Node {
    id: scaleGizmo

    property View3D view3D
    property bool highlightOnHover: false
    property Node targetNode: null
    readonly property bool dragging: scaleRodX.dragging || scaleRodY.dragging || scaleRodZ.dragging
                                     || planeX.dragging || planeY.dragging || planeZ.dragging
                                     || centerMouseArea.dragging
    property MouseArea3D dragHelper: null
    property alias freeDraggerArea: centerMouseArea

    position: dragHelper.pivotScenePosition(targetNode)

    onTargetNodeChanged: position = dragHelper.pivotScenePosition(targetNode)

    Connections {
        target: scaleGizmo.targetNode
        function onSceneTransformChanged()
        {
            scaleGizmo.position = scaleGizmo.dragHelper.pivotScenePosition(scaleGizmo.targetNode);
        }
    }

    signal scaleCommit()
    signal scaleChange()

    Node {
        rotation: !targetNode ? Qt.quaternion(1, 0, 0, 0) : targetNode.sceneRotation

        ScaleRod {
            id: scaleRodX
            eulerRotation: Qt.vector3d(0, 0, -90)
            axis: Qt.vector3d(1, 0, 0)
            targetNode: scaleGizmo.targetNode
            color: highlightOnHover && (hovering || dragging) ? Qt.lighter(Qt.rgba(1, 0, 0, 1))
                                                              : Qt.rgba(1, 0, 0, 1)
            view3D: scaleGizmo.view3D
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            onScaleCommit: scaleGizmo.scaleCommit()
            onScaleChange: scaleGizmo.scaleChange()
        }

        ScaleRod {
            id: scaleRodY
            eulerRotation: Qt.vector3d(0, 0, 0)
            axis: Qt.vector3d(0, 1, 0)
            targetNode: scaleGizmo.targetNode
            color: highlightOnHover && (hovering || dragging) ? Qt.lighter(Qt.rgba(0, 0.6, 0, 1))
                                                              : Qt.rgba(0, 0.6, 0, 1)
            view3D: scaleGizmo.view3D
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            onScaleCommit: scaleGizmo.scaleCommit()
            onScaleChange: scaleGizmo.scaleChange()
        }

        ScaleRod {
            id: scaleRodZ
            eulerRotation: Qt.vector3d(90, 0, 0)
            axis: Qt.vector3d(0, 0, 1)
            targetNode: scaleGizmo.targetNode
            color: highlightOnHover && (hovering || dragging) ? Qt.lighter(Qt.rgba(0, 0, 1, 1))
                                                              : Qt.rgba(0, 0, 1, 1)
            view3D: scaleGizmo.view3D
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            onScaleCommit: scaleGizmo.scaleCommit()
            onScaleChange: scaleGizmo.scaleChange()
        }

        PlanarScaleHandle {
            id: planeX

            y: 10
            z: 10

            eulerRotation: Qt.vector3d(0, 90, 0)
            axisX: Qt.vector3d(0, 0, -1)
            axisY: Qt.vector3d(0, 1, 0)
            targetNode: scaleGizmo.targetNode
            color: highlightOnHover && (hovering || dragging) ? Qt.lighter(Qt.rgba(1, 0, 0, 1))
                                                              : Qt.rgba(1, 0, 0, 1)
            view3D: scaleGizmo.view3D
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            onScaleCommit: scaleGizmo.scaleCommit()
            onScaleChange: scaleGizmo.scaleChange()
        }

        PlanarScaleHandle {
            id: planeY

            x: 10
            z: 10

            eulerRotation: Qt.vector3d(90, 0, 0)
            axisX: Qt.vector3d(1, 0, 0)
            axisY: Qt.vector3d(0, 0, 1)
            targetNode: scaleGizmo.targetNode
            color: highlightOnHover && (hovering || dragging) ? Qt.lighter(Qt.rgba(0, 0.6, 0, 1))
                                                              : Qt.rgba(0, 0.6, 0, 1)
            view3D: scaleGizmo.view3D
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            onScaleCommit: scaleGizmo.scaleCommit()
            onScaleChange: scaleGizmo.scaleChange()
        }

        PlanarScaleHandle {
            id: planeZ

            x: 10
            y: 10

            eulerRotation: Qt.vector3d(0, 0, 0)
            axisX: Qt.vector3d(1, 0, 0)
            axisY: Qt.vector3d(0, 1, 0)
            targetNode: scaleGizmo.targetNode
            color: highlightOnHover && (hovering || dragging) ? Qt.lighter(Qt.rgba(0, 0, 1, 1))
                                                              : Qt.rgba(0, 0, 1, 1)
            view3D: scaleGizmo.view3D
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            onScaleCommit: scaleGizmo.scaleCommit()
            onScaleChange: scaleGizmo.scaleChange()
        }
    }

    Model {
        id: centerCube

        readonly property bool _edit3dLocked: true // Make this non-pickable
        source: "#Cube"
        scale: Qt.vector3d(0.024, 0.024, 0.024)
        materials: DefaultMaterial {
            id: material
            diffuseColor: highlightOnHover
                           && (centerMouseArea.hovering || centerMouseArea.dragging)
                           ? Qt.lighter(Qt.rgba(0.5, 0.5, 0.5, 1))
                           : Qt.rgba(0.5, 0.5, 0.5, 1)
            lighting: DefaultMaterial.NoLighting
        }

        MouseArea3D {
            id: centerMouseArea
            view3D: scaleGizmo.view3D
            x: -60
            y: -60
            width: 120
            height: 120
            rotation: view3D.camera.rotation
            grabsMouse: scaleGizmo.targetNode
            priority: 10
            active: scaleGizmo.visible
            dragHelper: scaleGizmo.dragHelper

            property vector3d _startScale
            property point _startScreenPos

            function localScale(screenPos)
            {
                var yDelta = screenPos.y - _startScreenPos.y;
                if (yDelta === 0)
                    return _startScale;
                var scaler = 1.0 + (yDelta * 0.025);
                if (scaler === 0)
                    scaler = 0.0001;
                return Qt.vector3d(scaler * _startScale.x,
                                   scaler * _startScale.y,
                                   scaler * _startScale.z);
            }

            onPressed: (planePos, screenPos)=> {
                if (!scaleGizmo.targetNode)
                    return;

                if (targetNode == multiSelectionNode)
                    _generalHelper.restartMultiSelection();

                // Recreate vector so we don't follow the changes in targetNode.scale
                _startScale = Qt.vector3d(scaleGizmo.targetNode.scale.x,
                                          scaleGizmo.targetNode.scale.y,
                                          scaleGizmo.targetNode.scale.z);
                _startScreenPos = screenPos;
            }
            onDragged: (planePos, screenPos)=> {
                if (!scaleGizmo.targetNode)
                    return;
                scaleGizmo.targetNode.scale = localScale(screenPos);
                if (targetNode == multiSelectionNode)
                    _generalHelper.scaleMultiSelection(false);
                scaleGizmo.scaleChange();
            }
            onReleased: (planePos, screenPos)=> {
                if (!scaleGizmo.targetNode)
                    return;

                scaleGizmo.targetNode.scale = localScale(screenPos);
                if (targetNode == multiSelectionNode)
                    _generalHelper.scaleMultiSelection(true);
                scaleGizmo.scaleCommit();
            }
        }
    }
}
