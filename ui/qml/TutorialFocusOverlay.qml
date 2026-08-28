import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var projector: null
    property var points: []
    property real topInset: 0
    property real bottomInset: 0

    readonly property bool hasCamera: root.projector !== null && root.projector.ready

    anchors.fill: parent
    visible: root.hasCamera && root.points.length > 0

    Accessible.ignored: true

    property real pulse: 1

    SequentialAnimation on pulse  {
        running: root.visible && !Design.A11y.reducedMotion
        loops: Animation.Infinite

        NumberAnimation {
            from: 1
            to: 0.3
            duration: 900
            easing.type: Easing.InOutQuad
        }

        NumberAnimation {
            to: 1
            duration: 900
            easing.type: Easing.InOutQuad
        }
    }

    Repeater {
        model: root.points

        delegate: Item {
            id: marker

            required property var modelData

            readonly property var projection: {
                if (!root.hasCamera)
                    return null;
                root.projector.tick;
                return root.projector.project(modelData.world_x || 0, 0, modelData.world_z || 0);
            }
            readonly property bool projected: projection !== null
            readonly property real screenX: projected ? projection.x : 0
            readonly property real screenY: projected ? projection.y : 0

            width: Design.Metrics.space24 * 2
            height: width
            x: screenX - width / 2
            y: screenY - height / 2
            visible: projected && screenX >= 0 && screenX <= root.width && screenY >= root.topInset && screenY <= root.height - root.bottomInset

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.width: Design.Metrics.borderFocus
                border.color: Design.Theme.focus
                opacity: root.pulse * 0.9
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * (Design.A11y.reducedMotion ? 0.6 : (0.45 + (1 - root.pulse) * 0.55))
                height: width
                radius: width / 2
                color: "transparent"
                border.width: Design.Metrics.borderThin
                border.color: Design.Theme.focus
                opacity: Design.A11y.reducedMotion ? 0.6 : root.pulse * 0.5
            }
        }
    }
}
