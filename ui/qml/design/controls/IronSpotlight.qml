import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property bool active: false
    property color tone: Design.Theme.focus
    property real inset: -Design.Metrics.space4
    property real cornerRadius: Design.Metrics.radiusMedium
    property bool halo: true

    property Item target: root.parent

    readonly property bool animating: root.active && !Design.A11y.reducedMotion

    anchors.fill: root.target
    anchors.margins: root.inset
    visible: root.active
    z: 40

    Accessible.ignored: true

    property real pulse: 1

    SequentialAnimation on pulse  {
        running: root.animating
        loops: Animation.Infinite

        NumberAnimation {
            from: 1
            to: 0.35
            duration: 900
            easing.type: Easing.InOutQuad
        }

        NumberAnimation {
            to: 1
            duration: 900
            easing.type: Easing.InOutQuad
        }
    }

    onAnimatingChanged: {
        if (!root.animating)
            root.pulse = 1;
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius + Math.abs(root.inset)
        color: "transparent"
        border.width: Design.Metrics.borderFocus
        border.color: root.tone
        opacity: root.pulse
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Design.Metrics.space4
        visible: root.halo && root.animating
        radius: root.cornerRadius + Math.abs(root.inset) + Design.Metrics.space4
        color: "transparent"
        border.width: Design.Metrics.borderThin
        border.color: root.tone
        opacity: (1 - root.pulse) * 0.7
    }
}
