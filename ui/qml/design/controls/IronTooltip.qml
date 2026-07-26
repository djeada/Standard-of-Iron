import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ToolTip {
    id: control

    delay: Design.Metrics.tooltipDelay
    timeout: 8000
    padding: Design.Metrics.space8
    contentItem: Text {
        text: control.text
        color: Design.Theme.textPrimary
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.caption
        wrapMode: Text.WordWrap
        width: Math.min(implicitWidth, Design.Metrics.tooltipWidth)
    }
    background: Rectangle {
        color: Design.Theme.panelLeather
        radius: Design.Metrics.radiusSmall
        border.color: Design.Theme.borderStrong
    }
}
