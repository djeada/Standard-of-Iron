import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property var mission_objectives: null

    signal closeRequested()

    function refreshObjectives() {
        if (typeof game !== 'undefined' && game.get_current_mission_objectives)
            mission_objectives = game.get_current_mission_objectives();
        else
            mission_objectives = null;
    }

    anchors.fill: parent
    z: 10
    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.closeRequested();
            event.accepted = true;
        }
    }
    Component.onCompleted: {
        refreshObjectives();
    }
    onVisibleChanged: {
        if (visible)
            refreshObjectives();

    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.7, 800)
        height: Math.min(parent.height * 0.8, 600)
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

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Label {
                    text: qsTr("Mission Objectives")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: "×"
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    onClicked: root.closeRequested()
                }

            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            ScrollView {
                id: objectivesScroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: objectivesScroll.availableWidth

                ColumnLayout {
                    width: objectivesScroll.availableWidth
                    spacing: Theme.spacingLarge

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall
                        visible: !!(mission_objectives && mission_objectives.title)

                        Label {
                            text: mission_objectives && mission_objectives.title ? mission_objectives.title : ""
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeTitle
                            font.bold: true
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Label {
                            text: mission_objectives && mission_objectives.summary ? mission_objectives.summary : ""
                            color: Theme.textSubLite
                            font.pointSize: Theme.fontSizeMedium
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium
                        visible: !!(mission_objectives && mission_objectives.victory_conditions && mission_objectives.victory_conditions.length > 0)

                        Label {
                            text: qsTr("Victory Conditions")
                            color: Theme.successText
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Repeater {
                            model: mission_objectives && mission_objectives.victory_conditions ? mission_objectives.victory_conditions : []

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: victory_content.implicitHeight + Theme.spacingMedium * 2
                                radius: Theme.radiusMedium
                                color: Theme.successBg
                                border.color: Theme.successBr
                                border.width: 1

                                RowLayout {
                                    id: victory_content

                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingMedium
                                    spacing: Theme.spacingMedium

                                    Label {
                                        text: "✓"
                                        color: Theme.successText
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.alignment: Qt.AlignTop
                                    }

                                    Label {
                                        text: modelData.description || qsTr("Complete the objective")
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeMedium
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                }

                            }

                        }

                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium
                        visible: !!(mission_objectives && mission_objectives.defeat_conditions && mission_objectives.defeat_conditions.length > 0)

                        Label {
                            text: qsTr("Defeat Conditions")
                            color: Theme.removeColor
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Repeater {
                            model: mission_objectives && mission_objectives.defeat_conditions ? mission_objectives.defeat_conditions : []

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: defeat_content.implicitHeight + Theme.spacingMedium * 2
                                radius: Theme.radiusMedium
                                color: Theme.dangerBg
                                border.color: Theme.dangerBr
                                border.width: 1

                                RowLayout {
                                    id: defeat_content

                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingMedium
                                    spacing: Theme.spacingMedium

                                    Label {
                                        text: "✗"
                                        color: Theme.removeColor
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.alignment: Qt.AlignTop
                                    }

                                    Label {
                                        text: modelData.description || qsTr("Avoid this condition")
                                        color: Theme.removeColor
                                        font.pointSize: Theme.fontSizeMedium
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                }

                            }

                        }

                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMedium
                        visible: !!(mission_objectives && mission_objectives.optional_objectives && mission_objectives.optional_objectives.length > 0)

                        Label {
                            text: qsTr("Optional Objectives")
                            color: Theme.accent
                            font.pointSize: Theme.fontSizeLarge
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Repeater {
                            model: mission_objectives && mission_objectives.optional_objectives ? mission_objectives.optional_objectives : []

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: optional_content.implicitHeight + Theme.spacingMedium * 2
                                radius: Theme.radiusMedium
                                color: Theme.infoBg
                                border.color: Theme.infoBr
                                border.width: 1

                                RowLayout {
                                    id: optional_content

                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingMedium
                                    spacing: Theme.spacingMedium

                                    Label {
                                        text: "★"
                                        color: Theme.accent
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.alignment: Qt.AlignTop
                                    }

                                    Label {
                                        text: modelData.description || qsTr("Optional objective")
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeMedium
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                }

                            }

                        }

                    }

                    Label {
                        visible: !mission_objectives || (!mission_objectives.victory_conditions && !mission_objectives.defeat_conditions && !mission_objectives.optional_objectives)
                        text: qsTr("No mission objectives available.\nThis may be a skirmish game or objectives have not been configured.")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeMedium
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spacingXLarge
                    }

                }

            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Item {
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: qsTr("Close")
                    onClicked: root.closeRequested()
                }

            }

        }

    }

}
