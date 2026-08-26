import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio

Item {
    id: root

    property var maps_model: []
    property int selected_index: 0
    property var colors: ({})

    // No shipped map authors a thumbnail and no *_thumb.png exists, so every row
    // showed the same empty square while the real preview rendered only in the
    // detail panel. Draw the same preview into the rows. Held by path so a
    // recycled delegate does not re-render the map it scrolled past, and named
    // so a test can hand in its own generator and store.
    property var previewSource: (typeof game !== "undefined" && game.setup) ? game.setup : null
    property var previewStore: (typeof map_preview_provider !== "undefined") ? map_preview_provider : null
    property var thumbnailCache: ({})

    signal map_selected(int index)
    signal map_double_clicked

    function field(obj, key) {
        return (obj && obj[key] !== undefined) ? String(obj[key]) : "";
    }

    function thumbnail_source(authored, map_path) {
        if (authored && authored !== "")
            return authored;
        if (!map_path || map_path === "")
            return "";
        if (thumbnailCache[map_path] !== undefined)
            return thumbnailCache[map_path];
        if (!previewSource || !previewSource.map_preview || !previewStore)
            return "";
        var id = "maplist:" + map_path;
        var url = "";
        try {
            previewStore.set_preview_image(id, previewSource.map_preview(map_path, []));
            url = "image://mappreview/" + id;
        } catch (e) {
            console.warn("MapListPanel: no preview for", map_path, e);
        }
        thumbnailCache[map_path] = url;
        return url;
    }

    anchors.fill: parent

    Text {
        id: title

        text: qsTr("Maps")
        color: colors.textMain
        font.pixelSize: Design.Typography.bodyLarge
        font.bold: true

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    Text {
        id: countLabel

        text: "(" + Design.Numerals.roman(list.count || 0) + ")"
        color: colors.textSubLite
        font.pixelSize: Design.Typography.caption

        anchors {
            left: title.right
            leftMargin: 8
            verticalCenter: title.verticalCenter
        }
    }

    Rectangle {
        id: listFrame

        color: "transparent"
        radius: 10
        border.color: colors.panelBr
        border.width: 1
        clip: true

        anchors {
            top: title.bottom
            topMargin: 12
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        ListView {
            id: list

            anchors.fill: parent
            anchors.margins: 8
            model: root.maps_model
            clip: true
            spacing: 8
            currentIndex: root.selected_index
            keyNavigationWraps: false
            boundsBehavior: Flickable.StopAtBounds
            onCurrentIndexChanged: {
                root.selected_index = currentIndex;
                if (currentIndex >= 0)
                    root.map_selected(currentIndex);
            }
            highlightMoveDuration: 120
            highlightFollowsCurrentItem: true

            Item {
                anchors.fill: parent
                visible: list.count === 0

                Text {
                    text: qsTr("No maps available")
                    color: colors.textSub
                    font.pixelSize: Design.Typography.label
                    anchors.centerIn: parent
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            highlight: Rectangle {
                color: "transparent"
                radius: 8
                border.color: colors.selectedBr
                border.width: 1
            }

            delegate: Item {
                width: list.width
                height: 68

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: {
                        if (containsMouse && typeof game !== "undefined")
                            UiAudio.play_hover(game.audio_system);
                    }
                    onClicked: {
                        if (typeof game !== "undefined")
                            UiAudio.play_click(game.audio_system);
                        list.currentIndex = index;
                    }
                    onDoubleClicked: root.map_double_clicked()
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    clip: true
                    color: rowMouse.containsPress ? colors.hoverBg : (index === list.currentIndex ? colors.selectedBg : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : "transparent"))
                    border.width: 1
                    border.color: (index === list.currentIndex) ? colors.selectedBr : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : colors.thumbBr)

                    Rectangle {
                        id: thumbWrap

                        width: 60
                        height: 42
                        radius: 6
                        color: "#031314"
                        border.color: colors.thumbBr
                        border.width: 1
                        clip: true

                        anchors {
                            left: parent.left
                            leftMargin: 10
                            verticalCenter: parent.verticalCenter
                        }

                        Image {
                            anchors.fill: parent
                            source: root.thumbnail_source((typeof thumbnail !== "undefined") ? thumbnail : "", (typeof path !== "undefined") ? String(path) : "")
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                            visible: status === Image.Ready
                        }
                    }

                    Column {
                        spacing: 4

                        anchors {
                            left: thumbWrap.right
                            leftMargin: 10
                            right: parent.right
                            rightMargin: 10
                            verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: (typeof name !== "undefined") ? String(name) : ""
                            color: (index === list.currentIndex) ? "white" : "#dff0ff"
                            font.pixelSize: (index === list.currentIndex) ? Design.Typography.body : Design.Typography.label
                            font.bold: (index === list.currentIndex)
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            text: (typeof description !== "undefined") ? String(description) : ""
                            color: (index === list.currentIndex) ? "#d0e8ff" : colors.textSub
                            font.pixelSize: Design.Typography.caption
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    Behavior on color  {
                        ColorAnimation {
                            duration: 160
                        }
                    }

                    Behavior on border.color  {
                        ColorAnimation {
                            duration: 160
                        }
                    }
                }
            }
        }
    }
}
