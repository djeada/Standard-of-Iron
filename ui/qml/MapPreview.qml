import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var map_path: ""
    property var player_configs: []
    property bool loading: false
    property string preview_id: ""

    function refresh_preview() {
        if (!map_path || map_path === "" || !player_configs || player_configs.length === 0) {
            preview_image.source = "";
            preview_id = "";
            return;
        }
        if (typeof game === "undefined" || !game.generate_map_preview)
            return;
        loading = true;
        try {
            var config_str = JSON.stringify(player_configs);
            var hash = 0;
            for (var i = 0; i < config_str.length; i++) {
                var code_point = config_str.charCodeAt(i);
                hash = ((hash << 5) - hash) + code_point;
                hash = hash & hash;
            }
            var new_id = map_path + "_" + hash + "_" + Date.now();
            var preview = game.generate_map_preview(map_path, player_configs);
            if (typeof map_preview_provider !== "undefined") {
                map_preview_provider.set_preview_image(new_id, preview);
                preview_id = new_id;
                preview_image.source = "image://mappreview/" + new_id;
            }
            loading = false;
        } catch (e) {
            console.error("MapPreview: Failed to generate preview:", e);
            loading = false;
        }
    }

    radius: Theme.radiusLarge
    color: Theme.cardBase
    border.color: Theme.panelBr
    border.width: 1
    clip: true
    onMap_pathChanged: refresh_preview()
    onPlayer_configsChanged: refresh_preview()

    Text {
        id: title_text

        text: qsTr("Map Preview")
        color: Theme.textMain
        font.pixelSize: 16
        font.bold: true

        anchors {
            top: parent.top
            left: parent.left
            margins: Theme.spacingMedium
        }
    }

    Rectangle {
        id: preview_container

        color: Theme.cardBaseB
        radius: Theme.radiusMedium
        border.color: Theme.thumbBr
        border.width: 1

        anchors {
            top: title_text.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.spacingMedium
        }

        RowLayout {
            id: preview_row

            anchors.fill: parent
            anchors.margins: Theme.spacingSmall
            spacing: Theme.spacingMedium
            visible: !loading

            Item {
                id: image_wrapper

                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                Layout.preferredWidth: Math.min(preview_container.width * 0.6, preview_container.height - Theme.spacingLarge)
                Layout.preferredHeight: Layout.preferredWidth

                Image {
                    id: preview_image

                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    cache: false
                    visible: status === Image.Ready
                }
            }

            Text {
                id: legend_text

                text: qsTr("Player bases shown as colored circles")
                color: Theme.textSubLite
                font.pixelSize: 12
                font.italic: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                visible: map_path !== ""
            }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("Select a map\nto see preview")
            color: Theme.textHint
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            visible: !loading && preview_image.status !== Image.Ready && map_path === ""
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("No preview available")
            color: Theme.textHint
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            visible: !loading && preview_image.status !== Image.Ready && map_path !== ""
        }

        Item {
            anchors.centerIn: parent
            visible: loading
            width: 60
            height: 60

            Text {
                anchors.centerIn: parent
                text: "⟳"
                font.pixelSize: 36
                color: Theme.accent

                RotationAnimator on rotation  {
                    from: 0
                    to: 360
                    duration: 1500
                    loops: Animation.Infinite
                    running: loading
                }
            }
        }
    }
}
