import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import StandardOfIron.Core 1.0

Item {
    id: root

    property var current_campaign: null
    property var campaigns: []
    property int selected_mission_index: -1
    property var campaign_map_state: null
    readonly property string theater_heading: qsTr("Campaign War Table • Mediterranean Theater")
    readonly property bool short_screen: height < Design.A11y.scaled(820)
    readonly property int total_count: current_campaign && current_campaign.missions ? current_campaign.missions.length : 0
    readonly property int completed_count: {
        if (!current_campaign || !current_campaign.missions)
            return 0;
        var count = 0;
        for (var i = 0; i < current_campaign.missions.length; i++) {
            if (current_campaign.missions[i] && current_campaign.missions[i].completed)
                count++;
        }
        return count;
    }

    property string selected_difficulty: (typeof game !== "undefined" && game.setup) ? game.setup.preferred_difficulty : DifficultyCatalog.defaultId

    signal mission_selected(string campaign_id, string mission_id, string difficulty)
    signal cancelled

    function refresh_campaigns() {
        if (typeof game !== "undefined" && game.setup.campaigns) {
            campaigns = game.setup.campaigns;
            if (campaigns.length > 0 && !current_campaign)
                current_campaign = campaigns[0];
        }
        build_campaign_state();
        ensure_mission_selection();
    }

    function select_next_unlocked_mission() {
        if (!current_campaign || !current_campaign.missions)
            return;
        var all_completed = current_campaign.missions.length > 0;
        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (mission && mission.unlocked && !mission.completed) {
                selected_mission_index = i;
                return;
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
            return;
        }
        if (selected_mission_index >= 0 && selected_mission_index < current_campaign.missions.length)
            return;
        select_next_unlocked_mission();
    }

    function select_mission_by_region(region_id) {
        if (!current_campaign || !current_campaign.missions || !region_id)
            return;
        for (var i = 0; i < current_campaign.missions.length; i++) {
            var mission = current_campaign.missions[i];
            if (mission.world_region_id === region_id) {
                selected_mission_index = i;
                return;
            }
        }
    }

    function build_campaign_state() {
        if (!current_campaign || !current_campaign.missions) {
            campaign_map_state = null;
            return;
        }
        var region_stats = {};
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
        if (visible && typeof game !== "undefined" && game.setup.load_campaigns) {
            game.setup.load_campaigns();
            refresh_campaigns();
        }
    }
    onCurrent_campaignChanged: {
        build_campaign_state();
        ensure_mission_selection();
    }
    anchors.fill: parent
    focus: true
    Keys.forwardTo: [campaign_map_panel.key_handler]
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
    }

    Connections {
        function onCampaigns_changed() {
            refresh_campaigns();
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

        width: Math.min(parent.width * 0.975, 1660)
        height: Math.min(parent.height * 0.975, 1040)
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
            anchors.margins: root.short_screen ? Theme.spacingMedium : Theme.spacingLarge
            spacing: root.short_screen ? Theme.spacingSmall : Theme.spacingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny

                    Label {
                        text: current_campaign ? current_campaign.title : qsTr("Campaign")
                        color: Theme.textMain
                        font.pixelSize: root.short_screen ? Design.Typography.title : Design.Typography.hero
                        font.bold: true
                        font.family: "serif"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        id: campaign_description

                        objectName: "campaignDescription"
                        text: current_campaign ? current_campaign.description : ""
                        visible: text.length > 0
                        color: Theme.textSubLite
                        font.pixelSize: root.short_screen ? Design.Typography.label : Design.Typography.body
                        font.family: "serif"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                StyledButton {
                    Layout.alignment: Qt.AlignTop
                    uiSound: "none"
                    text: qsTr("← Back")
                    onClicked: root.cancelled()
                }
            }

            RowLayout {
                id: campaign_columns

                readonly property int column_gap: root.short_screen ? Theme.spacingMedium : Theme.spacingLarge
                readonly property int chronicle_width: Math.round(Math.max(Design.A11y.scaled(290), Math.min(Design.A11y.scaled(420), width * 0.26)))
                readonly property int detail_width: Math.round(Math.max(Design.A11y.scaled(330), Math.min(Design.A11y.scaled(520), width * 0.3)))

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                spacing: column_gap

                Rectangle {
                    objectName: "campaignChronicle"
                    Layout.fillHeight: true
                    Layout.preferredWidth: campaign_columns.chronicle_width
                    Layout.maximumWidth: campaign_columns.chronicle_width
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
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        spacing: Theme.spacingSmall

                        Label {
                            text: qsTr("Campaign Chronicle")
                            color: Theme.textMain
                            font.pixelSize: Design.Typography.heading
                            font.bold: true
                            font.family: "serif"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: progress_layout.implicitHeight + Theme.spacingSmall * 2
                            Layout.preferredHeight: implicitHeight
                            radius: Theme.radiusSmall
                            color: "#241c14"
                            border.color: "#8f6d43"
                            border.width: 1

                            RowLayout {
                                id: progress_layout

                                anchors.fill: parent
                                anchors.margins: Theme.spacingSmall
                                spacing: Theme.spacingSmall

                                Label {
                                    text: qsTr("Progress")
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.body
                                    font.bold: true
                                    font.family: "serif"
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 12
                                    radius: 6
                                    color: Theme.disabledBg
                                    border.color: Theme.border
                                    border.width: 1

                                    Rectangle {
                                        width: parent.width * (root.total_count > 0 ? root.completed_count / root.total_count : 0)
                                        height: parent.height
                                        radius: parent.radius
                                        color: Theme.successBg
                                        border.color: Theme.successBr
                                        border.width: 1

                                        Behavior on width  {
                                            NumberAnimation {
                                                duration: Theme.animNormal
                                            }
                                        }
                                    }
                                }

                                Label {
                                    objectName: "campaignProgressCount"
                                    text: qsTr("%1 / %2").arg(root.completed_count).arg(root.total_count)
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.body
                                    font.bold: true
                                }
                            }
                        }

                        ListView {
                            id: mission_list_view

                            objectName: "campaignMissionList"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 0
                            clip: true
                            model: current_campaign ? current_campaign.missions : []
                            spacing: Theme.spacingSmall
                            currentIndex: selected_mission_index
                            boundsBehavior: Flickable.StopAtBounds
                            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

                            ScrollBar.vertical: Design.IronScrollBar {
                                id: mission_list_scroll_bar

                                objectName: "campaignMissionListScrollBar"
                            }

                            delegate: MissionListItem {
                                width: mission_list_view.width - (mission_list_scroll_bar.overflowing ? mission_list_scroll_bar.thickness + Theme.spacingTiny : 0)
                                mission_data: modelData
                                is_selected: root.selected_mission_index === index
                                onClicked: {
                                    selected_mission_index = index;
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    radius: Theme.radiusMedium
                    color: "#201913"
                    border.color: "#8f6d43"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        spacing: Theme.spacingSmall

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: theater_label.implicitHeight + Theme.spacingSmall * 2
                            radius: Theme.radiusSmall
                            color: "#5a2f23"
                            border.color: "#a7814a"
                            border.width: 1

                            Label {
                                id: theater_label

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Theme.spacingSmall
                                text: root.theater_heading
                                color: "#f0dfbc"
                                font.pixelSize: Design.Typography.bodyLarge
                                font.bold: true
                                font.family: "serif"
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        MediterraneanMapPanel {
                            id: campaign_map_panel

                            objectName: "campaignMapPanel"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: Design.A11y.scaled(220)
                            selected_mission: selected_mission_index >= 0 && current_campaign && current_campaign.missions ? current_campaign.missions[selected_mission_index] : null
                            campaign_state: root.campaign_map_state
                            onRegion_selected: function (region_id) {
                                select_mission_by_region(region_id);
                            }
                        }

                        DifficultySelector {
                            id: campaign_difficulty

                            objectName: "campaignDifficultySelector"
                            Layout.fillWidth: true
                            compact: true
                            visible: selected_mission_index >= 0
                            selected_id: root.selected_difficulty
                            onChosen: function (difficulty_id) {
                                root.selected_difficulty = difficulty_id;
                            }
                        }
                    }
                }

                MissionDetailPanel {
                    id: mission_detail_panel

                    objectName: "missionDetailPanel"

                    Layout.fillHeight: true
                    Layout.preferredWidth: campaign_columns.detail_width
                    Layout.maximumWidth: campaign_columns.detail_width
                    visible: selected_mission_index >= 0
                    mission_data: selected_mission_index >= 0 && current_campaign && current_campaign.missions ? current_campaign.missions[selected_mission_index] : null
                    campaign_id: current_campaign ? current_campaign.id : ""
                    difficulty_id: root.selected_difficulty
                    onStart_mission_clicked: {
                        if (current_campaign && mission_data && mission_data.mission_id)
                            root.mission_selected(current_campaign.id, mission_data.mission_id, root.selected_difficulty);
                    }
                }
            }
        }
    }
}
