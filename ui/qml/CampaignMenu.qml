import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property var campaigns: (typeof game !== "undefined" && game.available_campaigns) ? game.available_campaigns : []

    signal missionSelected(string campaignId)
    signal cancelled()

    anchors.fill: parent
    focus: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        }
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
        property real mapOrbitYaw: 180.0
        property real mapOrbitPitch: 55.0
        property real mapOrbitDistance: 2.4
        property var provinceLabels: []
        property var ownerLegend: [
            { name: qsTr("Rome"), color: "#d01f1a" },
            { name: qsTr("Carthage"), color: "#cc8f47" },
            { name: qsTr("Neutral"), color: "#3a3a3a" }
        ]
        property var provinceSources: [
            "qrc:/assets/campaign_map/provinces.json",
            "qrc:/StandardOfIron/assets/campaign_map/provinces.json",
            "qrc:/qt/qml/StandardOfIron/assets/campaign_map/provinces.json",
            "assets/campaign_map/provinces.json"
        ]
        property int labelRefresh: 0
        Component.onCompleted: loadProvinces()
        onCampaignDataChanged: {
            mapOrbitYaw = 180.0;
            mapOrbitPitch = 55.0;
            mapOrbitDistance = 2.4;
        }

        function loadProvinces() {
            loadProvincesFrom(0);
        }

        function loadProvincesFrom(index) {
            if (index >= provinceSources.length)
                return;
            var xhr = new XMLHttpRequest();
            xhr.open("GET", provinceSources[index]);
            xhr.onreadystatechange = function() {
                if (xhr.readyState !== XMLHttpRequest.DONE)
                    return;
                if (xhr.status !== 200 && xhr.status !== 0) {
                    loadProvincesFrom(index + 1);
                    return;
                }
                try {
                    var data = JSON.parse(xhr.responseText);
                    if (data && data.provinces) {
                        missionDetailPanel.provinceLabels = data.provinces;
                        missionDetailPanel.labelRefresh += 1;
                    }
                } catch (e) {
                    loadProvincesFrom(index + 1);
                }
            };
            xhr.send();
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

                    CampaignMapView {
                        id: campaignMap

                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        orbitYaw: missionDetailPanel.mapOrbitYaw
                        orbitPitch: missionDetailPanel.mapOrbitPitch
                        orbitDistance: missionDetailPanel.mapOrbitDistance

                        onOrbitYawChanged: missionDetailPanel.labelRefresh += 1
                        onOrbitPitchChanged: missionDetailPanel.labelRefresh += 1
                        onOrbitDistanceChanged: missionDetailPanel.labelRefresh += 1
                        onWidthChanged: missionDetailPanel.labelRefresh += 1
                        onHeightChanged: missionDetailPanel.labelRefresh += 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton

                        property real lastX: 0
                        property real lastY: 0

                        onPressed: function(mouse) {
                            lastX = mouse.x;
                            lastY = mouse.y;
                        }

                        onPositionChanged: function(mouse) {
                            if (!(mouse.buttons & Qt.LeftButton))
                                return;
                            var dx = mouse.x - lastX;
                            var dy = mouse.y - lastY;
                            lastX = mouse.x;
                            lastY = mouse.y;
                            missionDetailPanel.mapOrbitYaw += dx * 0.4;
                            missionDetailPanel.mapOrbitPitch = Math.max(5.0, Math.min(85.0, missionDetailPanel.mapOrbitPitch + dy * 0.4));
                        }

                        onMouseXChanged: function() {
                            if (!hoverEnabled)
                                return;
                            var id = campaignMap.provinceAtScreen(mouseX, mouseY);
                            campaignMap.hoverProvinceId = id;
                        }

                        onMouseYChanged: function() {
                            if (!hoverEnabled)
                                return;
                            var id = campaignMap.provinceAtScreen(mouseX, mouseY);
                            campaignMap.hoverProvinceId = id;
                        }

                        onExited: campaignMap.hoverProvinceId = ""

                        onWheel: function(wheel) {
                            var step = wheel.angleDelta.y > 0 ? 0.9 : 1.1;
                            var nextDistance = missionDetailPanel.mapOrbitDistance * step;
                            missionDetailPanel.mapOrbitDistance = Math.min(5.0, Math.max(1.2, nextDistance));
                            wheel.accepted = true;
                        }
                    }

                    Repeater {
                        model: missionDetailPanel.provinceLabels

                        delegate: Text {
                            text: modelData.name
                            color: (campaignMap.hoverProvinceId === modelData.id) ? Theme.accent : Theme.textMain
                            font.pointSize: Theme.fontSizeSmall
                            font.bold: true
                            style: Text.Outline
                            styleColor: "#101010"
                            visible: modelData.label_uv && modelData.label_uv.length === 2
                            opacity: 0.85
                            z: 2

                            property int _refresh: missionDetailPanel.labelRefresh
                            property var _pos: campaignMap.screenPosForUv(
                                modelData.label_uv[0], modelData.label_uv[1]
                            )
                            x: _pos.x - width / 2
                            y: _pos.y - height / 2
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
