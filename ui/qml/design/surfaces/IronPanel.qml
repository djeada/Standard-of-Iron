import QtQuick 2.15
import ".." as Design

Rectangle {
    id: root

    property bool raised: false
    property string accessibleName: ""
    property bool shieldsBackground: true

    property bool translucent: false
    property bool castsShadow: true
    property int contentPadding: Design.Metrics.panelPadding
    default property alias content: contentHost.data

    color: raised ? Design.Theme.panelLeather : Design.Theme.panelIron
    opacity: translucent ? 0.93 : 1
    radius: Design.Metrics.radiusMedium
    border.width: Design.Metrics.borderThin
    border.color: raised ? Design.Theme.borderStrong : Design.Theme.borderSubtle
    Accessible.name: accessibleName

    gradient: Gradient {
        GradientStop {
            position: 0
            color: Design.Theme.sheenTop(root.color)
        }

        GradientStop {
            position: 0.35
            color: root.color
        }

        GradientStop {
            position: 1
            color: Design.Theme.sheenBottom(root.color)
        }
    }

    Design.IronShadow {
        cornerRadius: root.radius
        strength: root.castsShadow ? (root.raised ? 1 : 0.7) : 0
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.radius
        anchors.rightMargin: root.radius
        anchors.topMargin: Design.Metrics.borderThin
        height: Design.Metrics.borderThin
        color: root.border.color
        opacity: 0.45
    }

    MouseArea {
        id: backgroundShield

        anchors.fill: parent
        enabled: root.shieldsBackground
        visible: enabled
        acceptedButtons: Qt.AllButtons
        hoverEnabled: enabled
        onWheel: function (wheel) {
            wheel.accepted = true;
        }
    }

    Item {
        id: contentHost

        anchors.fill: parent
        anchors.margins: root.contentPadding
    }
}
