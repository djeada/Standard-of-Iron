import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property var campaigns: []

    signal missionSelected(string campaignId)
    signal cancelled()

    function refreshCampaigns() {
        if (typeof game !== "undefined" && game.available_campaigns)
            campaigns = game.available_campaigns;

    }

    onVisibleChanged: {
        if (visible && typeof game !== "undefined" && game.load_campaigns) {
            game.load_campaigns();
            refreshCampaigns();
        }
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
            refreshCampaigns();
        }

        target: (typeof game !== "undefined") ? game : null
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.85, 1200)
        height: Math.min(parent.height * 0.85, 800)
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
                    text: qsTr("Campaign Missions")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                StyledButton {
                    text: qsTr("← Back")
                    onClicked: root.cancelled()
                }

            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: listView

                    model: root.campaigns
                    spacing: Theme.spacingMedium

                    delegate: Rectangle {
                        width: listView.width
                        height: 120
                        radius: Theme.radiusLarge
                        color: mouseArea.containsMouse ? Theme.hoverBg : Theme.cardBase
                        border.color: mouseArea.containsMouse ? Theme.selectedBr : Theme.cardBorder
                        border.width: 1

                        MouseArea {
                            id: mouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                missionDetailPanel.visible = true;
                                missionDetailPanel.campaignData = modelData;
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMedium
                            spacing: Theme.spacingMedium

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Label {
                                        text: modelData.title || ""
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeTitle
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }

                                    Rectangle {
                                        visible: modelData.completed || false
                                        Layout.preferredWidth: 100
                                        Layout.preferredHeight: 24
                                        radius: Theme.radiusSmall
                                        color: Theme.successBg
                                        border.color: Theme.successBr
                                        border.width: 1

                                        Label {
                                            anchors.centerIn: parent
                                            text: qsTr("✓ Completed")
                                            color: Theme.successText
                                            font.pointSize: Theme.fontSizeSmall
                                            font.bold: true
                                        }

                                    }

                                    Rectangle {
                                        visible: !(modelData.unlocked || false)
                                        Layout.preferredWidth: 80
                                        Layout.preferredHeight: 24
                                        radius: Theme.radiusSmall
                                        color: Theme.disabledBg
                                        border.color: Theme.border
                                        border.width: 1

                                        Label {
                                            anchors.centerIn: parent
                                            text: qsTr("🔒 Locked")
                                            color: Theme.textDim
                                            font.pointSize: Theme.fontSizeSmall
                                        }

                                    }

                                }

                                Label {
                                    text: modelData.description || ""
                                    color: Theme.textSubLite
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    font.pointSize: Theme.fontSizeMedium
                                }

                            }

                            Text {
                                text: "›"
                                font.pointSize: Theme.fontSizeHero
                                color: Theme.textHint
                            }

                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.animNormal
                            }

                        }

                        Behavior on border.color {
                            ColorAnimation {
                                duration: Theme.animNormal
                            }

                        }

                    }

                }

            }

            Label {
                visible: root.campaigns.length === 0
                text: qsTr("No campaign missions available")
                color: Theme.textDim
                font.pointSize: Theme.fontSizeMedium
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

        }

    }

    Rectangle {
        id: missionDetailPanel

        property var campaignData: null
        property real map_orbit_yaw: 180
        property real map_orbit_pitch: 90
        property real map_orbit_distance: 1.2
        property var province_labels: []
        property string hoverProvinceName: ""
        property string hoverProvinceOwner: ""
        property real hoverMouseX: 0
        property real hoverMouseY: 0
        property var ownerLegend: [{
            "name": qsTr("Rome"),
            "color": "#d01f1a"
        }, {
            "name": qsTr("Carthage"),
            "color": "#cc8f47"
        }, {
            "name": qsTr("Neutral"),
            "color": "#3a3a3a"
        }]
        property var provinceSources: ["assets/campaign_map/provinces.json", "qrc:/assets/campaign_map/provinces.json", "qrc:/StandardOfIron/assets/campaign_map/provinces.json", "qrc:/qt/qml/StandardOfIron/assets/campaign_map/provinces.json"]
        property int label_refresh: 0
        property int current_mission_index: 7

        function loadProvinces() {
            loadProvincesFrom(0);
        }

        function labelUvFor(prov) {
            if (prov && prov.label_uv && prov.label_uv.length === 2)
                return prov.label_uv;

            if (!prov || !prov.triangles || prov.triangles.length === 0)
                return null;

            var sumU = 0;
            var sumV = 0;
            var count = 0;
            var step = Math.max(1, Math.floor(prov.triangles.length / 200));
            for (var i = 0; i < prov.triangles.length; i += step) {
                var pt = prov.triangles[i];
                if (!pt || pt.length < 2)
                    continue;

                sumU += pt[0];
                sumV += pt[1];
                count += 1;
            }
            if (count === 0)
                return null;

            return [sumU / count, sumV / count];
        }

        function provinceInfoFor(id) {
            if (!id)
                return null;

            for (var i = 0; i < province_labels.length; i++) {
                var prov = province_labels[i];
                if (prov && prov.id === id)
                    return prov;

            }
            return null;
        }

        function loadProvincesFrom(index) {
            if (index >= provinceSources.length)
                return ;

            var xhr = new XMLHttpRequest();
            xhr.open("GET", provinceSources[index]);
            xhr.onreadystatechange = function() {
                if (xhr.readyState !== XMLHttpRequest.DONE)
                    return ;

                if (xhr.status !== 200 && xhr.status !== 0) {
                    loadProvincesFrom(index + 1);
                    return ;
                }
                try {
                    var data = JSON.parse(xhr.responseText);
                    if (data && data.provinces) {
                        var hasCities = false;
                        for (var i = 0; i < data.provinces.length; i++) {
                            var prov = data.provinces[i];
                            if (prov && prov.cities && prov.cities.length > 0) {
                                hasCities = true;
                                break;
                            }
                        }
                        if (!hasCities) {
                            loadProvincesFrom(index + 1);
                            return ;
                        }
                        missionDetailPanel.province_labels = data.provinces;
                        missionDetailPanel.label_refresh += 1;
                    }
                } catch (e) {
                    loadProvincesFrom(index + 1);
                }
            };
            xhr.send();
        }

        onProvince_labels_changed: label_refresh += 1
        Component.onCompleted: {
            refreshCampaigns();
            if (campaignMapLoader.item) {
                var labels = campaignMapLoader.item.province_labels;
                if (labels && labels.length > 0) {
                    missionDetailPanel.province_labels = labels;
                    missionDetailPanel.label_refresh += 1;
                } else {
                    loadProvinces();
                }
            } else {
                loadProvinces();
            }
        }
        onCampaignDataChanged: {
            map_orbit_yaw = 180;
            map_orbit_pitch = 90;
            map_orbit_distance = 1.2;
        }
        visible: false
        anchors.fill: parent
        color: Theme.dim
        z: 100

        MouseArea {
            anchors.fill: parent
            onClicked: missionDetailPanel.visible = false
        }

        Rectangle {
            width: Math.min(parent.width * 0.7, 900)
            height: Math.min(parent.height * 0.8, 700)
            anchors.centerIn: parent
            radius: Theme.radiusPanel
            color: Theme.panelBase
            border.color: Theme.panelBr
            border.width: 2

            MouseArea {
                anchors.fill: parent
                onClicked: {
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingXLarge
                spacing: Theme.spacingLarge

                Label {
                    text: missionDetailPanel.campaignData ? (missionDetailPanel.campaignData.title || "") : ""
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeHero
                    font.bold: true
                    Layout.fillWidth: true
                }

                Label {
                    text: missionDetailPanel.campaignData ? (missionDetailPanel.campaignData.description || "") : ""
                    color: Theme.textSubLite
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.pointSize: Theme.fontSizeMedium
                }

                Rectangle {
                    id: mapFrame

                    Layout.fillWidth: true
                    Layout.preferredHeight: 240
                    radius: Theme.radiusMedium
                    color: Theme.cardBase
                    border.color: Theme.cardBorder
                    border.width: 1

                    Loader {
                        id: campaignMapLoader

                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        active: root.visible && (typeof mainWindow === 'undefined' || !mainWindow.gameStarted)

                        sourceComponent: Component {
                            CampaignMapView {
                                id: campaignMap

                                anchors.fill: parent
                                orbit_yaw: missionDetailPanel.map_orbit_yaw
                                orbit_pitch: missionDetailPanel.map_orbit_pitch
                                orbit_distance: missionDetailPanel.map_orbit_distance
                                current_mission: missionDetailPanel.current_mission_index
                                onOrbit_yaw_changed: missionDetailPanel.label_refresh += 1
                                onOrbit_pitch_changed: missionDetailPanel.label_refresh += 1
                                onOrbit_distance_changed: missionDetailPanel.label_refresh += 1
                                onCurrent_mission_changed: missionDetailPanel.label_refresh += 1
                                onWidthChanged: missionDetailPanel.label_refresh += 1
                                onHeightChanged: missionDetailPanel.label_refresh += 1
                            }

                        }

                    }

                    MouseArea {
                        property real lastX: 0
                        property real lastY: 0

                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        onPressed: function(mouse) {
                            lastX = mouse.x;
                            lastY = mouse.y;
                        }
                        onPositionChanged: function(mouse) {
                            if (!(mouse.buttons & Qt.LeftButton))
                                return ;

                            var dx = mouse.x - lastX;
                            var dy = mouse.y - lastY;
                            lastX = mouse.x;
                            lastY = mouse.y;
                            missionDetailPanel.map_orbit_yaw += dx * 0.4;
                            missionDetailPanel.map_orbit_pitch = Math.max(5, Math.min(90, missionDetailPanel.map_orbit_pitch + dy * 0.4));
                        }
                        onMouseXChanged: function() {
                            if (!hoverEnabled || !campaignMapLoader.item)
                                return ;

                            missionDetailPanel.hoverMouseX = mouseX;
                            missionDetailPanel.hoverMouseY = mouseY;
                            var info = campaignMapLoader.item.province_info_at_screen(mouseX, mouseY);
                            var id = info && info.id ? info.id : "";
                            campaignMapLoader.item.hover_province_id = id;
                            missionDetailPanel.hoverProvinceName = info && info.name ? info.name : "";
                            missionDetailPanel.hoverProvinceOwner = info && info.owner ? info.owner : "";
                        }
                        onMouseYChanged: function() {
                            if (!hoverEnabled || !campaignMapLoader.item)
                                return ;

                            missionDetailPanel.hoverMouseX = mouseX;
                            missionDetailPanel.hoverMouseY = mouseY;
                            var info = campaignMapLoader.item.province_info_at_screen(mouseX, mouseY);
                            var id = info && info.id ? info.id : "";
                            campaignMapLoader.item.hover_province_id = id;
                            missionDetailPanel.hoverProvinceName = info && info.name ? info.name : "";
                            missionDetailPanel.hoverProvinceOwner = info && info.owner ? info.owner : "";
                        }
                        onExited: {
                            if (campaignMapLoader.item)
                                campaignMapLoader.item.hover_province_id = "";

                            missionDetailPanel.hoverProvinceName = "";
                            missionDetailPanel.hoverProvinceOwner = "";
                        }
                        onWheel: function(wheel) {
                            var step = wheel.angleDelta.y > 0 ? 0.9 : 1.1;
                            var nextDistance = missionDetailPanel.map_orbit_distance * step;
                            if (campaignMapLoader.item)
                                missionDetailPanel.map_orbit_distance = Math.min(campaignMapLoader.item.max_orbit_distance, Math.max(campaignMapLoader.item.min_orbit_distance, nextDistance));

                            wheel.accepted = true;
                        }
                    }

                    Item {
                        id: hannibalIcon

                        property int _refresh: missionDetailPanel.label_refresh
                        property var _pos: (_refresh >= 0 && campaignMapLoader.item) ? campaignMapLoader.item.hannibal_icon_position() : Qt.point(0, 0)
                        property var _iconSources: ["qrc:/StandardOfIron/assets/visuals/hannibal.png", "qrc:/assets/visuals/hannibal.png", "assets/visuals/hannibal.png", "qrc:/qt/qml/StandardOfIron/assets/visuals/hannibal.png"]
                        property int _iconIndex: 0

                        visible: campaignMapLoader.item && _pos.x > 0 && _pos.y > 0
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

                    Repeater {
                        model: missionDetailPanel.province_labels

                        delegate: Text {
                            property var _labelUv: missionDetailPanel.labelUvFor(modelData)
                            property int _refresh: missionDetailPanel.label_refresh
                            property var _pos: (_labelUv !== null && _refresh >= 0 && campaignMapLoader.item) ? campaignMapLoader.item.screen_pos_for_uv(_labelUv[0], _labelUv[1]) : Qt.point(0, 0)

                            visible: false
                            text: modelData.name
                            color: (campaignMapLoader.item && campaignMapLoader.item.hover_province_id === modelData.id) ? Theme.accent : Theme.textMain
                            font.pointSize: Theme.fontSizeSmall
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#101010"
                            opacity: (campaignMapLoader.item && campaignMapLoader.item.hover_province_id === modelData.id) ? 1 : 0.85
                            z: (campaignMapLoader.item && campaignMapLoader.item.hover_province_id === modelData.id) ? 3 : 2
                            x: _pos.x - width / 2
                            y: _pos.y - height / 2
                        }

                    }

                    Repeater {
                        model: missionDetailPanel.province_labels

                        delegate: Repeater {
                            property var _cities: (modelData && modelData.cities) ? modelData.cities : []

                            model: _cities

                            delegate: Item {
                                property var cityData: modelData
                                property var _cityUv: cityData.uv && cityData.uv.length === 2 ? cityData.uv : null
                                property int _refresh: missionDetailPanel.label_refresh
                                property var _pos: (_cityUv !== null && _refresh >= 0 && campaignMapLoader.item) ? campaignMapLoader.item.screen_pos_for_uv(_cityUv[0], _cityUv[1]) : Qt.point(0, 0)

                                visible: _cityUv !== null && cityData.name && cityData.name.length > 0
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
                                    text: cityData.name
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

                    Rectangle {
                        id: hoverTooltip

                        visible: campaignMap.hover_province_id !== "" && missionDetailPanel.hoverProvinceName !== ""
                        x: Math.min(parent.width - width - Theme.spacingSmall, Math.max(Theme.spacingSmall, missionDetailPanel.hoverMouseX + 12))
                        y: Math.min(parent.height - height - Theme.spacingSmall, Math.max(Theme.spacingSmall, missionDetailPanel.hoverMouseY + 12))
                        radius: 6
                        color: "#1a1a1a"
                        border.color: "#2c2c2c"
                        border.width: 1
                        opacity: 0.95
                        z: 5

                        ColumnLayout {
                            anchors.margins: 8
                            anchors.fill: parent
                            spacing: 2

                            Label {
                                text: missionDetailPanel.hoverProvinceName
                                color: "#ffffff"
                                style: Text.Outline
                                styleColor: "#000000"
                                font.bold: true
                                font.pointSize: Theme.fontSizeSmall
                            }

                            Label {
                                text: qsTr("Control: ") + missionDetailPanel.hoverProvinceOwner
                                color: "#ffffff"
                                style: Text.Outline
                                styleColor: "#000000"
                                font.pointSize: Theme.fontSizeTiny
                            }

                        }

                    }

                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMedium

                    Label {
                        text: qsTr("Legend")
                        color: Theme.textMain
                        font.pointSize: Theme.fontSizeSmall
                        font.bold: true
                    }

                    Repeater {
                        model: missionDetailPanel.ownerLegend

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
                                font.pointSize: Theme.fontSizeSmall
                            }

                        }

                    }

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }

                Label {
                    text: qsTr("Select a Mission")
                    color: Theme.textMain
                    font.pointSize: Theme.fontSizeTitle
                    font.bold: true
                    Layout.fillWidth: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: missionListView

                        model: missionDetailPanel.campaignData ? (missionDetailPanel.campaignData.missions || []) : []
                        spacing: Theme.spacingMedium

                        delegate: Rectangle {
                            width: missionListView.width
                            height: 80
                            radius: Theme.radiusMedium
                            color: missionMouseArea.containsMouse ? Theme.hoverBg : Theme.cardBase
                            border.color: missionMouseArea.containsMouse ? Theme.selectedBr : Theme.cardBorder
                            border.width: 1

                            MouseArea {
                                id: missionMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (missionDetailPanel.campaignData && modelData.mission_id)
                                        root.missionSelected(missionDetailPanel.campaignData.id + "/" + modelData.mission_id);

                                }
                                onContainsMouseChanged: {
                                    if (containsMouse && modelData.order_index !== undefined)
                                        missionDetailPanel.current_mission_index = modelData.order_index;
                                    else if (!containsMouse)
                                        missionDetailPanel.current_mission_index = 7;
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingMedium

                                Label {
                                    text: (modelData.order_index + 1).toString()
                                    color: Theme.accent
                                    font.pointSize: Theme.fontSizeTitle
                                    font.bold: true
                                    Layout.preferredWidth: 40
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingTiny

                                    Label {
                                        text: modelData.mission_id || ""
                                        color: Theme.textMain
                                        font.pointSize: Theme.fontSizeLarge
                                        font.bold: true
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: modelData.intro_text || qsTr("Mission briefing...")
                                        color: Theme.textSubLite
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                        font.pointSize: Theme.fontSizeSmall
                                    }

                                }

                                Text {
                                    text: "›"
                                    font.pointSize: Theme.fontSizeTitle
                                    color: Theme.textHint
                                }

                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animNormal
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: Theme.animNormal
                                }

                            }

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
                        text: qsTr("Cancel")
                        onClicked: missionDetailPanel.visible = false
                    }

                }

            }

        }

    }

}
