import QtQuick 2.15
import ".." as Design
import "../surfaces" as Surfaces

Surfaces.IronPanel {
    id: root

    property string unitName: ""
    property string subtitle: ""
    property real health: 1.0
    implicitWidth: 180
    implicitHeight: 76
    accessibleName: unitName
    Column {
        anchors.fill: parent
        spacing: Design.Metrics.space4
        Text {
            text: root.unitName
            color: Design.Theme.textPrimary
            font.pixelSize: Design.Typography.label
            font.weight: Design.Typography.medium
        }
        Text {
            text: root.subtitle
            color: Design.Theme.textSecondary
            font.pixelSize: Design.Typography.caption
        }
        IronProgressBar { width: parent.width; value: root.health }
    }
}
