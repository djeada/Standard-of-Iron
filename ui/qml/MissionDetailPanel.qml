import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var mission_data: null
    property string campaign_id: ""
    property var mission_definition: null

    signal start_mission_clicked()

    function load_mission_definition() {
        if (mission_data && mission_data.mission_id && typeof game !== "undefined" && game.get_mission_definition) {
            mission_definition = game.get_mission_definition(mission_data.mission_id);
        }
    }

    onMission_dataChanged: load_mission_definition()

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
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny

                    Label {
                        text: qsTr("Objectives:")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Repeater {
                        model: {
                            if (!mission_definition || !mission_definition.victory_conditions)
                                return [];

                            return mission_definition.victory_conditions;
                        }

                        delegate: Label {
                            text: "• " + (modelData.description || qsTr("Complete mission objective"))
                            color: Theme.textSubLite
                            font.pointSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                    }

                    Label {
                        visible: !mission_definition || !mission_definition.victory_conditions || mission_definition.victory_conditions.length === 0
                        text: qsTr("• Complete the mission successfully")
                        color: Theme.textSubLite
                        font.pointSize: Theme.fontSizeSmall
                    }

                }

                ColumnLayout {
                    spacing: Theme.spacingTiny
                    visible: mission_definition && mission_definition.player_setup && mission_definition.player_setup.starting_units

                    Label {
                        text: qsTr("Your Forces:")
                        color: Theme.textDim
                        font.pointSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Flow {
                        Layout.preferredWidth: 180
                        spacing: Theme.spacingTiny

                        Repeater {
                            model: {
                                if (!mission_definition || !mission_definition.player_setup || !mission_definition.player_setup.starting_units)
                                    return [];

                                var units = mission_definition.player_setup.starting_units;
                                var grouped = {};
                                for (var i = 0; i < units.length; i++) {
                                    var unit = units[i];
                                    var type = unit.type || "unknown";
                                    grouped[type] = (grouped[type] || 0) + (unit.count || 1);
                                }
                                var result = [];
                                for (var key in grouped) {
                                    result.push({
                                        "type": key,
                                        "count": grouped[key]
                                    });
                                }
                                return result;
                            }

                            delegate: Rectangle {
                                width: unit_label.implicitWidth + 8
                                height: 20
                                radius: Theme.radiusSmall
                                color: Theme.infoBg
                                border.color: Theme.infoBr
                                border.width: 1

                                Label {
                                    id: unit_label

                                    anchors.centerIn: parent
                                    text: modelData.count + "x " + modelData.type
                                    color: Theme.infoText
                                    font.pointSize: Theme.fontSizeTiny
                                }

                            }

                        }

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
