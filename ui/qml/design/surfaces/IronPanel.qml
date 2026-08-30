import QtQuick 2.15
import ".." as Design

Rectangle {
    id: root

    property bool raised: false
    property string accessibleName: ""

    property bool translucent: false
    property int contentPadding: Design.Metrics.panelPadding
    default property alias content: contentHost.data

    color: raised ? Design.Theme.panelLeather : Design.Theme.panelIron
    opacity: translucent ? 0.93 : 1
    radius: Design.Metrics.radiusMedium
    border.width: Design.Metrics.borderThin
    border.color: raised ? Design.Theme.borderStrong : Design.Theme.borderSubtle
    Accessible.name: accessibleName

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

    Item {
        id: contentHost

        anchors.fill: parent
        anchors.margins: root.contentPadding
    }
}
