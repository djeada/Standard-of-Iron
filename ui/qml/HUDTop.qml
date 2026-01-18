import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import StandardOfIron 1.0

Item {
    id: topRoot

    property bool gameIsPaused: false
    property real currentSpeed: 1
    readonly property int barmin_height: 72
    readonly property bool compact: width < 800
    readonly property bool ultraCompact: width < 560

    signal pauseToggled()
    signal speedChanged(real speed)

    Rectangle {
        id: topPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: barmin_height
        color: "#1a1a1a"
        opacity: 0.98
        clip: true

        Rectangle {
            anchors.fill: parent
            opacity: 0.9

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#22303a"
                }

                GradientStop {
                    position: 1
                    color: "#0f1a22"
                }

            }

        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "transparent"
                }

                GradientStop {
                    position: 0.5
                    color: "#3498db"
                }

                GradientStop {
                    position: 1
                    color: "transparent"
                }

            }

        }

        RowLayout {
            id: barRow

            anchors.fill: parent
            anchors.margins: 8
            spacing: 12

            RowLayout {
                id: leftGroup

                spacing: 10
                Layout.alignment: Qt.AlignVCenter

                Button {
                    id: pauseBtn

                    Layout.preferredWidth: topRoot.compact ? 48 : 56
                    Layout.preferredHeight: Math.min(40, topPanel.height - 12)
                    text: topRoot.gameIsPaused ? "\u25B6" : "\u23F8"
                    font.pixelSize: 26
                    font.bold: true
                    focusPolicy: Qt.NoFocus
                    onClicked: topRoot.pauseToggled()

                    background: Rectangle {
                        color: parent.pressed ? "#e74c3c" : parent.hovered ? "#c0392b" : "#34495e"
                        radius: 6
                        border.color: "#2c3e50"
                        border.width: 1
                    }

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#ecf0f1"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                }

                Rectangle {
                    width: 2
                    Layout.fillHeight: true
                    radius: 1
                    visible: !topRoot.compact

                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 0.5
                            color: "#34495e"
                        }

                        GradientStop {
                            position: 1
                            color: "transparent"
                        }

                    }

                }

                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignVCenter

                    Label {
                        text: qsTr("Speed:")
                        visible: !topRoot.compact
                        color: "#ecf0f1"
                        font.pixelSize: 14
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    Row {
                        id: speedRow

                        property var options: [0.5, 1, 2]

                        spacing: 8
                        visible: !topRoot.compact

                        ButtonGroup {
                            id: speedGroup
                        }

                        Repeater {
                            model: speedRow.options

                            delegate: Button {
                                Layout.minimumWidth: 48
                                width: 56
                                height: Math.min(34, topPanel.height - 16)
                                checkable: true
                                enabled: !topRoot.gameIsPaused
                                checked: (topRoot.currentSpeed === modelData) && !topRoot.gameIsPaused
                                focusPolicy: Qt.NoFocus
                                text: modelData + "x"
                                ButtonGroup.group: speedGroup
                                onClicked: topRoot.speedChanged(modelData)

                                background: Rectangle {
                                    color: parent.checked ? "#27ae60" : parent.hovered ? "#34495e" : "#2c3e50"
                                    radius: 6
                                    border.color: parent.checked ? "#229954" : "#1a252f"
                                    border.width: 1
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: parent.enabled ? "#ecf0f1" : "#7f8c8d"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                            }

                        }

                    }

                    StyledComboBox {
                        id: speedCombo

                        visible: topRoot.compact
                        Layout.preferredWidth: 120
                        model: ["0.5x", "1x", "2x"]
                        currentIndex: topRoot.currentSpeed === 0.5 ? 0 : topRoot.currentSpeed === 1 ? 1 : 2
                        enabled: !topRoot.gameIsPaused
                        textPixelSize: 13
                        onActivated: function(i) {
                            var v = i === 0 ? 0.5 : (i === 1 ? 1 : 2);
                            topRoot.speedChanged(v);
                        }
                    }

                }

                Rectangle {
                    width: 2
                    Layout.fillHeight: true
                    radius: 1
                    visible: !topRoot.compact

                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 0.5
                            color: "#34495e"
                        }

                        GradientStop {
                            position: 1
                            color: "transparent"
                        }

                    }

                }

                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignVCenter

                    Label {
                        text: qsTr("Camera:")
                        visible: !topRoot.compact
                        color: "#ecf0f1"
                        font.pixelSize: 14
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }

                    Button {
                        id: followBtn

                        Layout.preferredWidth: topRoot.compact ? 44 : 80
                        Layout.preferredHeight: Math.min(34, topPanel.height - 16)
                        checkable: true
                        text: topRoot.compact ? "\u2609" : qsTr("Follow")
                        font.pixelSize: 13
                        focusPolicy: Qt.NoFocus
                        onToggled: {
                            if (typeof game !== 'undefined' && game.camera_follow_selection)
                                game.camera_follow_selection(checked);

                        }

                        background: Rectangle {
                            color: parent.checked ? "#3498db" : parent.hovered ? "#34495e" : "#2c3e50"
                            radius: 6
                            border.color: parent.checked ? "#2980b9" : "#1a252f"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "#ecf0f1"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                    Button {
                        id: resetBtn

                        Layout.preferredWidth: topRoot.compact ? 44 : 80
                        Layout.preferredHeight: Math.min(34, topPanel.height - 16)
                        text: topRoot.compact ? "\u21BA" : qsTr("Reset")
                        font.pixelSize: 13
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            if (typeof game !== 'undefined' && game.reset_camera)
                                game.reset_camera();

                        }

                        background: Rectangle {
                            color: parent.hovered ? "#34495e" : "#2c3e50"
                            radius: 6
                            border.color: "#1a252f"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "#ecf0f1"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                }

            }

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                id: spectatorIndicator

                visible: typeof game !== 'undefined' && game.is_spectator_mode
                Layout.preferredWidth: spectatorLabel.width + 24
                Layout.preferredHeight: Math.min(36, topPanel.height - 16)
                color: "#e74c3c"
                radius: 6
                border.color: "#c0392b"
                border.width: 1
                Layout.alignment: Qt.AlignVCenter

                Label {
                    id: spectatorLabel

                    anchors.centerIn: parent
                    text: "👁 " + qsTr("SPECTATOR MODE")
                    color: "#ecf0f1"
                    font.pixelSize: 13
                    font.bold: true
                }

                ToolTip {
                    visible: spectatorMA.containsMouse
                    text: qsTr("Watching CPU-only match. All players are AI-controlled. You cannot issue commands.")
                    delay: 300
                }

                MouseArea {
                    id: spectatorMA

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

            }

            RowLayout {
                id: rightGroup

                spacing: 12
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: 260

                Row {
                    id: statsRow

                    spacing: 10
                    Layout.alignment: Qt.AlignVCenter

                    Row {
                        id: playerRow

                        spacing: 6

                        Image {
                            width: 33
                            height: 33
                            source: StyleGuide.iconPath("troop_count.png")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }

                        Label {
                            id: playerLbl

                            text: (typeof game !== 'undefined' ? game.player_troop_count : 0) + " / " + (typeof game !== 'undefined' ? game.max_troops_per_player : 0)
                            color: {
                                if (typeof game === 'undefined')
                                    return "#95a5a6";

                                var count = game.player_troop_count;
                                var max = game.max_troops_per_player;
                                if (count >= max)
                                    return "#e74c3c";

                                if (count >= max * 0.8)
                                    return "#f39c12";

                                return "#2ecc71";
                            }
                            font.pixelSize: 14
                            font.bold: true
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                    Rectangle {
                        width: 2
                        height: 24
                        color: "#34495e"
                        opacity: 0.5
                        visible: !topRoot.compact
                    }

                    Item {
                        id: ownersContainer

                        property var owners: (typeof game !== 'undefined') ? game.owner_info : []

                        function playerCount() {
                            var ownersList = ownersContainer.owners || [];
                            var count = 0;
                            for (var i = 0; i < ownersList.length; i++) {
                                if (ownersList[i].type === "Player")
                                    count++;

                            }
                            return count;
                        }

                        function aiCount() {
                            var ownersList = ownersContainer.owners || [];
                            var count = 0;
                            for (var i = 0; i < ownersList.length; i++) {
                                if (ownersList[i].type === "AI")
                                    count++;

                            }
                            return count;
                        }

                        function ownersTooltip() {
                            if (typeof game === 'undefined')
                                return "";

                            var ownersList = ownersContainer.owners || [];
                            var tip = "Owner IDs:\n";
                            for (var i = 0; i < ownersList.length; i++) {
                                tip += ownersList[i].id + ": " + ownersList[i].name + " (" + ownersList[i].type + ")";
                                if (ownersList[i].isLocal)
                                    tip += " [You]";

                                tip += "\n";
                            }
                            return tip;
                        }

                        visible: !topRoot.compact
                        width: ownersRow.implicitWidth
                        height: ownersRow.implicitHeight
                        implicitWidth: ownersRow.implicitWidth
                        implicitHeight: ownersRow.implicitHeight

                        Row {
                            id: ownersRow

                            spacing: 6

                            Image {
                                width: 30
                                height: 30
                                source: StyleGuide.iconPath("human_player.png")
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Label {
                                id: humanCountLbl

                                text: ownersContainer.playerCount()
                                color: "#ecf0f1"
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }

                            Image {
                                width: 30
                                height: 30
                                source: StyleGuide.iconPath("ai_player.png")
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Label {
                                id: aiCountLbl

                                text: ownersContainer.aiCount()
                                color: "#ecf0f1"
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }

                        }

                        ToolTip {
                            visible: ownersMA.containsMouse
                            delay: 500
                            text: ownersContainer.ownersTooltip()
                        }

                        MouseArea {
                            id: ownersMA

                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }

                    }

                    Row {
                        id: enemyRow

                        spacing: 6

                        Image {
                            width: 30
                            height: 30
                            source: StyleGuide.iconPath("defeated.png")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }

                        Label {
                            id: enemyLbl

                            text: (typeof game !== 'undefined' ? game.enemy_troops_defeated : 0)
                            color: "#ecf0f1"
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                }

                Item {
                    id: miniWrap

                    visible: !topRoot.ultraCompact
                    Layout.preferredWidth: Math.round(topPanel.height * 2.2)
                    Layout.minimumWidth: Math.round(topPanel.height * 1.6)
                    Layout.preferredHeight: topPanel.height - 8
                }

            }

        }

    }

    Rectangle {
        id: minimapContainer

        visible: !topRoot.ultraCompact
        width: 240
        height: 240
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 8
        anchors.topMargin: 8
        z: 100
        color: "#0f1a22"
        radius: 8
        border.width: 2
        border.color: "#3498db"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: 6
            color: "#0a0f14"

            Image {
                id: minimapImage

                property int imageVersion: 0

                anchors.fill: parent
                anchors.margins: 2
                source: imageVersion > 0 ? "image://minimap/v" + imageVersion : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
                cache: false
                asynchronous: false

                Connections {
                    function onMinimap_image_changed() {
                        Qt.callLater(function() {
                            minimapImage.imageVersion++;
                        });
                    }

                    target: game
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("MINIMAP")
                    color: "#3f5362"
                    font.pixelSize: 12
                    font.bold: true
                    visible: parent.status !== Image.Ready
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        if (typeof game === 'undefined')
                            return ;

                        var paintedW = parent.paintedWidth;
                        var paintedH = parent.paintedHeight;
                        if (paintedW === 0 || paintedH === 0)
                            return ;

                        var offsetX = (parent.width - paintedW) / 2;
                        var offsetY = (parent.height - paintedH) / 2;
                        var imgX = mouse.x - offsetX;
                        var imgY = mouse.y - offsetY;
                        if (imgX < 0 || imgX >= paintedW || imgY < 0 || imgY >= paintedH)
                            return ;

                        if (mouse.button === Qt.LeftButton)
                            game.on_minimap_left_click(imgX, imgY, paintedW, paintedH);
                        else if (mouse.button === Qt.RightButton)
                            game.on_minimap_right_click(imgX, imgY, paintedW, paintedH);
                    }
                }

            }

        }

    }

}
