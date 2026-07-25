import QtQuick 2.15
import ".." as Design

Rectangle {
    id: root

    property bool raised: false
    property string accessibleName: ""
    default property alias content: contentHost.data

    color: raised ? Design.Theme.backgroundRaised : Design.Theme.panelIron
    radius: Design.Metrics.radiusMedium
    border.width: Design.Metrics.borderThin
    border.color: raised ? Design.Theme.borderStrong : Design.Theme.borderSubtle
    Accessible.name: accessibleName

    Item {
        id: contentHost

        anchors.fill: parent
        anchors.margins: Design.Metrics.space12
    }
}
