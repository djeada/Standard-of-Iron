

import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    anchors.fill: parent
    focus: true

    signal mapChosen(string mapPath)
    signal cancelled()

    
    readonly property color dim:         Qt.rgba(0, 0, 0, 0.45)
    readonly property color panelBg:     "#071018"
    readonly property color panelBr:     "#0f2430"
    readonly property color cardBaseA:   "#051219"
    readonly property color cardBaseB:   "#06141b"
    readonly property color hoverBg:     "#184c7a"
    readonly property color selectedBg:  "#1f8bf5"
    readonly property color selectedBr:  "#1b74d1"
    readonly property color textMain:    "#eaf6ff"
    readonly property color textSub:     "#79a6b7"
    readonly property color textSubLite: "#86a7b6"
    readonly property color hint:        "#2a5e6e"
    readonly property color thumbBr:     "#12323a"

    function mget(i) {
        var m = list.model
        if (!m || i < 0 || i >= list.count) return null
        if (m.get) return m.get(i)          
        if (m[i] !== undefined) return m[i] 
        return null
    }
    function field(obj, key) { return (obj && obj[key] !== undefined) ? String(obj[key]) : "" }
    function current() { return mget(list.currentIndex) }
    function acceptSelection() {
        
        if (!visible) return
        if (list.currentIndex < 0 || list.count <= 0) return
        var it = current()
        var p = field(it, "path") || field(it, "file")
        console.log("MapSelect: acceptSelection called, path=", p, "visible=", visible, "currentIndex=", list.currentIndex)
        if (p && p.length > 0) {
            console.log("MapSelect: emitting mapChosen for", p)
            root.mapChosen(p)
        } else {
            console.log("MapSelect: no valid path to choose")
        }
    }

    
    Rectangle { anchors.fill: parent; color: dim }

    
    
    Keys.onPressed: function(event) {
        
        
        
        if (!visible) return;

        
        
    if (event.key === Qt.Key_Down) {
            if (list.count > 0)
                list.currentIndex = Math.min(list.currentIndex + 1, list.count - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            if (list.count > 0)
                list.currentIndex = Math.max(list.currentIndex - 1, 0)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            acceptSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            backBtn.clicked()
            event.accepted = true
        }
    }

    
    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.78, 1100)
        height: Math.min(parent.height * 0.78, 700)
        radius: 14
        color: panelBg
        border.color: panelBr
        border.width: 1
        opacity: 0.98
        clip: true

        
        Item {
            id: left
            anchors {
                top: parent.top; left: parent.left; bottom: footer.top
                topMargin: 20; leftMargin: 20; bottomMargin: 12
            }
            width: Math.max(320, Math.min(panel.width * 0.45, 460))

            
            Text {
                id: leftTitle
                text: "Maps"
                color: textMain
                font.pixelSize: 18; font.bold: true
                anchors { top: parent.top; left: parent.left; right: parent.right }
            }
            Text {
                id: leftSub
                text: "(" + (list.count || 0) + ")"
                color: textSubLite
                font.pixelSize: 12
                anchors { left: leftTitle.right; leftMargin: 8; verticalCenter: leftTitle.verticalCenter }
            }

            
            Rectangle {
                id: listFrame
                anchors { top: leftTitle.bottom; topMargin: 12; left: parent.left; right: parent.right; bottom: parent.bottom }
                color: "transparent"; radius: 10
                border.color: panelBr; border.width: 1
                clip: true
            }

            
            ListView {
                id: list
                anchors.fill: listFrame
                anchors.margins: 8
                model: (typeof game !== "undefined" && game.availableMaps) ? game.availableMaps : []
                clip: true
                spacing: 8
                currentIndex: (count > 0 ? 0 : -1)
                keyNavigationWraps: false
                boundsBehavior: Flickable.StopAtBounds

                
                
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.acceptSelection()
                        event.accepted = true
                    }
                }

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                
                highlight: Rectangle {
                    color: "transparent"
                    radius: 8
                    border.color: selectedBr
                    border.width: 1
                }
                highlightMoveDuration: 120
                highlightFollowsCurrentItem: true

                delegate: Item {
                    id: row
                    width: list.width
                    height: 68
                    property bool hovered: false

                    
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        onEntered: { hovered = true; list.currentIndex = index }
                        onExited:  hovered = false
                        onClicked: list.currentIndex = index
                        onDoubleClicked: acceptSelection()
                    }

                    Rectangle {
                        id: card
                        anchors.fill: parent
                        radius: 8
                        clip: true

                        
                        
                        
                        color: rowMouse.containsPress
                               ? hoverBg
                               : (index === list.currentIndex ? selectedBg : "transparent")
                        border.width: 1
                        border.color: (index === list.currentIndex) ? selectedBr : thumbBr
                        Behavior on color { ColorAnimation { duration: 160 } }
                        Behavior on border.color { ColorAnimation { duration: 160 } }

                        
                        Rectangle {
                            id: thumbWrap
                            width: 72; height: 50; radius: 6
                            color: "#031314"
                            border.color: thumbBr; border.width: 1
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

                        
                        Item {
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
                                color: (index === list.currentIndex) ? "white" : "#dff0ff"
                                font.pixelSize: (index === list.currentIndex) ? 17 : 15
                                font.bold: (index === list.currentIndex)
                                elide: Text.ElideRight
                                anchors { top: parent.top; left: parent.left; right: parent.right }
                                Behavior on font.pixelSize { NumberAnimation { duration: 140 } }
                            }
                            Text {
                                text: (typeof description !== "undefined") ? String(description)
                                      : (modelData && modelData.description ? String(modelData.description) : "")
                                color: (index === list.currentIndex) ? "#d0e8ff" : textSub
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            }
                        }

                        
                        Text {
                            text: "›"
                            font.pointSize: 18
                            color: (index === list.currentIndex) ? "white" : "#2a5e6e"
                            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                        }
                    }
                }

                
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

        
        Item {
            id: right
            anchors {
                top: parent.top; bottom: footer.top; right: parent.right
                left: left.right; leftMargin: 18; rightMargin: 20; topMargin: 20; bottomMargin: 12
            }

            
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

            
            Rectangle {
                id: preview
                radius: 10
                color: "#031314"
                border.color: thumbBr; border.width: 1
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

        
        Item {
            id: footer
            height: 52
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 20; rightMargin: 20; bottomMargin: 16 }

            
            Button {
                id: backBtn
                text: "Back"
                anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                onClicked: root.cancelled()
                hoverEnabled: true

                
                MouseArea {
                    id: backHover
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    cursorShape: Qt.PointingHandCursor
                }

                contentItem: Text {
                    text: backBtn.text
                    font.pointSize: backHover.containsMouse ? 13 : 12
                    font.bold: backHover.containsMouse
                    color: "#dff0ff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Behavior on font.pointSize { NumberAnimation { duration: 120 } }
                }
                background: Rectangle {
                    implicitWidth: 110; implicitHeight: 38
                    radius: 8
                    color: backBtn.down ? hoverBg
                          : (backHover.containsMouse ? "#0e2a3c" : "transparent")
                    border.width: 1
                    border.color: backHover.containsMouse ? thumbBr : panelBr
                    Behavior on color { ColorAnimation { duration: 140 } }
                    Behavior on border.color { ColorAnimation { duration: 140 } }
                }

                ToolTip.visible: backHover.containsMouse
                ToolTip.text: "Esc"
            }

            
            Button {
                id: playPrimary
                text: "Play"
                enabled: list.currentIndex >= 0 && list.count > 0
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                hoverEnabled: true

                
                MouseArea {
                    id: playHover
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    cursorShape: Qt.PointingHandCursor
                }

                contentItem: Text {
                    text: playPrimary.text
                    font.pointSize: enabled ? (playHover.containsMouse ? 13 : 12) : 12
                    font.bold: true
                    color: enabled ? "white" : "#6f8793"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Behavior on font.pointSize { NumberAnimation { duration: 120 } }
                }
                background: Rectangle {
                    implicitWidth: 120; implicitHeight: 40
                    radius: 9
                    color: enabled
                           ? (playPrimary.down ? "#1b74d1" : (playHover.containsMouse ? "#2a7fe0" : selectedBg))
                           : "#0a1a24"
                    border.width: 1
                    border.color: enabled ? selectedBr : panelBr
                    Behavior on color { ColorAnimation { duration: 140 } }
                    Behavior on border.color { ColorAnimation { duration: 140 } }
                }

                onClicked: acceptSelection()

                ToolTip.visible: playHover.containsMouse
                ToolTip.text: "Enter"
            }
        }

        
        
        
    }
}
