// MapSelect.qml — anchored-only, clean two-pane selector with solid UX
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    anchors.fill: parent
    focus: true

    signal mapChosen(string mapPath)
    signal cancelled()

    // === Theme (kept subtle to fit your app) ===
    readonly property color dim:        Qt.rgba(0,0,0,0.55)
    readonly property color panelBg:    "#071216"
    readonly property color panelBr:    "#0f2b34"
    readonly property color cardA:      "#061316"
    readonly property color cardB:      "#051214"
    readonly property color cardSel:    "#0e2930"
    readonly property color accent:     "#35a6cc"
    readonly property color textMain:   "#eaf6ff"
    readonly property color textSub:    "#8fb2bf"
    readonly property color hint:       "#446b74"

    function mget(i) {
        var m = list.model
        if (!m || i < 0 || i >= list.count) return null
        if (m.get) return m.get(i)          // C++ QAbstractListModel
        if (m[i] !== undefined) return m[i] // JS array
        return null
    }
    function field(obj, key) { return (obj && obj[key] !== undefined) ? String(obj[key]) : "" }
    function current() { return mget(list.currentIndex) }

    // ==== Backdrop ====
    Rectangle { anchors.fill: parent; color: dim }

    // ==== Panel ====
    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.92, 1100)
        height: Math.min(parent.height * 0.88, 680)
        radius: 12
        color: panelBg
        border.color: panelBr
        border.width: 1
        clip: true

        // --- Left pane (list) ---
        Item {
            id: left
            anchors { top: parent.top; bottom: footer.top; left: parent.left; topMargin: 16; leftMargin: 16; bottomMargin: 12 }
            width: Math.max(320, Math.min(panel.width * 0.42, 420))

            // header
            Text {
                id: leftTitle
                text: "Maps"
                color: textMain
                font.pixelSize: 18; font.bold: true
                anchors { top: parent.top; left: parent.left; right: parent.right; bottomMargin: 8 }
            }
            Text {
                id: leftSub
                text: "(" + (list.count || 0) + ")"
                color: textSub
                font.pixelSize: 12
                anchors { left: leftTitle.right; leftMargin: 8; verticalCenter: leftTitle.verticalCenter }
            }

            // list frame
            Rectangle {
                id: listFrame
                anchors { top: leftTitle.bottom; topMargin: 10; left: parent.left; right: parent.right; bottom: parent.bottom }
                color: "transparent"; radius: 8
                border.color: panelBr; border.width: 1
            }

            // list itself
            ListView {
                id: list
                anchors.fill: listFrame
                anchors.margins: 6
                model: (typeof game !== "undefined" && game.availableMaps) ? game.availableMaps : []
                clip: true
                spacing: 6
                currentIndex: (count > 0 ? 0 : -1)
                keyNavigationWraps: false
                boundsBehavior: Flickable.StopAtBounds

                // visible scrollbar (nice for long lists)
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                // selection highlight
                highlight: Rectangle {
                    color: "transparent"
                    radius: 8
                    border.color: accent
                    border.width: 1
                }
                highlightMoveDuration: 120
                highlightFollowsCurrentItem: true

                delegate: Item {
                    id: row
                    height: 70
                    width: list.width
                    property bool hovered: false

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        onEntered: { row.hovered = true; list.currentIndex = index }
                        onExited:  { row.hovered = false }
                        onDoubleClicked: playPrimary.clicked()
                        onClicked: list.currentIndex = index
                    }

                    Rectangle {
                        id: card
                        anchors.fill: parent
                        radius: 8
                        color: (index === list.currentIndex || row.hovered) ? cardSel : (index % 2 ? cardA : cardB)
                        border.color: (index === list.currentIndex || row.hovered) ? accent : panelBr
                        border.width: 1
                        clip: true

                        // thumbnail
                        Rectangle {
                            id: thumbWrap
                            width: 70; height: 50; radius: 6
                            color: "#031314"
                            border.color: "#0b2a31"; border.width: 1
                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: (typeof thumbnail !== "undefined") ? thumbnail : ""
                                asynchronous: true
                                fillMode: Image.PreserveAspectCrop
                                visible: status === Image.Ready
                            }
                        }

                        // text block
                        Item {
                            id: textBlock
                            anchors {
                                left: thumbWrap.right; leftMargin: 10
                                right: parent.right; rightMargin: 10
                                verticalCenter: parent.verticalCenter
                            }
                            height: 54

                            Text {
                                id: mapName
                                text: (typeof name !== "undefined") ? String(name)
                                      : (typeof modelData === "string" ? modelData
                                      : (modelData && modelData.name ? String(modelData.name) : ""))
                                color: textMain
                                font.pixelSize: 16; font.bold: index === list.currentIndex
                                elide: Text.ElideRight
                                anchors { top: parent.top; left: parent.left; right: parent.right }
                            }
                            Text {
                                text: (typeof description !== "undefined") ? String(description)
                                      : (modelData && modelData.description ? String(modelData.description) : "")
                                color: textSub
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            }
                        }
                    }
                }

                // empty state
                Item {
                    anchors.fill: parent
                    visible: list.count === 0
                    Text {
                        text: "No maps available"
                        color: textSub
                        font.pixelSize: 14
                        anchors.centerIn: parent
                    }
                }
            }
        }

        // --- Right pane (preview) ---
        Item {
            id: right
            anchors {
                top: parent.top; bottom: footer.top; right: parent.right
                left: left.right; leftMargin: 16; rightMargin: 16; topMargin: 16; bottomMargin: 12
            }

            // name/title
            Text {
                id: title
                text: {
                    var it = current()
                    var t = field(it,"name")
                    return t || field(it,"path") || "Select a map"
                }
                color: textMain
                font.pixelSize: 22; font.bold: true
                elide: Text.ElideRight
                anchors { top: parent.top; left: parent.left; right: parent.right }
            }

            // description
            Text {
                id: descr
                text: field(current(), "description")
                color: textSub
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                anchors {
                    top: title.bottom; topMargin: 8
                    left: parent.left; right: parent.right
                }
                maximumLineCount: 5
            }

            // preview frame
            Rectangle {
                id: preview
                radius: 8
                color: "#031314"
                border.color: "#0b2a31"; border.width: 1
                clip: true
                anchors {
                    top: descr.bottom; topMargin: 12
                    left: parent.left; right: parent.right
                    bottom: parent.bottom; bottomMargin: 8
                }

                Image {
                    id: previewImage
                    anchors.fill: parent
                    source: field(current(), "thumbnail")
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                }
                Text {
                    anchors.centerIn: parent
                    visible: !previewImage.visible
                    text: "(map preview)"
                    color: hint
                    font.pixelSize: 14
                }
            }
        }

        // --- Footer with actions ---
        Item {
            id: footer
            height: 48
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 16; rightMargin: 16; bottomMargin: 16 }

            Button {
                id: backBtn
                text: "Back"
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                onClicked: root.cancelled()
                ToolTip.visible: hovered
                ToolTip.text: "Esc"
            }

            Button {
                id: playPrimary
                text: "Play"
                enabled: list.currentIndex >= 0 && list.count > 0
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                onClicked: {
                    var it = current()
                    var p = field(it, "path") || field(it, "file")
                    root.mapChosen(p)
                }
                ToolTip.visible: hovered
                ToolTip.text: "Enter"
            }
        }

        // keyboard flow
        Keys.onPressed: {
            if (event.key === Qt.Key_Down) {
                if (list.count > 0)
                    list.currentIndex = Math.min(list.currentIndex + 1, list.count - 1)
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                if (list.count > 0)
                    list.currentIndex = Math.max(list.currentIndex - 1, 0)
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (list.currentIndex >= 0 && list.count > 0) {
                    var it = current()
                    var p = field(it, "path") || field(it, "file")
                    root.mapChosen(p)
                }
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                root.cancelled()
                event.accepted = true
            }
        }
    }
}
