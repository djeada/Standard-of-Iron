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

    property var bases: []

    property var base_owners: ({})
    property string focused_base_key: ""
    property bool bases_interactive: false

    signal base_activated(string key)

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

    function owner_of(key) {
        if (!base_owners)
            return null;
        var entry = base_owners[String(key)];
        return entry === undefined ? null : entry;
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

    Item {
        id: painted_area

        x: preview_image.x + (preview_image.width - preview_image.paintedWidth) / 2
        y: preview_image.y + (preview_image.height - preview_image.paintedHeight) / 2
        width: preview_image.paintedWidth
        height: preview_image.paintedHeight
        visible: preview_image.visible && root.bases_interactive

        Repeater {
            model: root.bases

            delegate: Item {
                id: marker

                readonly property var owner_entry: root.owner_of(modelData.key)
                readonly property bool taken: owner_entry !== null
                readonly property bool focused: root.focused_base_key === String(modelData.key)

                width: Design.Metrics.space24
                height: Design.Metrics.space24
                x: Number(modelData.previewX) * painted_area.width - width / 2
                y: Number(modelData.previewY) * painted_area.height - height / 2

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    radius: width / 2
                    color: "transparent"
                    border.width: marker.focused ? 3 : 2
                    border.color: marker.focused ? Theme.accentBright : (marker_mouse.containsMouse ? Theme.textMain : "transparent")
                    opacity: marker.focused || marker_mouse.containsMouse ? 1 : 0

                    Behavior on opacity  {
                        NumberAnimation {
                            duration: Theme.animFast
                        }
                    }
                }

                MouseArea {
                    id: marker_mouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        Design.UiSound.activate();
                        root.base_activated(String(modelData.key));
                    }
                    onContainsMouseChanged: {
                        if (containsMouse)
                            Design.UiSound.hover();
                    }
                }

                ToolTip.visible: marker_mouse.containsMouse
                ToolTip.delay: Design.Metrics.tooltipDelay
                ToolTip.text: marker.taken ? qsTr("%1 — held by %2").arg(String(modelData.name)).arg(String(marker.owner_entry.owner)) : qsTr("%1 — free, click to claim it").arg(String(modelData.name))
            }
        }
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

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.spacingSmall
            anchors.rightMargin: Theme.spacingSmall
            text: root.bases_interactive ? qsTr("Click a base to reseat") : qsTr("Bases in player colours")
            color: Theme.textSubLite
            font.pixelSize: Design.Typography.caption
            font.italic: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
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
