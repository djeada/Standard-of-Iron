import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: overlayRoot

    property var minimap: null
    property real paintedW: 0
    property real paintedH: 0

    readonly property bool placeable: overlayRoot.paintedW > 0 && overlayRoot.paintedH > 0
    readonly property int maxBlips: 10
    readonly property color inkShadow: "#b32c2218"
    readonly property color stoneTint: "#8c7a5e"

    function relationColor(relation) {
        switch (relation) {
        case "self":
            return Design.Theme.danger;
        case "ally":
            return Design.Theme.warning;
        case "friendly":
            return Design.Theme.success;
        default:
            return Design.Theme.textSecondary;
        }
    }

    function blipSpan(kind) {
        return kind === "capture_finished" ? Design.Metrics.space16 : Design.Metrics.space12;
    }

    function mapX(nx) {
        return ((overlayRoot.width - overlayRoot.paintedW) / 2) + nx * overlayRoot.paintedW;
    }

    function mapY(ny) {
        return ((overlayRoot.height - overlayRoot.paintedH) / 2) + ny * overlayRoot.paintedH;
    }

    anchors.fill: parent

    ListModel {
        id: blipModel
    }

    Connections {
        function onEvent_blip(nx, ny, kind, relation, lifetime_seconds) {
            while (blipModel.count >= overlayRoot.maxBlips)
                blipModel.remove(0);
            blipModel.append({
                    "nx": nx,
                    "ny": ny,
                    "kind": kind,
                    "relation": relation,
                    "lifetimeMs": Math.max(300, lifetime_seconds * 1000)
                });
        }

        target: overlayRoot.minimap
    }

    Repeater {
        id: landmarkPins

        model: overlayRoot.minimap ? overlayRoot.minimap.landmarks : []

        delegate: Item {
            id: landmarkPin

            required property var modelData

            readonly property string state_id: modelData.state || "dormant"
            readonly property bool gold_vein: (modelData.kind || "") === "gold_vein"
            readonly property color tint: gold_vein ? (state_id === "owned" ? Design.Theme.success : (state_id === "enemy" ? Design.Theme.danger : (state_id === "destroyed" ? Design.Theme.textDisabled : "#d9a83c"))) : (state_id === "cleared" ? Design.Theme.success : (state_id === "awakened" ? Design.Theme.danger : Design.Theme.textDisabled))

            property real pulse: 1

            SequentialAnimation on pulse  {
                running: landmarkPin.state_id === "awakened" && !Design.A11y.reducedMotion
                loops: Animation.Infinite

                NumberAnimation {
                    from: 1
                    to: 0.35
                    duration: 1300
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    to: 1
                    duration: 1300
                    easing.type: Easing.InOutQuad
                }
            }

            visible: overlayRoot.placeable
            x: overlayRoot.mapX(modelData.nx || 0)
            y: overlayRoot.mapY(modelData.ny || 0)
            z: 9

            Rectangle {
                x: -(width / 2) + 1
                y: -(height / 2) + 1
                width: Design.Metrics.space4 + Design.Metrics.space2
                height: Design.Metrics.space12
                radius: width / 2
                color: overlayRoot.inkShadow
            }

            Rectangle {
                id: sepulcherStone

                x: -(width / 2)
                y: -(height / 2)
                width: Design.Metrics.space4 + Design.Metrics.space2
                height: Design.Metrics.space12
                radius: width / 2
                color: overlayRoot.stoneTint
                border.width: Design.Metrics.borderThin
                border.color: landmarkPin.tint
                opacity: landmarkPin.pulse
            }

            Rectangle {
                x: -(width / 2)
                y: (sepulcherStone.height / 2) - height
                width: Design.Metrics.space8
                height: Design.Metrics.space2
                color: landmarkPin.tint
                opacity: landmarkPin.pulse
            }

            Rectangle {
                x: -(width / 2)
                y: -(Design.Metrics.space2)
                width: Design.Metrics.space4
                height: Math.max(1, Design.Metrics.borderThin)
                color: landmarkPin.tint
                opacity: landmarkPin.pulse * 0.9
            }
        }
    }

    Repeater {
        id: destinationPins

        model: overlayRoot.minimap ? overlayRoot.minimap.destinations : []

        delegate: Item {
            id: destinationPin

            required property var modelData

            readonly property real destX: overlayRoot.mapX(modelData.nx || 0)
            readonly property real destY: overlayRoot.mapY(modelData.ny || 0)
            readonly property real originX: overlayRoot.mapX(modelData.origin_nx || 0)
            readonly property real originY: overlayRoot.mapY(modelData.origin_ny || 0)
            readonly property real leash: Math.hypot(destX - originX, destY - originY)

            anchors.fill: parent
            visible: overlayRoot.placeable
            z: 11

            Rectangle {
                visible: destinationPin.leash > Design.Metrics.space4
                x: destinationPin.originX
                y: destinationPin.originY - (height / 2)
                width: destinationPin.leash
                height: Math.max(1, Design.Metrics.borderThin)
                color: Design.Theme.selection
                opacity: 0.45
                transformOrigin: Item.Left
                rotation: Math.atan2(destinationPin.destY - destinationPin.originY, destinationPin.destX - destinationPin.originX) * 180 / Math.PI
            }

            Rectangle {
                x: destinationPin.destX - (width / 2)
                y: destinationPin.destY - height
                width: Math.max(1, Design.Metrics.borderThin)
                height: Design.Metrics.space12
                color: overlayRoot.inkShadow
            }

            Rectangle {
                x: destinationPin.destX
                y: destinationPin.destY - Design.Metrics.space12
                width: Design.Metrics.space8
                height: Design.Metrics.space4
                color: Design.Theme.selection
                border.width: Math.max(1, Design.Metrics.borderThin)
                border.color: overlayRoot.inkShadow
            }

            Rectangle {
                x: destinationPin.destX - (width / 2)
                y: destinationPin.destY - (height / 2)
                width: Design.Metrics.space4
                height: Math.max(1, Design.Metrics.borderThin)
                color: Design.Theme.selection
            }
        }
    }

    Repeater {
        id: eventBlips

        model: blipModel

        delegate: Item {
            id: blip

            required property int index
            required property real nx
            required property real ny
            required property string kind
            required property string relation
            required property real lifetimeMs

            readonly property color tint: overlayRoot.relationColor(relation)
            readonly property real span: overlayRoot.blipSpan(kind)

            property real ease: 0

            function retire() {
                if (blip.index >= 0 && blip.index < blipModel.count)
                    blipModel.remove(blip.index);
            }

            visible: overlayRoot.placeable
            x: overlayRoot.mapX(nx)
            y: overlayRoot.mapY(ny)
            z: 15

            SequentialAnimation {
                running: true

                NumberAnimation {
                    target: blip
                    property: "ease"
                    from: 0
                    to: 1
                    duration: Design.A11y.reducedMotion ? Math.min(600, blip.lifetimeMs) : blip.lifetimeMs
                    easing.type: Easing.OutQuad
                }

                ScriptAction {
                    script: blip.retire()
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: (Design.A11y.reducedMotion ? blip.span : blip.span * (0.55 + 1.15 * blip.ease)) + Design.Metrics.space2
                height: width
                radius: width / 2
                color: "transparent"
                border.width: Design.Metrics.borderFocus
                border.color: overlayRoot.inkShadow
                opacity: (1 - blip.ease) * 0.75
            }

            Rectangle {
                anchors.centerIn: parent
                width: Design.A11y.reducedMotion ? blip.span : blip.span * (0.55 + 1.15 * blip.ease)
                height: width
                radius: width / 2
                color: "transparent"
                border.width: Design.Metrics.borderFocus
                border.color: blip.tint
                opacity: 1 - blip.ease
            }

            Rectangle {
                anchors.centerIn: parent
                visible: blip.kind === "capture_finished"
                width: Design.A11y.reducedMotion ? blip.span * 0.6 : blip.span * (0.3 + 0.6 * blip.ease)
                height: width
                radius: width / 2
                color: "transparent"
                border.width: Design.Metrics.borderThin
                border.color: blip.tint
                opacity: 1 - blip.ease
            }

            Rectangle {
                anchors.centerIn: parent
                visible: blip.kind === "troops_attacked" || blip.kind === "capture_finished"
                width: Design.Metrics.space4
                height: width
                radius: width / 2
                color: blip.tint
                opacity: 1 - blip.ease * 0.6
            }

            Rectangle {
                anchors.centerIn: parent
                visible: blip.kind === "structure_attacked"
                width: Design.Metrics.space4
                height: width
                color: blip.tint
                opacity: 1 - blip.ease * 0.6
            }

            Rectangle {
                anchors.centerIn: parent
                visible: blip.kind === "capture_started"
                width: Design.Metrics.space4
                height: width
                rotation: 45
                color: blip.tint
                opacity: 1 - blip.ease * 0.6
            }

            Rectangle {
                anchors.centerIn: parent
                visible: blip.kind === "capture_contested"
                width: Design.Metrics.space8
                height: Design.Metrics.borderFocus
                rotation: 45
                color: blip.tint
                opacity: 1 - blip.ease * 0.6
            }

            Rectangle {
                anchors.centerIn: parent
                visible: blip.kind === "capture_contested"
                width: Design.Metrics.space8
                height: Design.Metrics.borderFocus
                rotation: -45
                color: blip.tint
                opacity: 1 - blip.ease * 0.6
            }
        }
    }
}
