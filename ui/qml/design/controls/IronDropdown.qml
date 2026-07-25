import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ComboBox {
    id: control

    implicitHeight: Design.Metrics.controlHeight
    contentItem: Text {
        text: control.displayText
        color: control.enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        verticalAlignment: Text.AlignVCenter
        leftPadding: Design.Metrics.space8
    }
    background: Rectangle {
        color: Design.Theme.panelIron
        radius: Design.Metrics.radiusSmall
        border.width: control.activeFocus ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.activeFocus ? Design.Theme.focus :
                      control.hovered ? Design.Theme.accent : Design.Theme.borderSubtle
    }
}
