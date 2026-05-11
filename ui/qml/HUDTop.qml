import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import StandardOfIron 1.0

Item {
    id: topRoot

    property bool game_is_paused: false
    property real current_speed: 1
    readonly property int bar_min_height: 72
    readonly property bool compact: width < 800
    readonly property bool ultra_compact: width < 560
    readonly property var hs: StyleGuide.historical

    signal pause_toggled()
    signal speed_changed(real speed)

    Rectangle {
        id: topPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: bar_min_height
        color: hs.parchmentDark
        opacity: 0.98
        clip: true

        Rectangle {
            anchors.fill: parent
            opacity: 0.9

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: hs.parchmentLight
                }

                GradientStop {
                    position: 1
                    color: hs.parchmentDark
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
                    color: hs.bronze
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
                    text: topRoot.game_is_paused ? "\u25B6" : "\u23F8"
                    font.pixelSize: 26
                    font.bold: true
                    focusPolicy: Qt.NoFocus
                    onClicked: topRoot.pause_toggled()

                    background: Item {
                        Rectangle {
                            x: 1; y: 2
                            width: parent.width; height: parent.height
                            radius: 7
                            color: "#000000"
                            opacity: pauseBtn.pressed ? 0.0 : 0.22
                        }

                        Rectangle {
                            id: pauseBg
                            anchors.fill: parent
                            color: pauseBtn.pressed ? hs.waxDark : pauseBtn.hovered ? hs.waxHover : hs.wax
                            radius: 6
                            border.color: hs.bronzeDeep
                            border.width: 1

                            Rectangle {
                                anchors.fill: parent; anchors.margins: 1
                                radius: Math.max(1, parent.radius - 1)
                                opacity: pauseBtn.pressed ? 0.0 : 0.12
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#FFFFFF" }
                                    GradientStop { position: 0.45; color: "#FFFFFF" }
                                    GradientStop { position: 1.0; color: "#000000" }
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top; anchors.topMargin: 1
                                anchors.left: parent.left; anchors.leftMargin: 6
                                anchors.right: parent.right; anchors.rightMargin: 6
                                height: 1
                                color: StyleGuide.palette.accentBright
                                opacity: pauseBtn.pressed ? 0.0 : 0.50
                            }
                        }
                    }

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: Theme.textMain
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        topPadding: pauseBtn.pressed ? 1 : 0
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
                            color: hs.bronzeDeep
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
                        color: Theme.textMain
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
                                enabled: !topRoot.game_is_paused
                                checked: (topRoot.current_speed === modelData) && !topRoot.game_is_paused
                                focusPolicy: Qt.NoFocus
                                text: modelData + "x"
                                ButtonGroup.group: speedGroup
                                onClicked: topRoot.speed_changed(modelData)

                                background: Item {
                                    Rectangle {
                                        x: 1; y: 2
                                        width: parent.width; height: parent.height
                                        radius: 7
                                        color: "#000000"
                                        opacity: parent.parent.pressed ? 0.0 : 0.20
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        color: parent.parent.checked ? hs.wax : parent.parent.hovered ? hs.parchmentLight : hs.parchmentDark
                                        radius: 6
                                        border.color: parent.parent.checked ? hs.bronze : hs.bronzeDeep
                                        border.width: 1

                                        Rectangle {
                                            anchors.fill: parent; anchors.margins: 1
                                            radius: Math.max(1, parent.radius - 1)
                                            opacity: parent.parent.parent.pressed ? 0.0 : 0.11
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: "#FFFFFF" }
                                                GradientStop { position: 0.45; color: "#FFFFFF" }
                                                GradientStop { position: 1.0; color: "#000000" }
                                            }
                                        }

                                        Rectangle {
                                            anchors.top: parent.top; anchors.topMargin: 1
                                            anchors.left: parent.left; anchors.leftMargin: 5
                                            anchors.right: parent.right; anchors.rightMargin: 5
                                            height: 1
                                            color: StyleGuide.palette.accentBright
                                            opacity: parent.parent.parent.pressed ? 0.0 : 0.45
                                        }
                                    }
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 13
                                    font.bold: true
                                    color: parent.enabled ? Theme.textMain : Theme.textDim
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    topPadding: parent.pressed ? 1 : 0
                                }

                            }

                        }

                    }

                    StyledComboBox {
                        id: speedCombo

                        visible: topRoot.compact
                        Layout.preferredWidth: 120
                        model: ["0.5x", "1x", "2x"]
                        currentIndex: topRoot.current_speed === 0.5 ? 0 : topRoot.current_speed === 1 ? 1 : 2
                        enabled: !topRoot.game_is_paused
                        text_pixel_size: 13
                        onActivated: function(i) {
                            var v = i === 0 ? 0.5 : (i === 1 ? 1 : 2);
                            topRoot.speed_changed(v);
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
                            color: hs.bronzeDeep
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
                        color: Theme.textMain
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

                        background: Item {
                            Rectangle {
                                x: 1; y: 2
                                width: parent.width; height: parent.height
                                radius: 7
                                color: "#000000"
                                opacity: followBtn.pressed ? 0.0 : 0.20
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: followBtn.checked ? hs.wax : followBtn.hovered ? hs.parchmentLight : hs.parchmentDark
                                radius: 6
                                border.color: followBtn.checked ? hs.bronze : hs.bronzeDeep
                                border.width: 1

                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 1
                                    radius: Math.max(1, parent.radius - 1)
                                    opacity: followBtn.pressed ? 0.0 : 0.11
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#FFFFFF" }
                                        GradientStop { position: 0.45; color: "#FFFFFF" }
                                        GradientStop { position: 1.0; color: "#000000" }
                                    }
                                }

                                Rectangle {
                                    anchors.top: parent.top; anchors.topMargin: 1
                                    anchors.left: parent.left; anchors.leftMargin: 5
                                    anchors.right: parent.right; anchors.rightMargin: 5
                                    height: 1
                                    color: StyleGuide.palette.accentBright
                                    opacity: followBtn.pressed ? 0.0 : 0.45
                                }
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: Theme.textMain
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            topPadding: followBtn.pressed ? 1 : 0
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

                        background: Item {
                            Rectangle {
                                x: 1; y: 2
                                width: parent.width; height: parent.height
                                radius: 7
                                color: "#000000"
                                opacity: resetBtn.pressed ? 0.0 : 0.20
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: resetBtn.hovered ? hs.parchmentLight : hs.parchmentDark
                                radius: 6
                                border.color: hs.bronzeDeep
                                border.width: 1

                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 1
                                    radius: Math.max(1, parent.radius - 1)
                                    opacity: resetBtn.pressed ? 0.0 : 0.11
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#FFFFFF" }
                                        GradientStop { position: 0.45; color: "#FFFFFF" }
                                        GradientStop { position: 1.0; color: "#000000" }
                                    }
                                }

                                Rectangle {
                                    anchors.top: parent.top; anchors.topMargin: 1
                                    anchors.left: parent.left; anchors.leftMargin: 5
                                    anchors.right: parent.right; anchors.rightMargin: 5
                                    height: 1
                                    color: StyleGuide.palette.accentBright
                                    opacity: resetBtn.pressed ? 0.0 : 0.45
                                }
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: Theme.textMain
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            topPadding: resetBtn.pressed ? 1 : 0
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
                color: hs.wax
                radius: 6
                border.color: hs.bronze
                border.width: 1
                Layout.alignment: Qt.AlignVCenter

                Label {
                    id: spectatorLabel

                    anchors.centerIn: parent
                    text: "👁 " + qsTr("SPECTATOR MODE")
                    color: Theme.textMain
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
                            source: StyleGuide.icon_path("troop_count.png")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }

                        Label {
                            id: playerLbl

                            text: (typeof game !== 'undefined' ? game.player_troop_count : 0) + " / " + (typeof game !== 'undefined' ? game.max_troops_per_player : 0)
                            color: {
                                if (typeof game === 'undefined')
                                    return Theme.textDim;

                                var count = game.player_troop_count;
                                var max = game.max_troops_per_player;
                                if (count >= max)
                                    return hs.waxHover;

                                if (count >= max * 0.8)
                                    return hs.bronze;

                                return Theme.accent;
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
                        color: hs.bronzeDeep
                        opacity: 0.5
                        visible: !topRoot.compact
                    }

                    Item {
                        id: ownersContainer

                        property var owners: (typeof game !== 'undefined') ? game.owner_info : []

                        function player_count() {
                            var ownersList = ownersContainer.owners || [];
                            var count = 0;
                            for (var i = 0; i < ownersList.length; i++) {
                                if (ownersList[i].type === "Player")
                                    count++;

                            }
                            return count;
                        }

                        function ai_count() {
                            var ownersList = ownersContainer.owners || [];
                            var count = 0;
                            for (var i = 0; i < ownersList.length; i++) {
                                if (ownersList[i].type === "AI")
                                    count++;

                            }
                            return count;
                        }

                        function owners_tooltip() {
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
                                source: StyleGuide.icon_path("human_player.png")
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Label {
                                id: humanCountLbl

                                text: ownersContainer.player_count()
                                color: Theme.textMain
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }

                            Image {
                                width: 30
                                height: 30
                                source: StyleGuide.icon_path("ai_player.png")
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Label {
                                id: aiCountLbl

                                text: ownersContainer.ai_count()
                                color: Theme.textMain
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }

                        }

                        ToolTip {
                            visible: ownersMA.containsMouse
                            delay: 500
                            text: ownersContainer.owners_tooltip()
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
                            source: StyleGuide.icon_path("defeated.png")
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }

                        Label {
                            id: enemyLbl

                            text: (typeof game !== 'undefined' ? game.enemy_troops_defeated : 0)
                            color: Theme.textMain
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                    }

                }

                Item {
                    id: miniWrap

                    visible: !topRoot.ultra_compact
                    Layout.preferredWidth: Math.round(topPanel.height * 2.2)
                    Layout.minimumWidth: Math.round(topPanel.height * 1.6)
                    Layout.preferredHeight: topPanel.height - 8
                }

            }

        }

    }

    Rectangle {
        id: minimapContainer

        visible: !topRoot.ultra_compact
        width: 240
        height: 240
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 8
        anchors.topMargin: 8
        z: 100
        color: hs.parchmentDark
        radius: 8
        border.width: 2
        border.color: hs.bronze

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: 6
            color: Theme.bgShade

            Image {
                id: minimapImage

                property int image_version: 0

                anchors.fill: parent
                anchors.margins: 2
                source: image_version > 0 ? "image://minimap/v" + image_version : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
                cache: false
                asynchronous: false

                Connections {
                    function onMinimap_image_changed() {
                        Qt.callLater(function() {
                            minimapImage.image_version++;
                        });
                    }

                    target: game
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("MINIMAP")
                    color: Theme.textDim
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
