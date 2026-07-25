import QtQuick 2.15
import ".." as Design

Row {
    id: root

    property string iconText: ""
    property int amount: 0
    property int trend: 0
    spacing: Design.Metrics.space4
    Accessible.name: iconText + " " + amount
    Text { text: root.iconText; color: Design.Theme.accent; font.pixelSize: Design.Typography.body }
    Text { text: root.amount; color: Design.Theme.textPrimary; font.pixelSize: Design.Typography.body; font.bold: true }
    Text {
        text: root.trend === 0 ? "" : (root.trend > 0 ? "+" : "") + root.trend
        color: root.trend >= 0 ? Design.Theme.success : Design.Theme.danger
        font.pixelSize: Design.Typography.caption
    }
}
