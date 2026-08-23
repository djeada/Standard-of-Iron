import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: meter

    property real value: 0.0
    property real ghostValue: -1.0
    property color fillColor: Design.Theme.danger
    property color troughColor: Design.Theme.backgroundDeep
    property color frameColor: Design.Theme.borderStrong
    property real frameOpacity: 0.5
    property int segments: 0
    property bool crest: true
    property bool starved: false
    property int fillDuration: Design.Motion.fast
    property int ghostDuration: Design.Motion.reducedMotion ? 0 : 520

    readonly property real clampedValue: Math.max(0.0, Math.min(1.0, value))
    readonly property real clampedGhost: Math.max(clampedValue, Math.min(1.0, ghostValue))

    implicitHeight: Math.max(6, Design.Metrics.space8)

    function _alpha(base, a) {
        return Qt.rgba(base.r, base.g, base.b, a);
    }

    Rectangle {
        anchors.fill: parent
        radius: Design.Metrics.radiusSmall
        color: meter._alpha(meter.troughColor, 0.94)
        border.width: Design.Metrics.borderThin
        border.color: meter._alpha(meter.frameColor, meter.frameOpacity)
    }

    Item {
        id: channel

        anchors.fill: parent
        anchors.margins: Math.max(1, Math.round(meter.height * 0.14))
        clip: true

        Rectangle {
            visible: meter.ghostValue >= 0.0 && width > 1
            width: channel.width * meter.clampedGhost
            height: channel.height
            color: meter._alpha(meter.fillColor, 0.3)

            Behavior on width  {
                NumberAnimation {
                    duration: meter.ghostDuration
                    easing.type: Design.Motion.exitEasing
                }
            }
        }

        Rectangle {
            id: fill

            width: channel.width * meter.clampedValue
            height: channel.height
            color: meter.fillColor
            opacity: meter.starved ? 0.55 : 1.0

            Behavior on width  {
                NumberAnimation {
                    duration: meter.fillDuration
                    easing.type: Design.Motion.standardEasing
                }
            }

            Behavior on color  {
                ColorAnimation {
                    duration: Design.Motion.normal
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.max(1, Math.round(parent.height * 0.22))
                visible: meter.crest && parent.width > 2
                color: Qt.lighter(meter.fillColor, 1.32)
                opacity: 0.32
            }

            Rectangle {
                anchors.right: parent.right
                width: Math.max(1, Math.round(meter.height * 0.1))
                height: parent.height
                visible: parent.width > 2
                color: Qt.lighter(meter.fillColor, 1.75)
                opacity: 0.6
            }
        }

        Repeater {
            model: Math.max(0, meter.segments - 1)

            delegate: Rectangle {
                required property int index

                x: Math.round(channel.width * (index + 1) / meter.segments)
                width: Design.Metrics.borderThin
                height: channel.height
                color: Qt.rgba(0, 0, 0, 0.62)
            }
        }
    }
}
