import QtQuick 2.15
import ".." as Design

Item {
    id: root

    property real cornerRadius: Design.Metrics.radiusMedium
    property real spread: Design.Metrics.space8
    property real lift: Design.Metrics.space2
    property real strength: 1

    anchors.fill: parent
    z: -1
    visible: Design.Theme.shadowStrength > 0 && root.strength > 0

    Accessible.ignored: true

    Repeater {
        model: 4

        Rectangle {
            required property int index

            readonly property real grow: root.spread * (index + 1) / 4

            x: -grow
            y: -grow + root.lift
            width: root.width + grow * 2
            height: root.height + grow * 2
            radius: root.cornerRadius + grow
            color: Design.Theme.backgroundDeep
            opacity: 0.14 * root.strength * Design.Theme.shadowStrength * (1 - index * 0.2)
        }
    }
}
