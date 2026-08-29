import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

HintCard {
    id: root

    signal open_settings_requested

    readonly property var entries: CameraGuide.entries

    hintId: "camera_legend"
    title: qsTr("Moving the camera")
    iconText: Design.Icons.follow
    closeTooltip: qsTr("Hide the camera legend")

    implicitWidth: Design.Metrics.space24 * 17

    Repeater {
        model: root.entries

        delegate: RowLayout {
            required property var modelData

            Layout.fillWidth: true
            spacing: Design.Metrics.space8
            opacity: modelData.muted ? 0.55 : 1.0

            Text {
                Layout.fillWidth: true
                text: modelData.name
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                elide: Text.ElideRight
            }

            Text {
                text: modelData.state.length > 0 ? modelData.compact + " · " + modelData.state : modelData.compact
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Design.IronDivider {
        Layout.fillWidth: true
    }

    Design.IronButton {
        Layout.fillWidth: true
        text: qsTr("Camera settings")
        tone: "secondary"
        onClicked: root.open_settings_requested()
    }
}
