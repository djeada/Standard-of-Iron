import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property var current_campaign: null
    property var campaigns: []
    property int selected_mission_index: -1
    property var campaign_map_state: null

    signal mission_selected(string campaign_id, string mission_id)
    signal cancelled()

    function refresh_campaigns() {
        if (typeof game !== "undefined" && game.available_campaigns) {
            campaigns = game.available_campaigns;
            if (campaigns.length > 0 && !current_campaign)
                current_campaign = campaigns[0];

        }
        build_campaign_state();
        ensure_mission_selection();
    }

    function select_next_unlocked_mission() {
        if (!current_campaign || !current_campaign.missions)
            return ;

        var all_completed = current_campaign.missions.length > 0;
        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (mission && mission.unlocked && !mission.completed) {
                selected_mission_index = i;
                return ;
            }
            if (!mission || !mission.completed)
                all_completed = false;

        }
        if (all_completed) {
            var last_index = current_campaign.missions.length - 1;
            selected_mission_index = last_index;
        }
    }

    function ensure_mission_selection() {
        if (!current_campaign || !current_campaign.missions || current_campaign.missions.length === 0) {
            selected_mission_index = -1;
            return ;
        }
        if (selected_mission_index >= 0 && selected_mission_index < current_campaign.missions.length)
            return ;

        select_next_unlocked_mission();
    }

    function select_mission_by_region(region_id) {
        if (!current_campaign || !current_campaign.missions || !region_id)
            return ;

        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (mission.world_region_id === region_id) {
                selected_mission_index = i;
                return ;
            }
        }
    }

    function build_campaign_state() {
        if (!current_campaign || !current_campaign.missions) {
            campaign_map_state = null;
            return ;
        }
        var region_stats = {
        };
        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (!mission || !mission.world_region_id)
                continue;

            var region_id = mission.world_region_id;
            if (!region_stats[region_id])
                region_stats[region_id] = {
                "completed": false,
                "unlocked": false
            };

            if (mission.completed)
                region_stats[region_id].completed = true;

            if (mission.unlocked)
                region_stats[region_id].unlocked = true;

        }
        var provinces = [];
        for (var key in region_stats) {
            if (!region_stats.hasOwnProperty(key))
                continue;

            var state = region_stats[key];
            var owner = state.completed ? "carthage" : (state.unlocked ? "neutral" : "rome");
            provinces.push({
                "id": key,
                "owner": owner
            });
        }
        campaign_map_state = {
            "provinces": provinces
        };
    }

    onVisibleChanged: {
        if (visible && typeof game !== "undefined" && game.load_campaigns) {
            game.load_campaigns();
            refresh_campaigns();
        }
    }
    onCurrent_campaignChanged: {
        build_campaign_state();
        ensure_mission_selection();
    }
    anchors.fill: parent
    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }

    Connections {
        function onAvailable_campaigns_changed() {
            refresh_campaigns();
        }

        target: (typeof game !== "undefined") ? game : null
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.95, 1600)
        height: Math.min(parent.height * 0.95, 1000)
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
            color: "transparent"
            border.color: "#4a3722"
            border.width: 1
            radius: Math.max(2, container.radius - 4)
            anchors.margins: 4
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingXLarge
            spacing: Theme.spacingLarge

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny

                    Label {
                        text: current_campaign ? current_campaign.title : qsTr("Campaign")
                        color: Theme.textMain
                        font.pointSize: Theme.fontSizeHero
                        font.bold: true
                        font.family: "serif"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Campaign Chronicle • ") + (current_campaign ? current_campaign.description : "")
                        color: Theme.textSubLite
                        font.pointSize: Theme.fontSizeMedium
                        font.family: "serif"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * 0.38
                    radius: Theme.radiusMedium
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#3a2f23"
                        }

                        GradientStop {
                            position: 1
                            color: "#241b14"
                        }

                    }
                    border.color: "#a7814a"
                    border.width: 2

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Campaign Chronicle")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeTitle
                            font.bold: true
                            font.family: "serif"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: progress_layout.implicitHeight + Theme.spacingSmall * 2
                            Layout.preferredHeight: implicitHeight
                            radius: Theme.radiusSmall
                            color: "#241c14"
                            border.color: "#8f6d43"
                            border.width: 1

                            ColumnLayout {
                                id: progress_layout

                                anchors.fill: parent
                                anchors.margins: Theme.spacingSmall
                                spacing: Theme.spacingSmall

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Label {
                                        text: qsTr("Campaign Progress:")
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeSmall
                                        font.bold: true
                                        font.family: "serif"
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 16
                                        radius: 8
                                        color: Theme.disabledBg
                                        border.color: Theme.border
                                        border.width: 1

                                        Rectangle {
                                            property int completed_count: {
                                                if (!current_campaign || !current_campaign.missions)
                                                    return 0;

                                                var count = 0;
                                                for (var i = 0; i < current_campaign.missions.length; i++) {
                                                    if (current_campaign.missions[i].completed)
                                                        count++;

                                                }
                                                return count;
                                            }
                                            property int total_count: current_campaign && current_campaign.missions ? current_campaign.missions.length : 0
                                            property real progress_ratio: total_count > 0 ? completed_count / total_count : 0

                                            width: parent.width * progress_ratio
                                            height: parent.height
                                            radius: parent.radius
                                            color: Theme.successBg
                                            border.color: Theme.successBr
                                            border.width: 1

                                            Behavior on width {
                                                NumberAnimation {
                                                    duration: Theme.animNormal
                                                }

                                            }

                                        }

                                    }

                                    Label {
                                        property int completed_count: {
                                            if (!current_campaign || !current_campaign.missions)
                                                return 0;

                                            var count = 0;
                                            for (var i = 0; i < current_campaign.missions.length; i++) {
                                                if (current_campaign.missions[i].completed)
                                                    count++;

                                            }
                                            return count;
                                        }
                                        property int total_count: current_campaign && current_campaign.missions ? current_campaign.missions.length : 0

                                        text: completed_count + " / " + total_count
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeSmall
                                        font.bold: true
                                    }

                                }

                            }

                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            ListView {
                                id: mission_list_view

                                model: current_campaign ? current_campaign.missions : []
                                spacing: Theme.spacingSmall
                                currentIndex: selected_mission_index

                                delegate: MissionListItem {
                                    width: mission_list_view.width - Theme.spacingSmall
                                    mission_data: modelData
                                    is_selected: root.selected_mission_index === index
                                    onClicked: {
                                        selected_mission_index = index;
                                    }
                                }

                            }

                        }

                    }

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * 0.62
                    radius: Theme.radiusMedium
                    color: "#201913"
                    border.color: "#8f6d43"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        spacing: Theme.spacingMedium

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: Theme.radiusSmall
                            color: "#5a2f23"
                            border.color: "#a7814a"
                            border.width: 1

                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Imperium Command Table • Mediterranean Theater")
                                color: "#f0dfbc"
                                font.pointSize: Theme.fontSizeMedium
                                font.bold: true
                                font.family: "serif"
                            }
                        }

                        MediterraneanMapPanel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            selected_mission: selected_mission_index >= 0 && current_campaign && current_campaign.missions ? current_campaign.missions[selected_mission_index] : null
                            campaign_state: root.campaign_map_state
                            onRegionSelected: function(region_id) {
                                select_mission_by_region(region_id);
                            }
                        }

                    }

                }

            }

            MissionDetailPanel {
                id: mission_detail_panel

                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 236 : 0
                visible: selected_mission_index >= 0
                mission_data: selected_mission_index >= 0 && current_campaign && current_campaign.missions ? current_campaign.missions[selected_mission_index] : null
                campaign_id: current_campaign ? current_campaign.id : ""
                onStart_mission_clicked: {
                    if (current_campaign && mission_data && mission_data.mission_id)
                        root.mission_selected(current_campaign.id, mission_data.mission_id);

                }

                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: Theme.animNormal
                    }

                }

            }

        }

    }

}
