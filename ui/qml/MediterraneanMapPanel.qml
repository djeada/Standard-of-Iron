import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var selected_mission: null
    property real map_orbit_yaw: 180
    property real map_orbit_pitch: 55
    property real map_orbit_distance: 2.4
    property string hover_province_name: ""
    property string hover_province_owner: ""
    property real hover_mouse_x: 0
    property real hover_mouse_y: 0

    color: "#1a1a1a"
    radius: Theme.radiusMedium

    onSelected_missionChanged: {
        // Reset camera when mission selection changes
        map_orbit_yaw = 180;
        map_orbit_pitch = 55;
        map_orbit_distance = 2.4;
    }

    CampaignMapView {
        id: campaign_map

        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        orbitYaw: root.map_orbit_yaw
        orbitPitch: root.map_orbit_pitch
        orbitDistance: root.map_orbit_distance
    }

    MouseArea {
        property real last_x: 0
        property real last_y: 0

        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        onPressed: function(mouse) {
            last_x = mouse.x;
            last_y = mouse.y;
        }

        onPositionChanged: function(mouse) {
            // Handle drag to rotate
            if (mouse.buttons & Qt.LeftButton) {
                var dx = mouse.x - last_x;
                var dy = mouse.y - last_y;
                last_x = mouse.x;
                last_y = mouse.y;
                
                root.map_orbit_yaw += dx * 0.4;
                root.map_orbit_pitch = Math.max(5, Math.min(85, root.map_orbit_pitch + dy * 0.4));
            }

            // Handle hover
            root.hover_mouse_x = mouse.x;
            root.hover_mouse_y = mouse.y;
            
            var info = campaign_map.provinceInfoAtScreen(mouse.x, mouse.y);
            var id = info && info.id ? info.id : "";
            campaign_map.hoverProvinceId = id;
            root.hover_province_name = info && info.name ? info.name : "";
            root.hover_province_owner = info && info.owner ? info.owner : "";
        }

        onExited: {
            campaign_map.hoverProvinceId = "";
            root.hover_province_name = "";
            root.hover_province_owner = "";
        }

        onWheel: function(wheel) {
            var step = wheel.angleDelta.y > 0 ? 0.9 : 1.1;
            var next_distance = root.map_orbit_distance * step;
            root.map_orbit_distance = Math.min(5, Math.max(1.2, next_distance));
            wheel.accepted = true;
        }
    }

    // Province hover tooltip
    Rectangle {
        id: hover_tooltip

        visible: campaign_map.hoverProvinceId !== "" && root.hover_province_name !== ""
        x: Math.min(parent.width - width - Theme.spacingSmall, Math.max(Theme.spacingSmall, root.hover_mouse_x + 12))
        y: Math.min(parent.height - height - Theme.spacingSmall, Math.max(Theme.spacingSmall, root.hover_mouse_y + 12))
        width: tooltip_layout.implicitWidth + 16
        height: tooltip_layout.implicitHeight + 16
        radius: 6
        color: "#1a1a1a"
        border.color: "#2c2c2c"
        border.width: 1
        opacity: 0.95
        z: 10

        ColumnLayout {
            id: tooltip_layout

            anchors.centerIn: parent
            spacing: 2

            Label {
                text: root.hover_province_name
                color: "#ffffff"
                style: Text.Outline
                styleColor: "#000000"
                font.bold: true
                font.pointSize: Theme.fontSizeSmall
            }

            Label {
                text: qsTr("Control: ") + root.hover_province_owner
                color: "#ffffff"
                style: Text.Outline
                styleColor: "#000000"
                font.pointSize: Theme.fontSizeTiny
            }
        }
    }

    // Legend
    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingMedium
        width: legend_layout.implicitWidth + 16
        height: legend_layout.implicitHeight + 16
        radius: 6
        color: "#1a1a1a"
        border.color: "#2c2c2c"
        border.width: 1
        opacity: 0.9

        ColumnLayout {
            id: legend_layout

            anchors.centerIn: parent
            spacing: Theme.spacingTiny

            Label {
                text: qsTr("Legend")
                color: Theme.textMain
                font.pointSize: Theme.fontSizeSmall
                font.bold: true
            }

            Repeater {
                model: [
                    { "name": qsTr("Rome"), "color": "#d01f1a" },
                    { "name": qsTr("Carthage"), "color": "#cc8f47" },
                    { "name": qsTr("Neutral"), "color": "#3a3a3a" }
                ]

                delegate: RowLayout {
                    spacing: Theme.spacingTiny

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 2
                        color: modelData.color
                        border.color: Theme.border
                        border.width: 1
                    }

                    Label {
                        text: modelData.name
                        color: Theme.textSubLite
                        font.pointSize: Theme.fontSizeTiny
                    }
                }
            }
        }
    }

    // Map controls hint
    Label {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingMedium
        text: qsTr("🖱️ Drag to rotate • Scroll to zoom")
        color: Theme.textDim
        font.pointSize: Theme.fontSizeTiny
        style: Text.Outline
        styleColor: "#000000"
    }
}
