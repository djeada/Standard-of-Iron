import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root

    property var mapPath: ""
    property var playerConfigs: []
    property bool loading: false
    property string previewId: ""

    radius: Theme.radiusLarge
    color: Theme.cardBase
    border.color: Theme.panelBr
    border.width: 1
    clip: true

    function refreshPreview() {
        if (!mapPath || mapPath === "" || !playerConfigs || playerConfigs.length === 0) {
            previewImage.source = "";
            previewId = "";
            return;
        }

        if (typeof game === "undefined" || !game.generate_map_preview) {
            return;
        }

        loading = true;
        try {
            // Generate a unique ID based on map path and player configs
            var configStr = JSON.stringify(playerConfigs);
            var newId = mapPath + "_" + configStr.length + "_" + Date.now();
            
            var preview = game.generate_map_preview(mapPath, playerConfigs);
            
            if (typeof mapPreviewProvider !== "undefined") {
                mapPreviewProvider.set_preview_image(newId, preview);
                previewId = newId;
                // Force image reload by changing the source
                previewImage.source = "image://mappreview/" + newId;
            }
            
            loading = false;
        } catch (e) {
            console.error("MapPreview: Failed to generate preview:", e);
            loading = false;
        }
    }

    onMapPathChanged: refreshPreview()
    onPlayerConfigsChanged: refreshPreview()

    Text {
        id: titleText

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
        id: previewContainer

        anchors {
            top: titleText.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.spacingMedium
        }

        color: Theme.cardBaseB
        radius: Theme.radiusMedium
        border.color: Theme.thumbBr
        border.width: 1

        Image {
            id: previewImage

            anchors.centerIn: parent
            width: Math.min(parent.width - 20, parent.height - 20)
            height: width
            fillMode: Image.PreserveAspectFit
            smooth: true
            cache: false
            visible: !loading && status === Image.Ready
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("Select a map\nto see preview")
            color: Theme.textHint
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            visible: !loading && previewImage.status !== Image.Ready && mapPath === ""
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("No preview available")
            color: Theme.textHint
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            visible: !loading && previewImage.status !== Image.Ready && mapPath !== ""
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

                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 1500
                    loops: Animation.Infinite
                    running: loading
                }
            }
        }
    }

    Text {
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: Theme.spacingSmall
        }
        text: qsTr("Player bases shown as colored circles")
        color: Theme.textSubLite
        font.pixelSize: 10
        font.italic: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
