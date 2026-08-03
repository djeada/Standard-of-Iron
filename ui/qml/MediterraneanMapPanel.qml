import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: root

    property var selected_mission: null
    property string active_region_id: selected_mission && selected_mission.world_region_id ? selected_mission.world_region_id : ""
    property real map_orbit_yaw: 180
    property real map_orbit_pitch: 90
    property real map_orbit_distance: 1.2

    property bool camera_settled: false
    property real map_pan_u: 0
    property real map_pan_v: 0

    property real terrain_height_scale: 0.085
    property bool show_province_fills: true
    property string hover_province_name: ""
    property string hover_province_owner: ""

    property string hover_province_id: ""
    property real hover_mouse_x: 0
    property real hover_mouse_y: 0
    property var province_labels: []
    property int label_refresh: 0

    readonly property string map_label_family: Design.Typography.displayFamily

    readonly property var city_rank: ({
            "Rome": 0,
            "Carthage": 0,
            "New Carthage": 0,
            "Massalia": 1,
            "Placentia": 1,
            "Saguntum": 1,
            "Capua": 2,
            "Syracuse": 2,
            "Cirta": 2,
            "Tarentum": 3,
            "Mediolanum": 3,
            "Ariminum": 3
        })
    readonly property int city_rank_default: 5

    readonly property var mission_regions: ({
            "transalpine_gaul": {
                "uv": [0.52, 0.755],
                "name": qsTr("Rhône")
            },
            "alps": {
                "uv": [0.63, 0.815],
                "name": qsTr("The Alps")
            },
            "cisalpine_gaul": {
                "uv": [0.7, 0.845],
                "name": qsTr("N. Italy")
            },
            "etruria": {
                "uv": [0.79, 0.762],
                "name": qsTr("Trasimene")
            },
            "apulia": {
                "uv": [0.925, 0.618],
                "name": qsTr("Cannae")
            },
            "campania": {
                "uv": [0.862, 0.64],
                "name": qsTr("Campania")
            },
            "southern_italy": {
                "uv": [0.868, 0.634],
                "name": qsTr("S. Italy")
            },
            "carthage_core": {
                "uv": [0.715, 0.345],
                "name": qsTr("Zama")
            }
        })

    readonly property string active_region_name: {
        var region = root.mission_regions[root.active_region_id];
        return region && region.name ? region.name : qsTr("Mission Region");
    }

    property var city_markers: []
    property var campaign_state: null
    property var campaign_state_sources: ["assets/campaign_map/campaign_state.json", "qrc:/assets/campaign_map/campaign_state.json", "qrc:/StandardOfIron/assets/campaign_map/campaign_state.json", "qrc:/qt/qml/StandardOfIron/assets/campaign_map/campaign_state.json"]

    property var owner_color_map: ({
            "rome": [0.72, 0.19, 0.16, 0.44],
            "carthage": [0.41, 0.16, 0.36, 0.44],
            "neutral": [0.44, 0.42, 0.38, 0.16]
        })

    property var region_camera_positions: ({
            "transalpine_gaul": {
                "yaw": 196,
                "pitch": 79,
                "distance": 1.12
            },
            "alps": {
                "yaw": 190,
                "pitch": 79,
                "distance": 1.1
            },
            "cisalpine_gaul": {
                "yaw": 184,
                "pitch": 79,
                "distance": 1.1
            },
            "etruria": {
                "yaw": 180,
                "pitch": 79,
                "distance": 1.1
            },
            "apulia": {
                "yaw": 174,
                "pitch": 79,
                "distance": 1.12
            },
            "campania": {
                "yaw": 177,
                "pitch": 79,
                "distance": 1.1
            },
            "southern_italy": {
                "yaw": 175,
                "pitch": 79,
                "distance": 1.12
            },
            "carthage_core": {
                "yaw": 170,
                "pitch": 80,
                "distance": 1.15
            }
        })

    signal region_selected(string region_id)

    function focus_on_region(region_id) {
        if (!region_id || region_id === "")
            return;
        var camera_pos = region_camera_positions[region_id];
        if (camera_pos) {
            map_orbit_yaw = camera_pos.yaw;
            map_orbit_pitch = camera_pos.pitch;
            map_orbit_distance = camera_pos.distance;
            map_pan_u = 0;
            map_pan_v = 0;
        }
    }

    function load_provinces() {
        if (campaign_map_loader.item) {
            var labels = campaign_map_loader.item.province_labels;
            if (labels && labels.length > 0) {
                province_labels = labels;
                label_refresh += 1;
                apply_campaign_state();
            }
        }
    }

    function load_campaign_state() {
        if (campaign_state)
            return;
        load_campaign_state_from(0);
    }

    function load_campaign_state_from(index) {
        if (index >= campaign_state_sources.length)
            return;
        var xhr = new XMLHttpRequest();
        xhr.open("GET", campaign_state_sources[index]);
        xhr.onreadystatechange = function () {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return;
            if (xhr.status !== 200 && xhr.status !== 0) {
                load_campaign_state_from(index + 1);
                return;
            }
            try {
                var data = JSON.parse(xhr.responseText);
                if (data && data.provinces && data.provinces.length > 0) {
                    if (!campaign_state) {
                        campaign_state = data;
                        apply_campaign_state();
                    }
                }
            } catch (e) {
                load_campaign_state_from(index + 1);
            }
        };
        xhr.send();
    }

    function owner_color_for(owner) {
        var key = owner ? owner.toLowerCase() : "neutral";
        if (owner_color_map[key])
            return owner_color_map[key];
        return owner_color_map.neutral;
    }

    function apply_campaign_state() {
        if (!campaign_map_loader.item)
            return;
        if (!campaign_state || !campaign_state.provinces)
            return;
        var owner_by_id = {};
        for (var i = 0; i < campaign_state.provinces.length; i++) {
            var entry = campaign_state.provinces[i];
            if (entry && entry.id)
                owner_by_id[entry.id] = entry.owner || "neutral";
        }
        var entries = [];
        if (province_labels && province_labels.length > 0) {
            for (var j = 0; j < province_labels.length; j++) {
                var prov = province_labels[j];
                if (!prov || !prov.id)
                    continue;
                var owner = owner_by_id[prov.id] || prov.owner || "neutral";
                var color = owner_color_for(owner);
                entries.push({
                        "id": prov.id,
                        "owner": owner,
                        "color": color
                    });
            }
        } else {
            for (var k = 0; k < campaign_state.provinces.length; k++) {
                var state_prov = campaign_state.provinces[k];
                if (!state_prov || !state_prov.id)
                    continue;
                var fallback_owner = state_prov.owner || "neutral";
                var fallback_color = owner_color_for(fallback_owner);
                entries.push({
                        "id": state_prov.id,
                        "owner": fallback_owner,
                        "color": fallback_color
                    });
            }
        }
        if (entries.length > 0)
            campaign_map_loader.item.apply_province_state(entries);
    }

    function update_hover(x, y) {
        if (!campaign_map_loader.item) {
            root.clear_hover();
            return;
        }
        var info = campaign_map_loader.item.province_info_at_screen(x, y);
        root.hover_province_id = info && info.id ? info.id : "";
        root.hover_province_name = info && info.name ? info.name : "";
        root.hover_province_owner = info && info.owner ? info.owner : "";
    }

    function clear_hover() {
        root.hover_province_id = "";
        root.hover_province_name = "";
        root.hover_province_owner = "";
    }

    function reset_view() {
        if (selected_mission && selected_mission.world_region_id) {
            focus_on_region(selected_mission.world_region_id);
            return;
        }
        map_orbit_yaw = 180;
        map_orbit_pitch = 90;
        map_orbit_distance = 1.2;
        map_pan_u = 0;
        map_pan_v = 0;
    }

    function collect_city_markers() {
        var markers = [];
        if (!root.province_labels)
            return markers;
        for (var p = 0; p < root.province_labels.length; ++p) {
            var cities = root.province_labels[p] ? root.province_labels[p].cities : null;
            if (!cities)
                continue;
            for (var c = 0; c < cities.length; ++c) {
                var city = cities[c];
                if (!city || !city.name || !city.uv || city.uv.length !== 2)
                    continue;
                var rank = root.city_rank[city.name];
                markers.push({
                        "name": city.name,
                        "uv": city.uv,
                        "rank": rank === undefined ? root.city_rank_default : rank,
                        "labelled": false
                    });
            }
        }
        markers.sort(function (a, b) {
                if (a.rank !== b.rank)
                    return a.rank - b.rank;
                return a.name < b.name ? -1 : (a.name > b.name ? 1 : 0);
            });
        return markers;
    }

    function rebuild_city_markers() {
        var markers = root.collect_city_markers();
        var view = campaign_map_loader.item;
        if (!view) {
            root.city_markers = markers;
            return;
        }
        var kept = [];
        var portrait = view.hannibal_icon_position();
        if (portrait && portrait.x > 0 && portrait.y > 0 && root.selected_mission) {
            kept.push({
                    "left": portrait.x - 26,
                    "right": portrait.x + 26,
                    "top": portrait.y - 26,
                    "bottom": portrait.y + 26
                });
        }
        kept.push({
                "left": 0,
                "right": map_viewport.width,
                "top": map_viewport.height - 34,
                "bottom": map_viewport.height
            });
        kept.push({
                "left": 0,
                "right": 150,
                "top": map_viewport.height - 130,
                "bottom": map_viewport.height
            });
        var region = root.mission_regions[root.active_region_id];
        if (region && region.uv) {
            var marker = view.screen_pos_for_uv(region.uv[0], region.uv[1]);
            kept.push({
                    "left": marker.x - 40,
                    "right": marker.x - 4,
                    "top": marker.y + 4,
                    "bottom": marker.y + 40
                });
        }
        for (var i = 0; i < markers.length; ++i) {
            var marker = markers[i];
            var pos = view.screen_pos_for_uv(marker.uv[0], marker.uv[1]);
            var capital = marker.rank === 0;
            var metrics = capital ? capital_metrics : town_metrics;
            metrics.text = marker.name;
            var width = metrics.advanceWidth;
            var half_height = metrics.height * 0.5;
            var gap = capital ? 9 : 7;
            var box = {
                "left": pos.x + gap - 2,
                "right": pos.x + gap + width + 3,
                "top": pos.y - half_height - 2,
                "bottom": pos.y + half_height + 2
            };
            var clear = pos.x > -width && pos.x < map_viewport.width + width && pos.y > 0 && pos.y < map_viewport.height;
            for (var k = 0; clear && k < kept.length; ++k) {
                var other = kept[k];
                clear = box.right < other.left || box.left > other.right || box.bottom < other.top || box.top > other.bottom;
            }
            marker.labelled = clear;
            if (clear)
                kept.push(box);
            markers[i] = marker;
        }
        root.city_markers = markers;
    }

    TextMetrics {
        id: capital_metrics

        font.bold: true
        font.family: root.map_label_family
        font.pointSize: Theme.fontSizeSmall
    }

    TextMetrics {
        id: town_metrics

        font.family: root.map_label_family
        font.pointSize: Theme.fontSizeTiny
    }

    Timer {
        id: camera_settle_timer

        interval: 350
        running: true
        onTriggered: root.camera_settled = true
    }

    Timer {
        id: declutter_timer

        interval: 130
        onTriggered: root.rebuild_city_markers()
    }

    onLabel_refreshChanged: declutter_timer.restart()
    onProvince_labelsChanged: root.rebuild_city_markers()

    function label_uv_for(prov) {
        if (prov && prov.label_uv && prov.label_uv.length === 2)
            return prov.label_uv;
        if (!prov || !prov.triangles || prov.triangles.length === 0)
            return null;
        var sum_u = 0;
        var sum_v = 0;
        var count = 0;
        var step = Math.max(1, Math.floor(prov.triangles.length / 200));
        for (var i = 0; i < prov.triangles.length; i += step) {
            var pt = prov.triangles[i];
            if (!pt || pt.length < 2)
                continue;
            sum_u += pt[0];
            sum_v += pt[1];
            count += 1;
        }
        if (count === 0)
            return null;
        return [sum_u / count, sum_v / count];
    }

    color: "#20170f"
    radius: Theme.radiusMedium
    Component.onCompleted: {
        load_provinces();
        load_campaign_state();
    }
    onSelected_missionChanged: {
        if (selected_mission && selected_mission.world_region_id) {
            focus_on_region(selected_mission.world_region_id);
        } else {
            map_orbit_yaw = 180;
            map_orbit_pitch = 90;
            map_orbit_distance = 1.2;
            map_pan_u = 0;
            map_pan_v = 0;
        }
    }
    onCampaign_stateChanged: {
        apply_campaign_state();
    }
    onVisibleChanged: {
        if (visible) {
            load_campaign_state();
        }
    }

    Item {
        id: map_viewport

        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        clip: true

        Loader {
            id: campaign_map_loader

            anchors.fill: parent
            active: root.visible
            onStatusChanged: {
                if (status === Loader.Ready) {
                    root.load_provinces();
                    root.apply_campaign_state();
                }
            }

            sourceComponent: Component {
                CampaignMapView {
                    id: campaign_map

                    anchors.fill: parent
                    orbit_yaw: root.map_orbit_yaw
                    orbit_pitch: root.map_orbit_pitch
                    orbit_distance: root.map_orbit_distance
                    pan_u: root.map_pan_u
                    pan_v: root.map_pan_v
                    terrain_height_scale: root.terrain_height_scale
                    show_province_fills: root.show_province_fills
                    current_mission: root.selected_mission && root.selected_mission.order_index !== undefined ? root.selected_mission.order_index : 7
                    hover_province_id: root.hover_province_id
                    selected_province_id: root.active_region_id
                    onOrbit_yaw_changed: root.label_refresh += 1
                    onOrbit_pitch_changed: root.label_refresh += 1
                    onOrbit_distance_changed: root.label_refresh += 1
                    onPan_u_changed: root.label_refresh += 1
                    onPan_v_changed: root.label_refresh += 1
                    onCurrent_mission_changed: root.label_refresh += 1
                    onWidthChanged: root.label_refresh += 1
                    onHeightChanged: root.label_refresh += 1

                    Behavior on orbit_yaw  {
                        enabled: root.camera_settled

                        NumberAnimation {
                            duration: 600
                            easing.type: Easing.InOutQuad
                        }
                    }

                    Behavior on orbit_pitch  {
                        enabled: root.camera_settled

                        NumberAnimation {
                            duration: 600
                            easing.type: Easing.InOutQuad
                        }
                    }

                    Behavior on orbit_distance  {
                        enabled: root.camera_settled

                        NumberAnimation {
                            duration: 600
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#2a2118"
            visible: campaign_map_loader.active && campaign_map_loader.status !== Loader.Ready
            z: 1

            Text {
                anchors.centerIn: parent
                text: qsTr("Loading map…")
                color: "#c8b89a"
                font.pointSize: Theme.fontSizeMedium
            }
        }

        Rectangle {
            anchors.fill: parent
            z: 2
            color: "transparent"
            border.color: "#8f6d43"
            border.width: 1
        }

        Rectangle {
            anchors.fill: parent
            z: 2
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#4d100b06"
                }

                GradientStop {
                    position: 0.14
                    color: "transparent"
                }

                GradientStop {
                    position: 0.86
                    color: "transparent"
                }

                GradientStop {
                    position: 1
                    color: "#5c1a1008"
                }
            }
        }

        Row {
            id: map_control_row

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Theme.spacingMedium
            spacing: Theme.spacingSmall
            z: 8
            visible: campaign_map_loader.item

            Rectangle {
                id: pitch_down_button

                height: 24
                width: pitch_down_label.implicitWidth + 12
                radius: 4
                color: pitch_down_area.containsMouse ? "#7a1f1d" : "#3b2a1d"
                border.color: "#c29555"
                border.width: 1
                opacity: pitch_down_area.containsMouse ? 1 : 0.9

                Label {
                    id: pitch_down_label

                    anchors.centerIn: parent
                    text: qsTr("Tilt -")
                    color: "#f2dfba"
                    font.pointSize: Theme.fontSizeTiny
                    font.bold: true
                }

                MouseArea {
                    id: pitch_down_area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        Design.UiSound.activate();
                        root.map_orbit_pitch = Math.max(5, root.map_orbit_pitch - 5);
                    }
                }
            }

            Rectangle {
                id: pitch_up_button

                height: 24
                width: pitch_up_label.implicitWidth + 12
                radius: 4
                color: pitch_up_area.containsMouse ? "#7a1f1d" : "#3b2a1d"
                border.color: "#c29555"
                border.width: 1
                opacity: pitch_up_area.containsMouse ? 1 : 0.9

                Label {
                    id: pitch_up_label

                    anchors.centerIn: parent
                    text: qsTr("Tilt +")
                    color: "#f2dfba"
                    font.pointSize: Theme.fontSizeTiny
                    font.bold: true
                }

                MouseArea {
                    id: pitch_up_area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        Design.UiSound.activate();
                        root.map_orbit_pitch = Math.min(90, root.map_orbit_pitch + 5);
                    }
                }
            }

            Rectangle {
                id: reset_view_button

                height: 24
                width: reset_view_label.implicitWidth + 16
                radius: 4
                color: reset_view_area.containsMouse ? "#7a1f1d" : "#3b2a1d"
                border.color: "#c29555"
                border.width: 1
                opacity: reset_view_area.containsMouse ? 1 : 0.9

                Label {
                    id: reset_view_label

                    anchors.centerIn: parent
                    text: qsTr("Reset view")
                    color: "#f2dfba"
                    font.pointSize: Theme.fontSizeTiny
                    font.bold: true
                }

                MouseArea {
                    id: reset_view_area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        Design.UiSound.activate();
                        root.reset_view();
                    }
                }
            }
        }

        MouseArea {
            property real last_x: 0
            property real last_y: 0
            property real drag_distance: 0

            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: function (mouse) {
                last_x = mouse.x;
                last_y = mouse.y;
                drag_distance = 0;
            }
            onPositionChanged: function (mouse) {
                var dx = mouse.x - last_x;
                var dy = mouse.y - last_y;
                if ((mouse.buttons & Qt.RightButton) || (mouse.buttons & Qt.LeftButton && (mouse.modifiers & Qt.ShiftModifier))) {
                    var pan_scale = 0.0015 * root.map_orbit_distance;
                    root.map_pan_u -= dx * pan_scale;
                    root.map_pan_v += dy * pan_scale;
                } else if (mouse.buttons & Qt.LeftButton) {
                    root.map_orbit_yaw += dx * 0.4;
                    root.map_orbit_pitch = Math.max(5, Math.min(90, root.map_orbit_pitch + dy * 0.4));
                }
                drag_distance += Math.abs(dx) + Math.abs(dy);
                last_x = mouse.x;
                last_y = mouse.y;
                root.hover_mouse_x = mouse.x;
                root.hover_mouse_y = mouse.y;
                root.update_hover(mouse.x, mouse.y);
            }
            onExited: root.clear_hover()
            onReleased: function (mouse) {
                if (mouse.button !== Qt.LeftButton)
                    return;
                if (drag_distance > 6)
                    return;
                if (!campaign_map_loader.item)
                    return;
                var info = campaign_map_loader.item.province_info_at_screen(mouse.x, mouse.y);
                var id = info && info.id ? info.id : "";
                if (id !== "")
                    root.region_selected(id);
            }
            onWheel: function (wheel) {
                var step = wheel.angleDelta.y > 0 ? 0.9 : 1.1;
                var next_distance = root.map_orbit_distance * step;
                if (campaign_map_loader.item)
                    root.map_orbit_distance = Math.min(campaign_map_loader.item.max_orbit_distance, Math.max(campaign_map_loader.item.min_orbit_distance, next_distance));
                wheel.accepted = true;
            }
        }

        Repeater {
            model: root.city_markers

            delegate: Item {
                required property var modelData
                readonly property int rank: modelData.rank === undefined ? root.city_rank_default : modelData.rank
                readonly property bool is_capital: rank === 0

                readonly property int _refresh: root.label_refresh
                readonly property point _pos: (_refresh >= 0 && campaign_map_loader.item) ? campaign_map_loader.item.screen_pos_for_uv(modelData.uv[0], modelData.uv[1]) : Qt.point(0, 0)

                z: 4
                x: _pos.x
                y: _pos.y

                Rectangle {
                    width: is_capital ? 9 : 6
                    height: width
                    radius: width / 2
                    color: "#f7ecd4"
                    border.color: "#2d241c"
                    border.width: is_capital ? 2 : 1
                    x: -width / 2
                    y: -height / 2
                }

                Text {

                    visible: modelData.labelled
                    text: modelData.name
                    color: "#191108"
                    font.family: root.map_label_family
                    font.pointSize: parent.is_capital ? Theme.fontSizeSmall : Theme.fontSizeTiny
                    font.bold: parent.is_capital
                    style: Text.Outline
                    styleColor: "#f7ecd4"
                    x: parent.is_capital ? 9 : 7
                    y: -height / 2
                }
            }
        }

        Repeater {
            id: mission_marker_repeater

            model: root.selected_mission ? 1 : 0

            delegate: Item {
                property var region_info: root.mission_regions[root.active_region_id] || null
                property var marker_uv: region_info ? region_info.uv : null
                property int _refresh: root.label_refresh
                property var _pos: (marker_uv !== null && _refresh >= 0 && campaign_map_loader.item) ? campaign_map_loader.item.screen_pos_for_uv(marker_uv[0], marker_uv[1]) : Qt.point(0, 0)

                visible: marker_uv !== null && root.active_region_id !== ""
                z: 6

                x: _pos.x - 22
                y: _pos.y + 22

                Rectangle {
                    width: 24
                    height: 24
                    radius: 12
                    color: "#cc8f47"
                    border.color: "#ffffff"
                    border.width: 2
                    x: -width / 2
                    y: -height / 2
                    opacity: 0.9

                    Text {
                        anchors.centerIn: parent
                        text: "⚔"
                        color: "#ffffff"
                        font.pointSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    SequentialAnimation on scale  {
                        loops: Animation.Infinite
                        running: visible

                        NumberAnimation {
                            from: 1
                            to: 1.15
                            duration: 800
                            easing.type: Easing.InOutQuad
                        }

                        NumberAnimation {
                            from: 1.15
                            to: 1
                            duration: 800
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }
        }

        Item {
            id: hannibal_icon

            property int _refresh: root.label_refresh
            property var _pos: (_refresh >= 0 && campaign_map_loader.item) ? campaign_map_loader.item.hannibal_icon_position() : Qt.point(0, 0)
            property var icon_sources: ["qrc:/StandardOfIron/assets/visuals/hannibal.png", "qrc:/assets/visuals/hannibal.png", "assets/visuals/hannibal.png", "qrc:/qt/qml/StandardOfIron/assets/visuals/hannibal.png"]
            property int icon_index: 0

            visible: campaign_map_loader.item && _pos.x > 0 && _pos.y > 0 && root.selected_mission
            z: 10
            x: _pos.x
            y: _pos.y

            Rectangle {
                width: 44
                height: 44
                x: -width / 2
                y: -height / 2
                radius: 6
                color: "#2a1f1a"
                border.color: "#d4a857"
                border.width: 2
                opacity: 0.95

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: 4
                    color: "transparent"
                    border.color: "#6b4423"
                    border.width: 1
                }
            }

            Image {
                source: hannibal_icon.icon_sources[hannibal_icon.icon_index]
                width: 36
                height: 36
                x: -width / 2
                y: -height / 2
                smooth: true
                mipmap: true
                fillMode: Image.PreserveAspectFit
                cache: true
                asynchronous: false
                onStatusChanged: {
                    if (status === Image.Error && hannibal_icon.icon_index + 1 < hannibal_icon.icon_sources.length) {
                        hannibal_icon.icon_index += 1;
                        source = hannibal_icon.icon_sources[hannibal_icon.icon_index];
                    }
                }
            }

            Rectangle {
                width: 50
                height: 50
                x: -width / 2
                y: -height / 2
                radius: width / 2
                color: "transparent"
                border.color: "#d4a857"
                border.width: 2
                opacity: 0.4

                SequentialAnimation on opacity  {
                    loops: Animation.Infinite
                    running: hannibal_icon.visible

                    NumberAnimation {
                        from: 0.4
                        to: 0
                        duration: 1500
                        easing.type: Easing.OutCubic
                    }

                    PauseAnimation {
                        duration: 500
                    }
                }

                SequentialAnimation on scale  {
                    loops: Animation.Infinite
                    running: hannibal_icon.visible

                    NumberAnimation {
                        from: 1
                        to: 1.3
                        duration: 1500
                        easing.type: Easing.OutCubic
                    }

                    NumberAnimation {
                        from: 1.3
                        to: 1
                        duration: 0
                    }

                    PauseAnimation {
                        duration: 500
                    }
                }
            }
        }

        Rectangle {
            id: hover_tooltip

            visible: root.hover_province_id !== "" && root.hover_province_name !== ""
            x: Math.min(parent.width - width - Theme.spacingSmall, Math.max(Theme.spacingSmall, root.hover_mouse_x + 12))
            y: Math.min(parent.height - height - Theme.spacingSmall, Math.max(Theme.spacingSmall, root.hover_mouse_y + 12))
            width: tooltip_layout.implicitWidth + 16
            height: tooltip_layout.implicitHeight + 16
            radius: Design.Metrics.radiusSmall
            color: Design.Theme.panelIron
            border.color: Design.Theme.borderStrong
            border.width: Design.Metrics.borderThin
            opacity: 0.95
            z: 10

            ColumnLayout {
                id: tooltip_layout

                anchors.centerIn: parent
                spacing: 2

                Label {
                    text: root.hover_province_name
                    color: "#2d241c"
                    font.bold: true
                    font.pointSize: Theme.fontSizeSmall
                }

                Label {
                    text: qsTr("Control: ") + root.hover_province_owner
                    color: "#4a3f32"
                    font.pointSize: Theme.fontSizeTiny
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Theme.spacingMedium
            width: legend_layout.implicitWidth + 16
            height: legend_layout.implicitHeight + 16
            radius: 4
            color: "#f5f0e6"
            border.color: "#8b7355"
            border.width: 2
            opacity: 0.95

            ColumnLayout {
                id: legend_layout

                anchors.centerIn: parent
                spacing: Theme.spacingTiny

                Label {
                    text: qsTr("Legend")
                    color: Design.Theme.textSecondary
                    font.pointSize: Theme.fontSizeSmall
                    font.bold: true
                }

                Repeater {

                    model: [{
                            "name": qsTr("Rome"),
                            "color": "#b4302a"
                        }, {
                            "name": qsTr("Carthage"),
                            "color": "#6b2a5e"
                        }, {
                            "name": qsTr("Neutral"),
                            "color": "#8c8478"
                        }]

                    delegate: RowLayout {
                        spacing: Theme.spacingTiny

                        Rectangle {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            radius: 2
                            color: modelData.color
                            border.color: "#5a4a3a"
                            border.width: 1
                        }

                        Label {
                            text: modelData.name
                            color: "#4a3f32"
                            font.pointSize: Theme.fontSizeTiny
                        }
                    }
                }
            }
        }

        Label {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.spacingMedium
            text: qsTr("Drag to rotate  ·  Shift or right-drag to pan  ·  Scroll to zoom")
            color: "#4a3f32"
            font.pointSize: Theme.fontSizeTiny
            style: Text.Outline
            styleColor: "#f5f0e6"
        }

        Rectangle {

            visible: root.active_region_id !== ""
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.spacingMedium
            width: active_region_label.implicitWidth + 16
            height: active_region_label.implicitHeight + 12
            radius: 4
            color: "#cc8f47"
            border.color: "#8b6332"
            border.width: 2
            opacity: 0.95
            z: 10

            Label {
                id: active_region_label

                anchors.centerIn: parent
                text: Design.Icons.capture + " " + root.active_region_name
                color: "#2d241c"
                font.pointSize: Theme.fontSizeSmall
                font.bold: true
            }
        }
    }
}
