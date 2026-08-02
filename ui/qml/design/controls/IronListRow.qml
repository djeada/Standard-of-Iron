import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ItemDelegate {
    id: control

    implicitHeight: 42
    hoverEnabled: true

    Connections {
        function onClicked() {
            Design.UiSound.activate();
        }

        function onHoveredChanged() {
            if (control.hovered && control.enabled)
                Design.UiSound.hover();
        }

        target: control
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        color: control.checked ? Design.Theme.selection : control.hovered ? Design.Theme.panelLeather : Design.Theme.panelIron
        border.width: control.activeFocus ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.activeFocus ? Design.Theme.focus : Design.Theme.borderSubtle
    }
}
