import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ToolTip {
    id: control

    delay: Design.Metrics.tooltipDelay
    timeout: 8000
    padding: Design.Metrics.space8
    implicitWidth: Math.min(tipText.implicitWidth + control.leftPadding + control.rightPadding, Design.Metrics.tooltipWidth)
    implicitHeight: tipText.contentHeight + control.topPadding + control.bottomPadding
    contentItem: Text {
        id: tipText

        text: control.text
        color: Design.Theme.textPrimary
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.caption
        wrapMode: Text.WordWrap
        width: control.availableWidth
    }
    background: Rectangle {
        color: Design.Theme.panelLeather
        radius: Design.Metrics.radiusSmall
        border.color: Design.Theme.borderStrong
    }
}
