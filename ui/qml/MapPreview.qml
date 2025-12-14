import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Rectangle {
    id: root

    property var mapPath: ""
    property var playerConfigs: []
    property bool loading: false
    property string previewId: ""

    function refreshPreview() {
        if (!mapPath || mapPath === "" || !playerConfigs || playerConfigs.length === 0) {
            previewImage.source = "";
            previewId = "";
            return ;
        }
        if (typeof game === "undefined" || !game.generate_map_preview)
            return ;

        loading = true;
        try {
            var configStr = JSON.stringify(playerConfigs);
            var hash = 0;
            for (var i = 0; i < configStr.length; i++) {
                var codePoint = configStr.charCodeAt(i);
                hash = ((hash << 5) - hash) + codePoint;
                hash = hash & hash;
            }
            var newId = mapPath + "_" + hash + "_" + Date.now();
            var preview = game.generate_map_preview(mapPath, playerConfigs);
            if (typeof mapPreviewProvider !== "undefined") {
                mapPreviewProvider.set_preview_image(newId, preview);
                previewId = newId;
                previewImage.source = "image://mappreview/" + newId;
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

        color: Theme.cardBaseB
        radius: Theme.radiusMedium
        border.color: Theme.thumbBr
        border.width: 1

        anchors {
            top: titleText.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.spacingMedium
        }

        RowLayout {
            id: previewRow

            anchors.fill: parent
            anchors.margins: Theme.spacingSmall
            spacing: Theme.spacingMedium
            visible: !loading

            Item {
                id: imageWrapper

                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                Layout.preferredWidth: Math.min(previewContainer.width * 0.6, previewContainer.height - Theme.spacingLarge)
                Layout.preferredHeight: Layout.preferredWidth

                Image {
                    id: previewImage

                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    cache: false
                    visible: status === Image.Ready
                }

            }

            Text {
                id: legendText

                text: qsTr("Player bases shown as colored circles")
                color: Theme.textSubLite
                font.pixelSize: 12
                font.italic: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                visible: mapPath !== ""
            }

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

}
