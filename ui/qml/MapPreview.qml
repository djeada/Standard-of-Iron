import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

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
        if (typeof game === "undefined" || !game.setup.map_preview)
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
            var preview = game.setup.map_preview(map_path, player_configs);
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

    radius: Theme.radiusSmall
    color: "#241c14"
    border.color: "#8f6d43"
    border.width: 1
    clip: true
    onMap_pathChanged: refresh_preview()
    onPlayer_configsChanged: refresh_preview()

    Image {
        id: preview_image

        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: false
        visible: status === Image.Ready
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: legend_label.implicitHeight + Theme.spacingSmall
        color: "#cc140f0b"
        visible: preview_image.visible

        Text {
            id: legend_label

            anchors.centerIn: parent
            text: qsTr("Bases shown in player colours")
            color: Theme.textSubLite
            font.pixelSize: Design.Typography.caption
            font.italic: true
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - Theme.spacingMedium * 2
        visible: !loading && !preview_image.visible
        text: map_path === "" ? qsTr("Select a map\nto see preview") : qsTr("No preview available")
        color: Theme.textHint
        font.pixelSize: Design.Typography.caption
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    Text {
        anchors.centerIn: parent
        visible: loading
        text: Design.Icons.autoGather
        font.pixelSize: Design.Typography.glyph
        color: Theme.accent

        RotationAnimator on rotation  {
            from: 0
            to: 360
            duration: 1500
            loops: Animation.Infinite
            running: root.loading
        }
    }
}
