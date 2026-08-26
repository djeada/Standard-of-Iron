import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio

Item {
    id: root

    property var maps_model: []
    property int selected_index: 0
    property var colors: ({})

    signal map_selected(int index)
    signal map_double_clicked

    function field(obj, key) {
        return (obj && obj[key] !== undefined) ? String(obj[key]) : "";
    }


    anchors.fill: parent

    MapThumbnails {
        id: thumbnails
    }

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
                            source: thumbnails.source_for((typeof thumbnail !== "undefined") ? thumbnail : "", (typeof path !== "undefined") ? String(path) : "")
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
