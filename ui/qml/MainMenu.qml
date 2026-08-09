import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design
import "ui_audio.js" as UiAudio

Item {
    id: root

    property bool game_started: false
    readonly property bool compact: width < 820
    readonly property bool narrow: width < 620
    readonly property int side_margin: Math.max(24, Math.min(72, width * 0.058))
    readonly property int top_margin: Math.max(20, Math.min(54, height * 0.058))
    readonly property int command_width: root.compact ? width - side_margin * 2 : Math.min(600, Math.max(460, width * 0.40))
    readonly property var hs: StyleGuide.historical
    readonly property color ink: "#0B0806"
    readonly property color bronze: hs.bronze

    signal open_skirmish
    signal open_campaign
    signal open_objectives
    signal open_settings
    signal load_save
    signal save_game
    signal exit_requested

    anchors.fill: parent
    z: 10
    focus: true

    function trigger_selection(index) {
        var m = menuModel.get(index);
        if (!m || (m.requiresGame && !root.game_started))
            return;
        if (m.idStr === "skirmish")
            root.open_skirmish();
        else if (m.idStr === "campaign")
            root.open_campaign();
        else if (m.idStr === "objectives")
            root.open_objectives();
        else if (m.idStr === "save")
            root.save_game();
        else if (m.idStr === "load")
            root.load_save();
        else if (m.idStr === "settings")
            root.open_settings();
        else if (m.idStr === "exit")
            root.exit_requested();
    }

    function move_selection(direction) {
        var next = commandList.currentIndex + direction;
        while (next >= 0 && next < menuModel.count) {
            var m = menuModel.get(next);
            if (!m.requiresGame || root.game_started) {
                commandList.currentIndex = next;
                return;
            }
            next += direction;
        }
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Down) {
            move_selection(1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            move_selection(-1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            trigger_selection(commandList.currentIndex);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            if (typeof mainWindow !== "undefined" && mainWindow.menu_visible && mainWindow.game_started) {
                mainWindow.menu_visible = false;
                event.accepted = true;
            }
        }
    }

    ListModel {
        id: menuModel

        ListElement {
            idStr: "skirmish"
            title: QT_TR_NOOP("Play Skirmish")
            subtitle: QT_TR_NOOP("Choose the field and deploy armies")
            detail: QT_TR_NOOP("Battle")
            requiresGame: false
            accent: "#B6362F"
        }

        ListElement {
            idStr: "campaign"
            title: QT_TR_NOOP("Campaign")
            subtitle: QT_TR_NOOP("March through the Second Punic War")
            detail: QT_TR_NOOP("War Map")
            requiresGame: false
            accent: "#C29555"
        }

        ListElement {
            idStr: "objectives"
            title: QT_TR_NOOP("Objectives")
            subtitle: QT_TR_NOOP("Review active orders")
            detail: QT_TR_NOOP("Orders")
            requiresGame: true
            accent: "#8C6A3E"
        }

        ListElement {
            idStr: "save"
            title: QT_TR_NOOP("Save Game")
            subtitle: QT_TR_NOOP("Record the current campaign state")
            detail: QT_TR_NOOP("Archive")
            requiresGame: true
            accent: "#A7814A"
        }

        ListElement {
            idStr: "load"
            title: QT_TR_NOOP("Load Game")
            subtitle: QT_TR_NOOP("Return to a saved command")
            detail: QT_TR_NOOP("Return")
            requiresGame: false
            accent: "#8A7047"
        }

        ListElement {
            idStr: "settings"
            title: QT_TR_NOOP("Settings")
            subtitle: QT_TR_NOOP("Display, audio, and controls")
            detail: QT_TR_NOOP("Options")
            requiresGame: false
            accent: "#8C6A3E"
        }

        ListElement {
            idStr: "exit"
            title: QT_TR_NOOP("Exit")
            subtitle: QT_TR_NOOP("Leave the war table")
            detail: QT_TR_NOOP("Retire")
            requiresGame: false
            accent: "#6E2B25"
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.ink
    }

    Item {
        anchors.fill: parent
        clip: true

        Image {
            id: backdrop

            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            source: "qrc:/StandardOfIron/assets/visuals/load_screen.png"
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            mipmap: true

            SequentialAnimation on scale  {
                running: root.visible
                loops: Animation.Infinite

                NumberAnimation {
                    from: 1
                    to: 1.05
                    duration: 24000
                    easing.type: Easing.InOutSine
                }

                NumberAnimation {
                    from: 1.05
                    to: 1
                    duration: 24000
                    easing.type: Easing.InOutSine
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: "#DB0B0806"
            }

            GradientStop {
                position: root.compact ? 0.75 : 0.34
                color: "#C40B0806"
            }

            GradientStop {
                position: root.compact ? 0.92 : 0.62
                color: "#570B0806"
            }

            GradientStop {
                position: 1
                color: "#140B0806"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#800B0806"
            }

            GradientStop {
                position: 0.34
                color: "#000B0806"
            }

            GradientStop {
                position: 0.76
                color: "#400B0806"
            }

            GradientStop {
                position: 1
                color: "#D90B0806"
            }
        }
    }

    Canvas {
        id: vignette

        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            var grad = ctx.createRadialGradient(width * 0.5, height * 0.48, Math.min(width, height) * 0.24, width * 0.5, height * 0.48, Math.max(width, height) * 0.76);
            grad.addColorStop(0, "rgba(11,8,6,0)");
            grad.addColorStop(0.58, "rgba(11,8,6,0.12)");
            grad.addColorStop(1, "rgba(11,8,6,0.55)");
            ctx.fillStyle = grad;
            ctx.fillRect(0, 0, width, height);
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 2
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: Theme.accentBright
            }

            GradientStop {
                position: 0.55
                color: root.bronze
            }

            GradientStop {
                position: 1
                color: "#00000000"
            }
        }
        opacity: 0.9
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: root.bronze
        opacity: 0.35
    }

    RowLayout {
        id: stage

        anchors.fill: parent
        anchors.leftMargin: root.side_margin
        anchors.rightMargin: root.side_margin
        anchors.topMargin: root.top_margin
        anchors.bottomMargin: root.top_margin
        spacing: root.compact ? 0 : Math.max(28, root.width * 0.04)

        ColumnLayout {
            id: commandColumn

            Layout.fillHeight: true
            Layout.preferredWidth: root.compact ? stage.width : root.command_width
            Layout.maximumWidth: root.compact ? stage.width : root.command_width
            spacing: Math.max(14, Math.min(26, root.height * 0.026))

            Item {
                id: titleBlock

                Layout.fillWidth: true
                Layout.preferredHeight: root.narrow ? 120 : 156

                Column {
                    id: titleStack

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 9
                    opacity: 0

                    transform: Translate {
                        id: titleSlide

                        x: -26
                    }

                    Row {
                        width: parent.width
                        height: 30
                        spacing: 11

                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 28
                            height: 28
                            source: "qrc:/StandardOfIron/assets/visuals/emblems/rome.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("SPQR")
                            color: Theme.accentBright
                            font.pixelSize: root.narrow ? 11 : 12
                            font.bold: true
                            font.letterSpacing: 3.2
                        }

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 1
                            height: 14
                            color: root.bronze
                            opacity: 0.7
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("QART-HADAST")
                            color: Theme.accentBright
                            font.pixelSize: root.narrow ? 11 : 12
                            font.bold: true
                            font.letterSpacing: 3.2
                        }

                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 28
                            height: 28
                            source: "qrc:/StandardOfIron/assets/visuals/emblems/cartaghe.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                    }

                    Item {
                        width: parent.width
                        height: root.narrow ? 44 : 62

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.verticalCenterOffset: 3
                            anchors.leftMargin: 2
                            text: qsTr("STANDARD OF IRON")
                            color: "#00060403"
                            font.family: "serif"
                            font.pixelSize: root.narrow ? 36 : 54
                            font.bold: true
                            font.letterSpacing: root.narrow ? 1.2 : 2.4
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            style: Text.Raised
                            styleColor: "#060403"
                            opacity: 0.55
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("STANDARD OF IRON")
                            color: Theme.textMain
                            font.family: "serif"
                            font.pixelSize: root.narrow ? 36 : 54
                            font.bold: true
                            font.letterSpacing: root.narrow ? 1.2 : 2.4
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            style: Text.Outline
                            styleColor: "#120D09"
                        }
                    }

                    Row {
                        width: parent.width
                        height: 3
                        spacing: 6

                        Rectangle {
                            width: 76
                            height: 3
                            color: root.bronze
                        }

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, parent.width - 82)
                            height: 1
                            gradient: Gradient {
                                orientation: Gradient.Horizontal

                                GradientStop {
                                    position: 0
                                    color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0.55)
                                }

                                GradientStop {
                                    position: 1
                                    color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0)
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Rome and Carthage at the edge of empire")
                        color: Theme.textSubLite
                        font.pixelSize: root.narrow ? 13 : 15
                        font.letterSpacing: 0.8
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }

                ParallelAnimation {
                    running: true

                    NumberAnimation {
                        target: titleStack
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 420
                        easing.type: Easing.OutCubic
                    }

                    NumberAnimation {
                        target: titleSlide
                        property: "x"
                        from: -26
                        to: 0
                        duration: 520
                        easing.type: Easing.OutCubic
                    }
                }
            }

            ListView {
                id: commandList

                Layout.fillWidth: true
                Layout.fillHeight: true
                model: menuModel
                currentIndex: 0
                spacing: 9
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height

                delegate: Item {
                    id: commandItem

                    required property int index
                    required property string idStr
                    required property string title
                    required property string subtitle
                    required property string detail
                    required property bool requiresGame
                    required property string accent

                    readonly property bool item_enabled: !requiresGame || root.game_started
                    readonly property bool selected: ListView.isCurrentItem && item_enabled
                    readonly property bool hovered: menuMouse.containsMouse && item_enabled

                    width: commandList.width
                    height: root.narrow ? 62 : 70
                    opacity: item_enabled ? 1 : 0.38

                    Item {
                        id: rowBody

                        anchors.fill: parent
                        opacity: 0

                        transform: Translate {
                            id: rowSlide

                            x: 30
                        }

                        Rectangle {
                            id: rowPlate

                            anchors.fill: parent
                            radius: 5
                            border.width: 1
                            border.color: commandItem.selected ? Theme.accentBright : (commandItem.hovered ? root.bronze : Qt.rgba(0.65, 0.49, 0.28, 0.42))
                            clip: true

                            gradient: Gradient {
                                orientation: Gradient.Horizontal

                                GradientStop {
                                    position: 0
                                    color: commandItem.selected ? "#7C2823" : (commandItem.hovered ? "#332619" : "#E8150F0A")
                                }

                                GradientStop {
                                    position: 0.55
                                    color: commandItem.selected ? "#99382016" : (commandItem.hovered ? "#D91E1610" : "#CC100B07")
                                }

                                GradientStop {
                                    position: 1
                                    color: commandItem.selected ? "#551A120C" : "#AA0C0806"
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: commandItem.selected ? 5 : 3
                                color: commandItem.accent
                                opacity: commandItem.selected ? 1 : (commandItem.hovered ? 0.85 : 0.55)

                                Behavior on width  {
                                    NumberAnimation {
                                        duration: Theme.animFast
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 1
                                height: 1
                                color: Theme.accentBright
                                opacity: commandItem.selected ? 0.55 : 0.12
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 1
                                height: 1
                                color: "#000000"
                                opacity: 0.4
                            }

                            Behavior on border.color  {
                                ColorAnimation {
                                    duration: Theme.animNormal
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 16
                            anchors.topMargin: 9
                            anchors.bottomMargin: 9
                            spacing: 13

                            Rectangle {
                                Layout.preferredWidth: root.narrow ? 26 : 32
                                Layout.preferredHeight: root.narrow ? 26 : 32
                                Layout.alignment: Qt.AlignVCenter
                                radius: 3
                                color: commandItem.selected ? "#BB2A1710" : "#99100B07"
                                border.width: 1
                                border.color: commandItem.selected ? Theme.accentBright : Qt.rgba(0.65, 0.49, 0.28, 0.45)
                                visible: !root.narrow || commandItem.width > 330

                                Text {
                                    anchors.centerIn: parent
                                    text: Design.Numerals.ordinal(commandItem.index)
                                    color: commandItem.selected ? Theme.accentBright : Theme.textDim
                                    font.family: "serif"
                                    font.pixelSize: root.narrow ? 11 : 13
                                    font.bold: true
                                    font.letterSpacing: 0.5
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr(commandItem.title)
                                    color: commandItem.selected ? Theme.textMain : Theme.textBright
                                    font.family: "serif"
                                    font.pixelSize: root.narrow ? 18 : 21
                                    font.bold: true
                                    font.letterSpacing: commandItem.selected ? 0.9 : 0.3
                                    elide: Text.ElideRight
                                    maximumLineCount: 1

                                    Behavior on font.letterSpacing  {
                                        NumberAnimation {
                                            duration: Theme.animNormal
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr(commandItem.subtitle)
                                    color: commandItem.selected ? Theme.accentBright : Theme.textDim
                                    font.pixelSize: root.narrow ? 11 : 13
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: detailLabel.implicitWidth + 16
                                radius: 2
                                color: commandItem.selected ? "#AA2C1811" : "transparent"
                                border.width: 1
                                border.color: commandItem.selected ? Qt.rgba(0.91, 0.79, 0.55, 0.6) : Qt.rgba(0.65, 0.49, 0.28, 0.32)
                                visible: commandItem.width > 400

                                Text {
                                    id: detailLabel

                                    anchors.centerIn: parent
                                    text: qsTr(commandItem.detail).toUpperCase()
                                    color: commandItem.selected ? Theme.accentBright : Theme.textDim
                                    font.pixelSize: 9
                                    font.bold: true
                                    font.letterSpacing: 1.4
                                }
                            }

                            Item {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 16

                                Rectangle {
                                    x: commandItem.selected ? 3 : 0
                                    y: 3
                                    width: 2
                                    height: 8
                                    radius: 1
                                    rotation: -40
                                    transformOrigin: Item.Bottom
                                    color: commandItem.selected ? Theme.accentBright : Theme.textHint
                                    opacity: commandItem.selected ? 1 : 0.55

                                    Behavior on x  {
                                        NumberAnimation {
                                            duration: Theme.animFast
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }

                                Rectangle {
                                    x: commandItem.selected ? 3 : 0
                                    y: 8
                                    width: 2
                                    height: 8
                                    radius: 1
                                    rotation: 40
                                    transformOrigin: Item.Top
                                    color: commandItem.selected ? Theme.accentBright : Theme.textHint
                                    opacity: commandItem.selected ? 1 : 0.55

                                    Behavior on x  {
                                        NumberAnimation {
                                            duration: Theme.animFast
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }
                            }
                        }
                    }

                    SequentialAnimation {
                        running: true

                        PauseAnimation {
                            duration: 40 + commandItem.index * 26
                        }

                        ParallelAnimation {
                            NumberAnimation {
                                target: rowBody
                                property: "opacity"
                                from: 0
                                to: 1
                                duration: 280
                                easing.type: Easing.OutCubic
                            }

                            NumberAnimation {
                                target: rowSlide
                                property: "x"
                                from: 30
                                to: 0
                                duration: 400
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    MouseArea {
                        id: menuMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: commandItem.item_enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                        onEntered: {
                            if (commandItem.item_enabled) {
                                commandList.currentIndex = commandItem.index;
                                if (typeof game !== "undefined")
                                    UiAudio.play_hover(game.audio_system);
                            }
                        }
                        onClicked: {
                            if (typeof game === "undefined") {
                                if (commandItem.item_enabled)
                                    root.trigger_selection(commandItem.index);
                                return;
                            }
                            if (!commandItem.item_enabled) {
                                UiAudio.play_error(game.audio_system);
                                return;
                            }
                            UiAudio.play_click(game.audio_system);
                            root.trigger_selection(commandItem.index);
                        }
                    }
                }
            }

            RowLayout {
                id: hintRow

                Layout.fillWidth: true
                Layout.preferredHeight: 20
                spacing: 16

                Repeater {
                    model: root.game_started ? ["navigate", "confirm", "resume"] : ["navigate", "confirm"]

                    delegate: Row {
                        required property string modelData

                        spacing: 7

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: hintKey.implicitWidth + 12
                            height: 18
                            radius: 2
                            color: "#99120D09"
                            border.width: 1
                            border.color: Qt.rgba(0.65, 0.49, 0.28, 0.45)

                            Text {
                                id: hintKey

                                anchors.centerIn: parent
                                text: modelData === "navigate" ? "↑ ↓" : (modelData === "confirm" ? qsTr("Enter") : qsTr("Esc"))
                                color: Theme.textSubLite
                                font.pixelSize: 9
                                font.bold: true
                                font.letterSpacing: 1
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData === "navigate" ? qsTr("Navigate") : (modelData === "confirm" ? qsTr("Confirm") : qsTr("Resume battle"))
                            color: Theme.textDim
                            font.pixelSize: 11
                            font.letterSpacing: 0.6
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        Item {
            id: visualColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.compact

            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                opacity: 0

                Item {
                    id: crestFrame

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    readonly property real crest_size: Math.min(parent.width * 0.68, crestFrame.height * 0.86, 360)

                    Canvas {
                        id: crestGlow

                        anchors.centerIn: crest
                        width: crestFrame.crest_size * 1.75
                        height: crestFrame.crest_size * 1.75
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            var grad = ctx.createRadialGradient(width / 2, height / 2, 0, width / 2, height / 2, width / 2);
                            grad.addColorStop(0, "rgba(226,168,88,0.30)");
                            grad.addColorStop(0.42, "rgba(178,116,54,0.14)");
                            grad.addColorStop(1, "rgba(178,116,54,0)");
                            ctx.fillStyle = grad;
                            ctx.beginPath();
                            ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2);
                            ctx.fill();
                        }
                        Component.onCompleted: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()

                        SequentialAnimation on opacity  {
                            running: root.visible
                            loops: Animation.Infinite

                            NumberAnimation {
                                from: 0.72
                                to: 1
                                duration: 3200
                                easing.type: Easing.InOutSine
                            }

                            NumberAnimation {
                                from: 1
                                to: 0.72
                                duration: 3200
                                easing.type: Easing.InOutSine
                            }
                        }
                    }

                    Image {
                        id: crest

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: crestFrame.crest_size
                        height: crestFrame.crest_size
                        source: "qrc:/StandardOfIron/assets/visuals/standard_of_iron.png"
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        smooth: true
                        mipmap: true

                        transform: Translate {
                            id: crestBob
                        }

                        SequentialAnimation {
                            running: root.visible
                            loops: Animation.Infinite

                            NumberAnimation {
                                target: crestBob
                                property: "y"
                                from: -5
                                to: 5
                                duration: 5200
                                easing.type: Easing.InOutSine
                            }

                            NumberAnimation {
                                target: crestBob
                                property: "y"
                                from: 5
                                to: -5
                                duration: 5200
                                easing.type: Easing.InOutSine
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.horizontalCenter
                        anchors.rightMargin: 16
                        height: 1
                        gradient: Gradient {
                            orientation: Gradient.Horizontal

                            GradientStop {
                                position: 0
                                color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0)
                            }

                            GradientStop {
                                position: 1
                                color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0.8)
                            }
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 8
                        height: 8
                        rotation: 45
                        color: root.bronze
                        opacity: 0.9
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.horizontalCenter
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        height: 1
                        gradient: Gradient {
                            orientation: Gradient.Horizontal

                            GradientStop {
                                position: 0
                                color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0.8)
                            }

                            GradientStop {
                                position: 1
                                color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0)
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    text: qsTr("SECOND PUNIC WAR")
                    color: Theme.textMain
                    font.family: "serif"
                    font.pixelSize: 19
                    font.bold: true
                    font.letterSpacing: 3
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 5
                    text: qsTr("Legions, fleets, elephants, and contested supply lines")
                    color: Theme.textDim
                    font.pixelSize: 12
                    font.letterSpacing: 0.4
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 22
                    spacing: 14

                    Repeater {
                        model: ["hannibal", "scipio"]

                        delegate: Rectangle {
                            id: plaque

                            required property string modelData

                            readonly property bool carthaginian: modelData === "hannibal"

                            width: 252
                            height: 82
                            radius: 5
                            color: "#E0140E09"
                            border.width: 1
                            border.color: Qt.rgba(root.bronze.r, root.bronze.g, root.bronze.b, 0.55)

                            Row {
                                anchors.fill: parent
                                anchors.margins: 11
                                spacing: 12

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 58
                                    height: 58
                                    radius: 3
                                    color: "#0C0806"
                                    border.width: 1
                                    border.color: root.bronze

                                    Image {
                                        id: portrait

                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: "qrc:/StandardOfIron/assets/visuals/" + plaque.modelData + ".png"
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: true
                                        mipmap: true
                                    }

                                    Rectangle {
                                        anchors.fill: portrait
                                        color: "#4A2A12"
                                        opacity: 0.3
                                    }

                                    Rectangle {
                                        anchors.fill: portrait
                                        gradient: Gradient {
                                            GradientStop {
                                                position: 0
                                                color: "#000C0806"
                                            }

                                            GradientStop {
                                                position: 1
                                                color: "#990C0806"
                                            }
                                        }
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 100
                                    spacing: 4

                                    Text {
                                        width: parent.width
                                        text: plaque.carthaginian ? qsTr("Hannibal Barca") : qsTr("Scipio Africanus")
                                        color: Theme.textBright
                                        font.family: "serif"
                                        font.pixelSize: 13
                                        font.bold: true
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }

                                    Rectangle {
                                        width: 26
                                        height: 1
                                        color: root.bronze
                                        opacity: 0.7
                                    }

                                    Text {
                                        width: parent.width
                                        text: plaque.carthaginian ? qsTr("CARTHAGE") : qsTr("ROME")
                                        color: Theme.textDim
                                        font.pixelSize: 10
                                        font.letterSpacing: 1.6
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }
                                }

                                Image {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 24
                                    height: 24
                                    source: plaque.carthaginian ? "qrc:/StandardOfIron/assets/visuals/emblems/cartaghe.png" : "qrc:/StandardOfIron/assets/visuals/emblems/rome.png"
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    opacity: 0.9
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                }

                NumberAnimation on opacity  {
                    running: true
                    from: 0
                    to: 1
                    duration: 700
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    Text {
        objectName: "mainMenuVersionLabel"

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 14
        anchors.bottomMargin: 8
        text: "v" + Qt.application.version
        color: Theme.textDim
        font.pixelSize: 10
        font.letterSpacing: 0.8
        opacity: 0.8
        z: 12
    }
}
