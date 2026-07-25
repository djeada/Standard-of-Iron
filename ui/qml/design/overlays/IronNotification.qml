import QtQuick 2.15
import ".." as Design
import "../surfaces" as Surfaces

Surfaces.IronPanel {
    id: root

    property string priority: "informational"
    property string message: ""
    property int count: 1
    implicitWidth: 360
    implicitHeight: label.implicitHeight + Design.Metrics.space24
    border.color: priority === "critical" ? Design.Theme.danger : priority === "urgent" ? Design.Theme.warning : Design.Theme.borderStrong
    Text {
        id: label
        anchors.fill: parent
        text: root.message + (root.count > 1 ? "  \u00D7" + root.count : "")
        color: Design.Theme.textPrimary
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.body
        wrapMode: Text.WordWrap
    }
}
