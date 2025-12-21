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
    }

    function select_next_unlocked_mission() {
        if (!current_campaign || !current_campaign.missions)
            return ;

        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (mission.unlocked && !mission.completed) {
                selected_mission_index = i;
                mission_list_view.currentIndex = i;
                return ;
            }
        }
    }

    function select_mission_by_region(region_id) {
        if (!current_campaign || !current_campaign.missions || !region_id)
            return ;

        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (mission.world_region_id === region_id) {
                selected_mission_index = i;
                mission_list_view.currentIndex = i;
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
    onCurrent_campaignChanged: build_campaign_state()
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

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny

                    Label {
                        text: current_campaign ? current_campaign.title : qsTr("Campaign")
                        color: Theme.textMain
                        font.pointSize: Theme.fontSizeHero
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: current_campaign ? current_campaign.description : ""
                        color: Theme.textSubLite
                        font.pointSize: Theme.fontSizeMedium
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
                    Layout.preferredWidth: parent.width * 0.45
                    radius: Theme.radiusMedium
                    color: Theme.cardBase
                    border.color: Theme.cardBorder
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Missions")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeTitle
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: progress_layout.implicitHeight + Theme.spacingSmall * 2
                            Layout.preferredHeight: implicitHeight
                            radius: Theme.radiusSmall
                            color: Theme.cardBase
                            border.color: Theme.cardBorder
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
                                        text: qsTr("Progress:")
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeSmall
                                        font.bold: true
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
                                    is_selected: mission_list_view.currentIndex === index
                                    onClicked: {
                                        selected_mission_index = index;
                                        mission_list_view.currentIndex = index;
                                    }
                                }

                            }

                        }

                    }

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * 0.55
                    radius: Theme.radiusMedium
                    color: Theme.cardBase
                    border.color: Theme.cardBorder
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingMedium

                        Label {
                            text: qsTr("Mediterranean Strategic Map")
                            color: Theme.textMain
                            font.pointSize: Theme.fontSizeTitle
                            font.bold: true
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
                Layout.preferredHeight: visible ? 180 : 0
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
