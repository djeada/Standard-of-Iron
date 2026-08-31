import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio
import StandardOfIron.Core 1.0

Item {
    id: root

    property var missions: []
    property int selected_index: -1

    readonly property var selected: (selected_index >= 0 && selected_index < missions.length) ? missions[selected_index] : null
    readonly property int completed_count: {
        var count = 0;
        for (var i = 0; i < missions.length; i++) {
            if (missions[i] && missions[i].completed)
                count++;
        }
        return count;
    }

    signal mission_chosen(string file_path)
    signal cancelled

    function refresh_missions() {
        if (typeof game === "undefined" || !game.setup)
            return;
        if (game.setup.load_missions)
            game.setup.load_missions();
        missions = game.setup.missions || [];
        if (missions.length > 0 && (selected_index < 0 || selected_index >= missions.length))
            selected_index = 0;
    }

    function condition_lines(conditions) {
        if (!conditions)
            return [];
        var lines = [];
        for (var i = 0; i < conditions.length; i++) {
            var line = conditions[i];
            if (line && line.description)
                lines.push(String(line.description));
        }
        return lines;
    }

    function objective_lines(mission) {
        return root.condition_lines(mission ? mission.objectives : null);
    }

    function bonus_lines(mission) {
        return root.condition_lines(mission ? mission.optional_objectives : null);
    }

    function failure_lines(mission) {
        return root.condition_lines(mission ? mission.defeat_conditions : null);
    }

    function starting_force_text(mission) {
        if (!mission || !mission.starting_force || mission.starting_force.length === 0)
            return "";
        var parts = [];
        for (var i = 0; i < mission.starting_force.length; i++) {
            var unit = mission.starting_force[i];
            if (!unit)
                continue;
            parts.push(qsTr("%1 × %2").arg(Design.Numerals.roman(unit.count)).arg(Design.Icons.humanise(String(unit.type))));
        }
        return parts.join(", ");
    }

    function orders_heading(mission) {
        if (!mission)
            return "";
        if (root.objective_lines(mission).length < 2)
            return qsTr("Orders");
        return String(mission.victory_mode).toLowerCase() === "any" ? qsTr("Orders — any one of these ends it") : qsTr("Orders — all of them, or none");
    }

    function field_size(mission) {
        if (!mission || !mission.map_width || !mission.map_height)
            return "";
        return mission.map_width + " × " + mission.map_height;
    }

    function preview_configs(mission) {
        if (!mission || !mission.map_path)
            return [];
        return [{
                "player_id": 1,
                "playerName": qsTr("You"),
                "colorIndex": 0,
                "team_id": 1,
                "nationId": "roman_republic",
                "isHuman": true
            }];
    }

    function start_selected() {
        if (!root.selected || !root.selected.file_path)
            return;
        root.mission_chosen(String(root.selected.file_path));
    }

    anchors.fill: parent
    focus: true

    onVisibleChanged: {
        if (visible)
            refresh_missions();
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            if (root.selected_index < root.missions.length - 1)
                root.selected_index++;
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            if (root.selected_index > 0)
                root.selected_index--;
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.start_selected();
            event.accepted = true;
        }
    }

    Connections {
        function onMissions_changed() {
            root.missions = game.setup.missions || [];
        }

        target: (typeof game !== "undefined") ? game.setup : null
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.94, 1420)
        height: Math.min(parent.height * 0.94, 940)
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#2b2118"
            }

            GradientStop {
                position: 1
                color: "#1a140f"
            }
        }
        border.color: "#8f6d43"
        border.width: 2
        opacity: 0.98
        clip: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            color: "transparent"
            border.color: "#4a3722"
            border.width: 1
            radius: Math.max(2, container.radius - 4)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny

                    Label {
                        text: qsTr("Missions")
                        color: Theme.textMain
                        font.pixelSize: Design.Typography.hero
                        font.bold: true
                        font.family: "serif"
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("One small field, one order to carry out. No campaign to lose, no second army to worry about.")
                        color: Theme.textSubLite
                        font.pixelSize: Design.Typography.bodyLarge
                        font.family: "serif"
                        wrapMode: Text.WordWrap
                    }
                }

                Label {
                    text: Design.Numerals.ratio(root.completed_count, root.missions.length) + " " + qsTr("carried out")
                    color: Theme.textSubLite
                    font.pixelSize: Design.Typography.label
                    visible: root.missions.length > 0
                }

                StyledButton {
                    text: qsTr("← Back")
                    onClicked: root.cancelled()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingLarge

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: Math.round(container.width * 0.36)
                    radius: Theme.radiusMedium
                    color: "#241c14"
                    border.color: "#8f6d43"
                    border.width: 1
                    clip: true

                    Label {
                        id: empty_note

                        anchors.centerIn: parent
                        width: parent.width - Theme.spacingLarge * 2
                        visible: root.missions.length === 0
                        text: qsTr("No missions are installed.")
                        color: Theme.textSub
                        font.pixelSize: Design.Typography.label
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        id: mission_list

                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        model: root.missions
                        spacing: Theme.spacingSmall
                        clip: true
                        currentIndex: root.selected_index
                        boundsBehavior: Flickable.StopAtBounds
                        visible: root.missions.length > 0

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            id: row

                            readonly property bool is_selected: root.selected_index === index
                            readonly property bool is_done: !!(modelData && modelData.completed)

                            width: mission_list.width - Theme.spacingSmall
                            implicitHeight: row_layout.implicitHeight + Theme.spacingMedium * 2
                            radius: Theme.radiusMedium
                            color: is_selected ? "#3d2b20" : (row_mouse.containsMouse ? "#33261c" : "#2a2018")
                            border.color: is_selected ? "#c29555" : "#6f5432"
                            border.width: is_selected ? 2 : 1

                            Rectangle {
                                width: 3
                                height: parent.height - Theme.spacingSmall * 2
                                x: Theme.spacingSmall
                                y: Theme.spacingSmall
                                radius: 1
                                color: row.is_done ? Theme.successBr : "#8f6d43"
                            }

                            MouseArea {
                                id: row_mouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onContainsMouseChanged: {
                                    if (containsMouse && typeof game !== "undefined")
                                        UiAudio.play_hover(game.audio_system);
                                }
                                onClicked: {
                                    if (typeof game !== "undefined")
                                        UiAudio.play_click(game.audio_system);
                                    root.selected_index = index;
                                }
                                onDoubleClicked: {
                                    root.selected_index = index;
                                    root.start_selected();
                                }
                            }

                            ColumnLayout {
                                id: row_layout

                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                anchors.leftMargin: Theme.spacingMedium + Theme.spacingSmall
                                spacing: Theme.spacingTiny

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData ? String(modelData.title) : ""
                                        color: Theme.textMain
                                        font.pixelSize: Design.Typography.bodyLarge
                                        font.bold: true
                                        font.family: "serif"
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: qsTr("Carried out")
                                        visible: row.is_done
                                        color: Theme.successText
                                        font.pixelSize: Design.Typography.caption
                                        font.bold: true
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData ? String(modelData.map_name) + " • " + root.field_size(modelData) : ""
                                    color: Theme.textSubLite
                                    font.pixelSize: Design.Typography.caption
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusMedium
                    color: "#201913"
                    border.color: "#8f6d43"
                    border.width: 1
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLarge
                        spacing: Theme.spacingMedium
                        visible: root.selected !== null

                        Label {
                            Layout.fillWidth: true
                            text: root.selected ? String(root.selected.title) : ""
                            color: Theme.textBright
                            font.pixelSize: Design.Typography.title
                            font.bold: true
                            font.family: "serif"
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selected ? String(root.selected.map_name) + " • " + root.field_size(root.selected) : ""
                            color: Theme.textSubLite
                            font.pixelSize: Design.Typography.label
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: "#4a3722"
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: Theme.spacingLarge

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: Theme.spacingMedium

                                Label {
                                    Layout.fillWidth: true
                                    text: root.selected ? String(root.selected.summary) : ""
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.body
                                    font.family: "serif"
                                    wrapMode: Text.WordWrap
                                }

                                MissionOrderList {
                                    Layout.fillWidth: true
                                    heading: root.orders_heading(root.selected)
                                    heading_color: Theme.accentBright
                                    lines: root.objective_lines(root.selected)
                                }

                                MissionOrderList {
                                    Layout.fillWidth: true
                                    heading: qsTr("Worth doing as well")
                                    heading_color: Theme.infoText
                                    lines: root.bonus_lines(root.selected)
                                }

                                MissionOrderList {
                                    Layout.fillWidth: true
                                    heading: qsTr("You bring")
                                    heading_color: Theme.textSubLite
                                    lines: root.starting_force_text(root.selected).length > 0 ? [root.starting_force_text(root.selected)] : []
                                }

                                MissionOrderList {
                                    Layout.fillWidth: true
                                    heading: qsTr("It ends badly if")
                                    heading_color: Theme.dangerBr
                                    lines: root.failure_lines(root.selected)
                                }

                                Item {
                                    Layout.fillHeight: true
                                }
                            }

                            MapPreview {
                                id: field_preview

                                Layout.alignment: Qt.AlignTop
                                Layout.preferredWidth: Math.round(container.width * 0.26)
                                Layout.preferredHeight: Layout.preferredWidth
                                color: "#181310"
                                border.color: "#6f5432"
                                border.width: 1
                                legend_text: ""
                                map_path: root.selected ? String(root.selected.map_path) : ""
                                player_configs: root.preview_configs(root.selected)
                                onMap_pathChanged: refresh_preview()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMedium

                            Label {
                                Layout.fillWidth: true
                                text: (root.selected && root.selected.completed) ? qsTr("You have carried this one out before.") : ""
                                color: Theme.textDim
                                font.pixelSize: Design.Typography.caption
                            }

                            StyledButton {
                                text: (root.selected && root.selected.completed) ? qsTr("Take it again") : qsTr("Take the field")
                                button_style: "primary"
                                onClicked: root.start_selected()
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: root.selected === null
                        text: qsTr("Pick a mission.")
                        color: Theme.textSub
                        font.pixelSize: Design.Typography.label
                    }
                }
            }
        }
    }
}
