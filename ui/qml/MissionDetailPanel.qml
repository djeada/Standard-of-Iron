import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var mission_data: null
    property string campaign_id: ""

    signal start_mission_clicked()

    radius: Theme.radiusMedium
    color: Theme.panelBase
    border.color: Theme.panelBr
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMedium
        spacing: Theme.spacingLarge

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingSmall

            Label {
                text: mission_data ? mission_data.mission_id : ""
                color: Theme.textMain
                font.pointSize: Theme.fontSizeTitle
                font.bold: true
            }

            Label {
                text: mission_data && mission_data.intro_text ? mission_data.intro_text : ""
                color: Theme.textSubLite
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.pointSize: Theme.fontSizeMedium
            }

            RowLayout {
                spacing: Theme.spacingMedium

                ColumnLayout {
                    spacing: Theme.spacingTiny

                    Label {
                        text: qsTr("Primary Objective:")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Label {
                        text: qsTr("• Complete the mission successfully")
                        color: Theme.textSubLite
                        font.pointSize: Theme.fontSizeSmall
                    }

                }

                Item {
                    Layout.fillWidth: true
                }

                ColumnLayout {
                    spacing: Theme.spacingTiny

                    Label {
                        text: qsTr("Stats:")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    RowLayout {
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Attempts: -")
                            color: Theme.textSubLite
                            font.pointSize: Theme.fontSizeSmall
                        }

                        Label {
                            text: qsTr("Best Time: -")
                            color: Theme.textSubLite
                            font.pointSize: Theme.fontSizeSmall
                        }

                    }

                }

            }

        }

        ColumnLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingMedium

            StyledButton {
                text: mission_data && mission_data.completed ? qsTr("Replay Mission") : qsTr("Start Mission")
                enabled: mission_data && mission_data.unlocked
                onClicked: root.start_mission_clicked()
                ToolTip.visible: !enabled && hovered
                ToolTip.text: {
                    if (!mission_data)
                        return "";

                    if (!mission_data.unlocked)
                        return qsTr("Complete previous missions to unlock");

                    return "";
                }
                ToolTip.delay: 500
            }

            Label {
                visible: mission_data && !mission_data.unlocked
                text: qsTr("🔒 Locked")
                color: Theme.textDim
                font.pointSize: Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                visible: mission_data && mission_data.completed
                text: qsTr("✓ Completed")
                color: Theme.successText
                font.pointSize: Theme.fontSizeSmall
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }

        }

    }

}
