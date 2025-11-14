import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron.UI 1.0

Item {
    id: root

    signal missionSelected(string campaignId)
    signal cancelled()

    property var campaigns: (typeof game !== "undefined" && game.available_campaigns) ? game.available_campaigns : []

    anchors.fill: parent
    focus: true

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.85, 1200)
        height: Math.min(parent.height * 0.85, 800)
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: Theme.panelBr
        border.width: 1
        opacity: 0.98

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingXLarge
            spacing: Theme.spacingLarge

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: qsTr("Campaign Missions")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: qsTr("← Back")
                    onClicked: root.cancelled()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            // Mission list
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: listView

                    model: root.campaigns
                    spacing: Theme.spacingMedium

                    delegate: Rectangle {
                        width: listView.width
                        height: 120
                        radius: Theme.radiusLarge
                        color: mouseArea.containsMouse ? Theme.hoverBg : Theme.cardBase
                        border.color: mouseArea.containsMouse ? Theme.selectedBr : Theme.cardBorder
                        border.width: 1

                        MouseArea {
                            id: mouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                missionDetailPanel.visible = true;
                                missionDetailPanel.campaignData = modelData;
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMedium
                            spacing: Theme.spacingMedium

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Label {
                                        text: modelData.title || ""
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeTitle
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }

                                    Rectangle {
                                        visible: modelData.completed || false
                                        Layout.preferredWidth: 100
                                        Layout.preferredHeight: 24
                                        radius: Theme.radiusSmall
                                        color: Theme.successBg
                                        border.color: Theme.successBr
                                        border.width: 1

                                        Label {
                                            anchors.centerIn: parent
                                            text: qsTr("✓ Completed")
                                            color: Theme.successText
                                            font.pointSize: Theme.fontSizeSmall
                                            font.bold: true
                                        }
                                    }

                                    Rectangle {
                                        visible: !(modelData.unlocked || false)
                                        Layout.preferredWidth: 80
                                        Layout.preferredHeight: 24
                                        radius: Theme.radiusSmall
                                        color: Theme.disabledBg
                                        border.color: Theme.border
                                        border.width: 1

                                        Label {
                                            anchors.centerIn: parent
                                            text: qsTr("🔒 Locked")
                                            color: Theme.textDim
                                            font.pointSize: Theme.fontSizeSmall
                                        }
                                    }
                                }

                                Label {
                                    text: modelData.description || ""
                                    color: Theme.textSubLite
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    font.pointSize: Theme.fontSizeMedium
                                }
                            }

                            Text {
                                text: "›"
                                font.pointSize: Theme.fontSizeHero
                                color: Theme.textHint
                            }
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.animNormal
                            }
                        }

                        Behavior on border.color {
                            ColorAnimation {
                                duration: Theme.animNormal
                            }
                        }
                    }
                }
            }

            // Empty state
            Label {
                visible: root.campaigns.length === 0
                text: qsTr("No campaign missions available")
                color: Theme.textDim
                font.pointSize: Theme.fontSizeMedium
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    // Mission detail panel
    Rectangle {
        id: missionDetailPanel

        property var campaignData: null

        visible: false
        anchors.fill: parent
        color: Theme.dim
        z: 100

        MouseArea {
            anchors.fill: parent
            onClicked: missionDetailPanel.visible = false
        }

        Rectangle {
            width: Math.min(parent.width * 0.7, 900)
            height: Math.min(parent.height * 0.8, 700)
            anchors.centerIn: parent
            radius: Theme.radiusPanel
            color: Theme.panelBase
            border.color: Theme.panelBr
            border.width: 2

            MouseArea {
                anchors.fill: parent
                onClicked: {} // Prevent click-through
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingXLarge
                spacing: Theme.spacingLarge

                // Title
                Label {
                    text: missionDetailPanel.campaignData ? (missionDetailPanel.campaignData.title || "") : ""
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                // Description
                Label {
                    text: missionDetailPanel.campaignData ? (missionDetailPanel.campaignData.description || "") : ""
                    color: Theme.textSubLite
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pointSize: Theme.fontSizeMedium
                }

                // Black placeholder scene
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#000000"
                    border.color: Theme.border
                    border.width: 1
                    radius: Theme.radiusMedium

                    Label {
                        anchors.centerIn: parent
                        text: qsTr("Mission Preview\n(Coming Soon)")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeLarge
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // Buttons
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMedium

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledButton {
                        text: qsTr("Cancel")
                        onClicked: missionDetailPanel.visible = false
                    }

                    StyledButton {
                        text: qsTr("Start Mission")
                        enabled: missionDetailPanel.campaignData ? (missionDetailPanel.campaignData.unlocked || false) : false
                        onClicked: {
                            if (missionDetailPanel.campaignData) {
                                root.missionSelected(missionDetailPanel.campaignData.id);
                            }
                        }
                    }
                }
            }
        }
    }
}
