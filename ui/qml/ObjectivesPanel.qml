import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var mission_objectives: null

    signal close_requested

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    function refresh_objectives() {
        mission_objectives = (game_ready() && game.setup.current_mission_objectives) ? game.setup.current_mission_objectives() : null;
    }

    function objective_list(key) {
        return (mission_objectives && mission_objectives[key]) ? mission_objectives[key] : [];
    }

    readonly property var stage_list: (game_ready() && game.mission && game.mission.staged) ? game.mission.stages : objective_list("stages")

    anchors.fill: parent
    z: 10
    focus: true
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.close_requested();
            event.accepted = true;
        }
    }
    Component.onCompleted: refresh_objectives()
    onVisibleChanged: {
        if (visible)
            refresh_objectives();
    }

    Connections {
        function onCurrent_mission_changed() {
            root.refresh_objectives();
        }

        ignoreUnknownSignals: true
        target: root.game_ready() ? game.setup : null
    }

    Rectangle {
        anchors.fill: parent
        color: Design.Theme.scrim
    }

    Design.IronPanel {
        id: container

        width: Math.min(parent.width * 0.7, Design.Metrics.space24 * 34)
        height: Math.min(parent.height * 0.8, Design.Metrics.space24 * 26)
        anchors.centerIn: parent
        raised: true
        accessibleName: qsTr("Mission briefing")

        ColumnLayout {
            anchors.fill: parent
            spacing: Design.Metrics.space12

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.Metrics.space12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Mission Briefing")
                    color: Design.Theme.textPrimary
                    font.family: Design.Typography.displayFamily
                    font.pixelSize: Design.Typography.title
                    font.weight: Design.Typography.bold
                }

                Design.IronIconButton {
                    iconText: Design.Icons.close
                    tooltip: qsTr("Close the briefing")
                    onClicked: root.close_requested()
                }
            }

            Design.IronDivider {
                Layout.fillWidth: true
            }

            Design.BriefingLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                factionId: root.game_ready() ? game.local_player_nation : ""
                title: root.mission_objectives && root.mission_objectives.title ? root.mission_objectives.title : ""
                summary: root.mission_objectives && root.mission_objectives.summary ? root.mission_objectives.summary : ""
                victoryConditions: root.objective_list("victory_conditions")
                defeatConditions: root.objective_list("defeat_conditions")
                optionalObjectives: root.objective_list("optional_objectives")
                stages: root.stage_list
                victoryMode: root.mission_objectives && root.mission_objectives.victory_mode ? root.mission_objectives.victory_mode : "any"
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Design.IronButton {
                    text: qsTr("To Battle")
                    tone: "primary"
                    onClicked: root.close_requested()
                }
            }
        }
    }
}
