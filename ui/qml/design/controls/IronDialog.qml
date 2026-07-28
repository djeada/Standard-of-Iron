import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Dialog {
    id: root

    property string tone: "info"
    property string message: ""
    property string primaryAction: qsTr("OK")
    property string secondaryAction: ""

    readonly property color toneColor: tone === "danger" ? Design.Theme.danger : tone === "warning" ? Design.Theme.warning : Design.Theme.accent
    readonly property string toneGlyph: tone === "info" ? Design.Icons.objective : Design.Icons.warning

    signal primaryActivated
    signal secondaryActivated

    modal: true
    focus: true
    padding: Design.Metrics.space16
    closePolicy: Popup.CloseOnEscape
    implicitWidth: Design.Metrics.space24 * 20

    background: Rectangle {
        color: Design.Theme.backgroundRaised
        radius: Design.Metrics.radiusLarge
        border.width: Design.Metrics.borderFocus
        border.color: root.toneColor
        Accessible.role: Accessible.Dialog
        Accessible.name: root.title
        Accessible.description: root.message
    }

    Overlay.modal: Rectangle {
        color: Design.Theme.backgroundDeep
        opacity: 0.82
    }

    header: Item {
        implicitHeight: Design.Metrics.space24 * 2

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Design.Metrics.space16
            anchors.rightMargin: Design.Metrics.space16
            spacing: Design.Metrics.space8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.toneGlyph
                color: root.toneColor
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.heading
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                color: Design.Theme.textPrimary
                font.family: Design.Typography.displayFamily
                font.pixelSize: Design.Typography.heading
                font.weight: Design.Typography.bold
            }
        }

        Design.IronDivider {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    contentItem: Text {
        text: root.message
        color: Design.Theme.textSecondary
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.body
        wrapMode: Text.WordWrap
    }

    footer: Item {
        implicitHeight: Design.Metrics.space24 * 2

        Design.IronDivider {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: Design.Metrics.space16
            spacing: Design.Metrics.space8

            Design.IronButton {
                visible: root.secondaryAction !== ""
                text: root.secondaryAction
                onClicked: {
                    root.secondaryActivated();
                    root.close();
                }
            }

            Design.IronButton {
                text: root.primaryAction
                tone: "primary"
                onClicked: {
                    root.primaryActivated();
                    root.close();
                }
            }
        }
    }
}
