import QtQuick 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Design.IronPanel {
    id: root

    signal dismissed
    signal open_settings_requested

    readonly property var entries: CameraGuide.entries

    implicitWidth: Design.Metrics.space24 * 17
    implicitHeight: layout.implicitHeight + Design.Metrics.space16 * 2
    raised: true
    accessibleName: qsTr("Camera controls")

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Design.Metrics.space16
        spacing: Design.Metrics.space8

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.Metrics.space8

            Text {
                text: Design.Icons.follow
                color: Design.Theme.accent
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.body
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Moving the camera")
                color: Design.Theme.textPrimary
                font.family: Design.Typography.displayFamily
                font.pixelSize: Design.Typography.heading
                font.weight: Design.Typography.bold
                elide: Text.ElideRight
            }

            Design.IronIconButton {
                iconText: Design.Icons.close
                tooltip: qsTr("Hide the camera legend")
                onClicked: root.dismissed()
            }
        }

        Design.IronDivider {
            Layout.fillWidth: true
        }

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
}
