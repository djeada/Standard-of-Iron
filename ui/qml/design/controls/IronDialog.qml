import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Dialog {
    id: root

    modal: true
    focus: true
    padding: Design.Metrics.space16
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: Design.Theme.backgroundRaised
        radius: Design.Metrics.radiusLarge
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderStrong
    }
    Overlay.modal: Rectangle { color: "#99000000" }
}
