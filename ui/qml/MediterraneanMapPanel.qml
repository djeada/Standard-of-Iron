import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var selected_mission: null
    property string active_region_id: selected_mission && selected_mission.world_region_id ? selected_mission.world_region_id : ""
    property real map_orbit_yaw: 180
    property real map_orbit_pitch: 90
    property real map_orbit_distance: 1.2
    property real map_pan_u: 0
    property real map_pan_v: 0
    property string hover_province_name: ""
    property string hover_province_owner: ""
    property real hover_mouse_x: 0
    property real hover_mouse_y: 0
    property var province_labels: []
    property int label_refresh: 0
    property var campaign_state: null
    property var campaign_state_sources: ["assets/campaign_map/campaign_state.json", "qrc:/assets/campaign_map/campaign_state.json", "qrc:/StandardOfIron/assets/campaign_map/campaign_state.json", "qrc:/qt/qml/StandardOfIron/assets/campaign_map/campaign_state.json"]
    property var owner_color_map: ({
        "rome": [0.82, 0.12, 0.1, 0.45],
        "carthage": [0.8, 0.56, 0.28, 0.45],
        "neutral": [0.25, 0.25, 0.25, 0.25]
    })
    property var region_camera_positions: ({
        "transalpine_gaul": {
            "yaw": 200,
            "pitch": 50,
            "distance": 2
        },
        "cisalpine_gaul": {
            "yaw": 185,
            "pitch": 48,
            "distance": 1.9
        },
        "etruria": {
            "yaw": 180,
            "pitch": 52,
            "distance": 1.8
        },
        "southern_italy": {
            "yaw": 175,
            "pitch": 50,
            "distance": 1.9
        },
        "carthage_core": {
            "yaw": 170,
            "pitch": 55,
            "distance": 2.2
        }
    })

    signal regionSelected(string region_id)

    function focus_on_region(region_id) {
        if (!region_id || region_id === "")
            return ;

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
        if (campaignMapLoader.item) {
            var labels = campaignMapLoader.item.provinceLabels;
            if (labels && labels.length > 0) {
                province_labels = labels;
                label_refresh += 1;
                apply_campaign_state();
            }
        }
    }

    function load_campaign_state() {
        if (campaign_state)
            return ;

        load_campaign_state_from(0);
    }

    function load_campaign_state_from(index) {
        if (index >= campaign_state_sources.length)
            return ;

        var xhr = new XMLHttpRequest();
        xhr.open("GET", campaign_state_sources[index]);
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return ;

            if (xhr.status !== 200 && xhr.status !== 0) {
                load_campaign_state_from(index + 1);
                return ;
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
        if (!campaignMapLoader.item)
            return ;

        if (!campaign_state || !campaign_state.provinces)
            return ;

        var owner_by_id = {
        };
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
            campaignMapLoader.item.applyProvinceState(entries);

    }

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

    color: "#28445C"
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

    Loader {
        id: campaignMapLoader

        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        active: root.visible && (typeof mainWindow === 'undefined' || !mainWindow.gameStarted)
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
                orbitYaw: root.map_orbit_yaw
                orbitPitch: root.map_orbit_pitch
                orbitDistance: root.map_orbit_distance
                panU: root.map_pan_u
                panV: root.map_pan_v
                currentMission: root.selected_mission && root.selected_mission.order_index !== undefined ? root.selected_mission.order_index : 7
                hoverProvinceId: {
                    if (root.active_region_id !== "")
                        return root.active_region_id;

                    var info = provinceInfoAtScreen(root.hover_mouse_x, root.hover_mouse_y);
                    return info && info.id ? info.id : "";
                }
                onOrbitYawChanged: root.label_refresh += 1
                onOrbitPitchChanged: root.label_refresh += 1
                onOrbitDistanceChanged: root.label_refresh += 1
                onPanUChanged: root.label_refresh += 1
                onPanVChanged: root.label_refresh += 1
                onCurrentMissionChanged: root.label_refresh += 1
                onWidthChanged: root.label_refresh += 1
                onHeightChanged: root.label_refresh += 1

                Behavior on orbitYaw {
                    NumberAnimation {
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }

                }

                Behavior on orbitPitch {
                    NumberAnimation {
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }

                }

                Behavior on orbitDistance {
                    NumberAnimation {
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }

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
        onPressed: function(mouse) {
            last_x = mouse.x;
            last_y = mouse.y;
            drag_distance = 0;
        }
        onPositionChanged: function(mouse) {
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
            if (root.active_region_id === "" && campaignMapLoader.item) {
                var info = campaignMapLoader.item.provinceInfoAtScreen(mouse.x, mouse.y);
                var id = info && info.id ? info.id : "";
                root.hover_province_name = info && info.name ? info.name : "";
                root.hover_province_owner = info && info.owner ? info.owner : "";
            }
        }
        onExited: {
            if (root.active_region_id === "") {
                root.hover_province_name = "";
                root.hover_province_owner = "";
            }
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton)
                return ;

            if (drag_distance > 6)
                return ;

            if (!campaignMapLoader.item)
                return ;

            var info = campaignMapLoader.item.provinceInfoAtScreen(mouse.x, mouse.y);
            var id = info && info.id ? info.id : "";
            if (id !== "")
                root.regionSelected(id);

        }
        onWheel: function(wheel) {
            var step = wheel.angleDelta.y > 0 ? 0.9 : 1.1;
            var next_distance = root.map_orbit_distance * step;
            root.map_orbit_distance = Math.min(5, Math.max(0.3, next_distance));
            wheel.accepted = true;
        }
    }

    Repeater {
        model: root.province_labels

        delegate: Repeater {
            property var _cities: (modelData && modelData.cities) ? modelData.cities : []

            model: _cities

            delegate: Item {
                property var city_data: modelData
                property var _city_uv: city_data.uv && city_data.uv.length === 2 ? city_data.uv : null
                property int _refresh: root.label_refresh
                property var _pos: (_city_uv !== null && _refresh >= 0 && campaignMapLoader.item) ? campaignMapLoader.item.screenPosForUv(_city_uv[0], _city_uv[1]) : Qt.point(0, 0)

                visible: _city_uv !== null && city_data.name && city_data.name.length > 0
                z: 4
                x: _pos.x
                y: _pos.y

                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: "#f2e6c8"
                    border.color: "#2d241c"
                    border.width: 1
                    x: -width / 2
                    y: -height / 2
                }

                Text {
                    text: city_data.name
                    color: "#111111"
                    font.pointSize: Theme.fontSizeTiny
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#f2e6c8"
                    x: 6
                    y: -height / 2
                }

            }

        }

    }

    Repeater {
        id: missionMarkerRepeater

        property var mission_region_map: ({
            "transalpine_gaul": {
                "uv": [0.28, 0.35],
                "name": "Rhône"
            },
            "cisalpine_gaul": {
                "uv": [0.42, 0.38],
                "name": "N. Italy"
            },
            "etruria": {
                "uv": [0.44, 0.48],
                "name": "Trasimene"
            },
            "southern_italy": {
                "uv": [0.5, 0.53],
                "name": "Cannae"
            },
            "carthage_core": {
                "uv": [0.4, 0.78],
                "name": "Zama"
            }
        })

        model: root.selected_mission ? 1 : 0

        delegate: Item {
            property var region_info: missionMarkerRepeater.mission_region_map[root.active_region_id] || null
            property var marker_uv: region_info ? region_info.uv : null
            property int _refresh: root.label_refresh
            property var _pos: (marker_uv !== null && _refresh >= 0 && campaignMapLoader.item) ? campaignMapLoader.item.screenPosForUv(marker_uv[0], marker_uv[1]) : Qt.point(0, 0)

            visible: marker_uv !== null && root.active_region_id !== ""
            z: 6
            x: _pos.x
            y: _pos.y

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

                SequentialAnimation on scale {
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

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: -24
                text: region_info ? region_info.name : ""
                color: "#ffffff"
                font.pointSize: Theme.fontSizeSmall
                font.bold: true
                style: Text.Outline
                styleColor: "#000000"
            }

        }

    }

    Item {
        id: hannibalIcon

        property int _refresh: root.label_refresh
        property var _pos: (_refresh >= 0 && campaignMapLoader.item) ? campaignMapLoader.item.hannibalIconPosition() : Qt.point(0, 0)
        property var _iconSources: ["qrc:/StandardOfIron/assets/visuals/hannibal.png", "qrc:/assets/visuals/hannibal.png", "assets/visuals/hannibal.png", "qrc:/qt/qml/StandardOfIron/assets/visuals/hannibal.png"]
        property int _iconIndex: 0

        visible: campaignMapLoader.item && _pos.x > 0 && _pos.y > 0 && root.selected_mission
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
            source: hannibalIcon._iconSources[hannibalIcon._iconIndex]
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
                if (status === Image.Error && hannibalIcon._iconIndex + 1 < hannibalIcon._iconSources.length) {
                    hannibalIcon._iconIndex += 1;
                    source = hannibalIcon._iconSources[hannibalIcon._iconIndex];
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

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: hannibalIcon.visible

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

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: hannibalIcon.visible

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

        visible: (root.active_region_id !== "" || (campaignMapLoader.item && campaignMapLoader.item.hoverProvinceId !== "" && root.hover_province_name !== "")) && root.active_region_id === ""
        x: Math.min(parent.width - width - Theme.spacingSmall, Math.max(Theme.spacingSmall, root.hover_mouse_x + 12))
        y: Math.min(parent.height - height - Theme.spacingSmall, Math.max(Theme.spacingSmall, root.hover_mouse_y + 12))
        width: tooltip_layout.implicitWidth + 16
        height: tooltip_layout.implicitHeight + 16
        radius: 4
        color: "#f5f0e6"
        border.color: "#8b7355"
        border.width: 2
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
                color: "#2d241c"
                font.pointSize: Theme.fontSizeSmall
                font.bold: true
            }

            Repeater {
                model: [{
                    "name": qsTr("Rome"),
                    "color": "#d01f1a"
                }, {
                    "name": qsTr("Carthage"),
                    "color": "#cc8f47"
                }, {
                    "name": qsTr("Neutral"),
                    "color": "#3a3a3a"
                }]

                delegate: RowLayout {
                    spacing: Theme.spacingTiny

                    Rectangle {
                        width: 12
                        height: 12
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
        text: qsTr("🖱️ Drag to rotate • Shift/Right-drag to pan • Scroll to zoom")
        color: "#4a3f32"
        font.pointSize: Theme.fontSizeTiny
        style: Text.Outline
        styleColor: "#f5f0e6"
    }

    Rectangle {
        visible: root.active_region_id !== ""
        anchors.right: parent.right
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
            text: qsTr("📍 Mission Region")
            color: "#2d241c"
            font.pointSize: Theme.fontSizeSmall
            font.bold: true
        }

    }

}
