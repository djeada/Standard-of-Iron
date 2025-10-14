
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    anchors.fill: parent

    property var mapsModel: []
    property int currentIndex: 0
    property var colors: ({})

    signal mapSelected(int index)
    signal mapDoubleClicked()

    function field(obj, key) {
        return (obj && obj[key] !== undefined) ? String(obj[key]) : ""
    }

    
    Text {
        id: title
        text: "Maps"
        color: colors.textMain
        font.pixelSize: 18
        font.bold: true
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    Text {
        id: countLabel
        text: "(" + (list.count || 0) + ")"
        color: colors.textSubLite
        font.pixelSize: 12
        anchors {
            left: title.right
            leftMargin: 8
            verticalCenter: title.verticalCenter
        }
    }

    
    Rectangle {
        id: listFrame
        anchors {
            top: title.bottom
            topMargin: 12
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        color: "transparent"
        radius: 10
        border.color: colors.panelBr
        border.width: 1
        clip: true

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 8
            model: root.mapsModel
            clip: true
            spacing: 8
            currentIndex: root.currentIndex
            keyNavigationWraps: false
            boundsBehavior: Flickable.StopAtBounds

            onCurrentIndexChanged: {
                root.currentIndex = currentIndex
                if (currentIndex >= 0) {
                    root.mapSelected(currentIndex)
                }
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            highlight: Rectangle {
                color: "transparent"
                radius: 8
                border.color: colors.selectedBr
                border.width: 1
            }
            highlightMoveDuration: 120
            highlightFollowsCurrentItem: true

            delegate: Item {
                width: list.width
                height: 68

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: list.currentIndex = index
                    onDoubleClicked: root.mapDoubleClicked()
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    clip: true
                    color: rowMouse.containsPress ? colors.hoverBg
                            : (index === list.currentIndex ? colors.selectedBg 
                            : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : "transparent"))
                    border.width: 1
                    border.color: (index === list.currentIndex) ? colors.selectedBr 
                            : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : colors.thumbBr)
                    Behavior on color { ColorAnimation { duration: 160 } }
                    Behavior on border.color { ColorAnimation { duration: 160 } }

                    Rectangle {
                        id: thumbWrap
                        width: 60
                        height: 42
                        radius: 6
                        color: "#031314"
                        border.color: colors.thumbBr
                        border.width: 1
                        anchors {
                            left: parent.left
                            leftMargin: 10
                            verticalCenter: parent.verticalCenter
                        }
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: (typeof thumbnail !== "undefined") ? thumbnail : ""
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                            visible: status === Image.Ready
                        }
                    }

                    Column {
                        anchors {
                            left: thumbWrap.right
                            leftMargin: 10
                            right: parent.right
                            rightMargin: 10
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: 4

                        Text {
                            text: (typeof name !== "undefined") ? String(name) : ""
                            color: (index === list.currentIndex) ? "white" : "#dff0ff"
                            font.pixelSize: (index === list.currentIndex) ? 15 : 14
                            font.bold: (index === list.currentIndex)
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            text: (typeof description !== "undefined") ? String(description) : ""
                            color: (index === list.currentIndex) ? "#d0e8ff" : colors.textSub
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }
                }
            }

            
            Item {
                anchors.fill: parent
                visible: list.count === 0
                Text {
                    text: "No maps available"
                    color: colors.textSub
                    font.pixelSize: 14
                    anchors.centerIn: parent
                }
            }
        }
    }
}
