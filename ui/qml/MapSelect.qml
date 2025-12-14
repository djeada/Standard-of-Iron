import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0

Item {
    id: root

    property var mapsModel: (typeof game !== "undefined" && game.available_maps) ? game.available_maps : []
    property bool mapsLoading: (typeof game !== "undefined" && game.maps_loading) ? game.maps_loading : false
    property int selectedMapIndex: -1
    property var selectedMapData: null
    property string selectedMapPath: ""
    property string validationError: ""
    property var availableNations: []

    signal mapChosen(string map_path, var playerConfigs)
    signal cancelled()

    function refreshAvailableNations() {
        if (typeof game !== "undefined" && game.available_nations)
            availableNations = game.available_nations;
        else
            availableNations = [];
    }

    function defaultNationEntry() {
        if (availableNations && availableNations.length > 0)
            return availableNations[0];

        return {
            "id": "roman_republic",
            "name": qsTr("Roman Republic")
        };
    }

    function hasMinimumDistinctTeams() {
        let enabledPlayers = [];
        for (let i = 0; i < playersModel.count; i++) {
            let p = playersModel.get(i);
            if (p.isEnabled) {
                enabledPlayers.push(p);
            }
        }

        if (enabledPlayers.length < 2)
            return false;

        let teams = new Set();
        for (let i = 0; i < enabledPlayers.length; i++) {
            teams.add(enabledPlayers[i].team_id);
        }
        return teams.size >= 2;
    }

    function updateValidationError() {
        let enabledCount = 0;
        for (let i = 0; i < playersModel.count; i++) {
            if (playersModel.get(i).isEnabled)
                enabledCount++;
        }
        
        if (enabledCount < 2)
            validationError = "Need at least 2 enabled players to start";
        else if (!hasMinimumDistinctTeams())
            validationError = "At least two teams must be selected to start a match";
        else
            validationError = "";
    }

    function field(obj, key) {
        return (obj && obj[key] !== undefined) ? String(obj[key]) : "";
    }

    function nationEmblemFor(nationId) {
        if (!nationId || !Theme.nationEmblems)
            return "";

        let emblem = Theme.nationEmblems[nationId];
        if (emblem === undefined || emblem === null)
            return "";

        return String(emblem);
    }

    function getMapData(index) {
        if (!mapsModel || index < 0 || index >= (mapsModel.length || list.count))
            return null;

        let m = (mapsModel.length !== undefined) ? mapsModel[index] : null;
        if (m && m.get)
            return m.get(index);

        return m;
    }

    function initializePlayers(mapData) {
        playersModel.clear();
        if (!mapData || !mapData.player_ids || mapData.player_ids.length === 0)
            return ;

        let humanPlayerId = mapData.player_ids.length > 0 ? mapData.player_ids[0] : 1;
        let defaultNation = defaultNationEntry();
        playersModel.append({
            "player_id": humanPlayerId,
            "playerName": "Player " + (humanPlayerId + 1),
            "colorIndex": 0,
            "colorHex": Theme.playerColors[0].hex,
            "colorName": Theme.playerColors[0].name,
            "team_id": 0,
            "teamIcon": Theme.teamIcons[0],
            "nationId": defaultNation.id,
            "nationName": defaultNation.name,
            "isHuman": true,
            "isEnabled": true
        });
        let cpuId = mapData.player_ids.find(function(id) {
            return id !== humanPlayerId;
        });
        if (cpuId !== undefined)
            addCPU();

        updateValidationError();
    }

    function addCPU() {
        if (!selectedMapData || !selectedMapData.player_ids)
            return ;

        if (playersModel.count >= selectedMapData.player_ids.length)
            return ;

        let usedIds = [];
        for (let i = 0; i < playersModel.count; i++) usedIds.push(playersModel.get(i).player_id)
        let nextId = -1;
        for (let j = 0; j < selectedMapData.player_ids.length; j++) {
            if (usedIds.indexOf(selectedMapData.player_ids[j]) === -1) {
                nextId = selectedMapData.player_ids[j];
                break;
            }
        }
        if (nextId === -1)
            return ;

        let usedColors = [];
        for (let k = 0; k < playersModel.count; k++) usedColors.push(playersModel.get(k).colorIndex)
        let colorIdx = 0;
        for (let c = 0; c < Theme.playerColors.length; c++) {
            if (usedColors.indexOf(c) === -1) {
                colorIdx = c;
                break;
            }
        }
        let defaultTeamId = playersModel.count > 0 ? 1 : 0;
        let defaultNation = defaultNationEntry();
        playersModel.append({
            "player_id": nextId,
            "playerName": "CPU " + nextId,
            "colorIndex": colorIdx,
            "colorHex": Theme.playerColors[colorIdx].hex,
            "colorName": Theme.playerColors[colorIdx].name,
            "team_id": defaultTeamId,
            "teamIcon": Theme.teamIcons[defaultTeamId],
            "nationId": defaultNation.id,
            "nationName": defaultNation.name,
            "isHuman": false,
            "isEnabled": true
        });
        updateValidationError();
    }

    function removePlayer(index) {
        if (index < 0 || index >= playersModel.count)
            return ;

        let p = playersModel.get(index);
        if (p.isHuman)
            return ;

        playersModel.remove(index);
        updateValidationError();
    }

    function cyclePlayerColor(index) {
        if (index < 0 || index >= playersModel.count)
            return ;

        let p = playersModel.get(index);
        let usedColors = [];
        for (let i = 0; i < playersModel.count; i++) {
            if (i !== index)
                usedColors.push(playersModel.get(i).colorIndex);

        }
        let startIdx = p.colorIndex;
        let newIdx = (startIdx + 1) % Theme.playerColors.length;
        let attempts = 0;
        while (usedColors.indexOf(newIdx) !== -1 && attempts < Theme.playerColors.length) {
            newIdx = (newIdx + 1) % Theme.playerColors.length;
            attempts++;
        }
        if (attempts >= Theme.playerColors.length)
            newIdx = (startIdx + 1) % Theme.playerColors.length;

        playersModel.setProperty(index, "colorIndex", newIdx);
        playersModel.setProperty(index, "colorHex", Theme.playerColors[newIdx].hex);
        playersModel.setProperty(index, "colorName", Theme.playerColors[newIdx].name);
    }

    function cyclePlayerTeam(index) {
        if (index < 0 || index >= playersModel.count)
            return ;

        let p = playersModel.get(index);
        let maxTeam = Math.min(8, playersModel.count);
        let newTeamId = (p.team_id + 1) % (maxTeam + 1);
        playersModel.setProperty(index, "team_id", newTeamId);
        playersModel.setProperty(index, "teamIcon", Theme.teamIcons[newTeamId]);
        updateValidationError();
    }

    function cyclePlayerNation(index) {
        if (index < 0 || index >= playersModel.count)
            return ;

        if (!availableNations || availableNations.length === 0)
            return ;

        let p = playersModel.get(index);
        let currentId = p.nationId || availableNations[0].id;
        let nextIndex = 0;
        for (let i = 0; i < availableNations.length; i++) {
            if (availableNations[i].id === currentId) {
                nextIndex = (i + 1) % availableNations.length;
                break;
            }
        }
        let nextNation = availableNations[nextIndex];
        playersModel.setProperty(index, "nationId", nextNation.id);
        playersModel.setProperty(index, "nationName", nextNation.name);
    }

    function togglePlayerEnabled(index) {
        if (index < 0 || index >= playersModel.count)
            return ;

        let p = playersModel.get(index);
        let newEnabled = !p.isEnabled;
        playersModel.setProperty(index, "isEnabled", newEnabled);
        updateValidationError();
    }

    function getPlayerConfigs() {
        let configs = [];
        for (let i = 0; i < playersModel.count; i++) {
            let p = playersModel.get(i);
            if (!p.isEnabled)
                continue;

            let config = {
                "player_id": p.player_id,
                "colorHex": p.colorHex,
                "team_id": p.team_id,
                "nationId": p.nationId,
                "isHuman": p.isHuman
            };
            console.log("MapSelect: Player", p.player_id, "config - Team:", p.team_id, "Color:", p.colorHex, "Nation:", p.nationId, "Human:", p.isHuman);
            configs.push(config);
        }
        return configs;
    }

    function acceptSelection() {
        if (selectedMapIndex < 0 || !selectedMapPath)
            return ;

        let enabledCount = 0;
        for (let i = 0; i < playersModel.count; i++) {
            if (playersModel.get(i).isEnabled)
                enabledCount++;
        }

        if (enabledCount < 2) {
            console.log("MapSelect: Need at least 2 enabled players to start");
            updateValidationError();
            return ;
        }
        if (!hasMinimumDistinctTeams()) {
            console.log("MapSelect: Need at least 2 different teams to start");
            updateValidationError();
            return ;
        }
        validationError = "";
        let configs = getPlayerConfigs();
        console.log("MapSelect: Starting game with", configs.length, "enabled players");
        root.mapChosen(selectedMapPath, configs);
    }

    function playerColorClicked(index) {
        cyclePlayerColor(index);
    }

    function playerTeamClicked(index) {
        cyclePlayerTeam(index);
    }

    function addCPUClicked() {
        addCPU();
    }

    function removePlayerClicked(index) {
        removePlayer(index);
    }

    Component.onCompleted: refreshAvailableNations()
    anchors.fill: parent
    focus: true
    onVisibleChanged: {
        if (visible) {
            root.focus = true;
            selectedMapIndex = -1;
            selectedMapData = null;
            selectedMapPath = "";
            playersModel.clear();
            refreshAvailableNations();
            if (typeof game !== "undefined" && game.start_loading_maps)
                game.start_loading_maps();

        }
    }
    Keys.onPressed: function(event) {
        if (!visible)
            return ;

        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            acceptSelection();
            event.accepted = true;
        } else if (event.key === Qt.Key_Down) {
            if (list.count > 0)
                list.currentIndex = Math.min(list.currentIndex + 1, list.count - 1);

            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            if (list.count > 0)
                list.currentIndex = Math.max(list.currentIndex - 1, 0);

            event.accepted = true;
        }
    }

    ListModel {
        id: playersModel
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: panel

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, 1300)
        height: Math.min(parent.height * 0.88, 800)
        radius: Theme.radiusPanel
        color: Theme.panelBase
        border.color: Theme.panelBr
        border.width: 1
        opacity: 0.98
        clip: true

        Item {
            id: left

            width: Math.max(360, Math.min(panel.width * 0.38, 460))

            anchors {
                top: parent.top
                left: parent.left
                bottom: footer.top
                topMargin: Theme.spacingXLarge
                leftMargin: Theme.spacingXLarge
                bottomMargin: Theme.spacingMedium
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: Theme.spacingMedium

                Text {
                    id: leftTitle

                    text: qsTr("Maps")
                    color: Theme.textMain
                    font.pixelSize: 20
                    font.bold: true
                    Layout.fillWidth: true
                }

                MapPreview {
                    id: mapPreviewLeft

                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 240 : 0
                    visible: selectedMapData !== null
                    mapPath: selectedMapPath
                    playerConfigs: getPlayerConfigs()
                }

                Rectangle {
                    id: listFrame

                    color: Qt.rgba(0, 0, 0, 0)
                    radius: Theme.radiusLarge
                    border.color: Theme.panelBr
                    border.width: 1
                    clip: true
                    Layout.fillWidth: true
                    Layout.fillHeight: (!mapsLoading && list.count > 0)

                    ListView {
                        id: list

                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall
                        model: mapsModel
                        spacing: Theme.spacingSmall
                        currentIndex: (count > 0 ? 0 : -1)
                        keyNavigationWraps: false
                        boundsBehavior: Flickable.StopAtBounds
                        onCurrentIndexChanged: {
                            if (currentIndex < 0) {
                                selectedMapIndex = -1;
                                selectedMapData = null;
                                selectedMapPath = "";
                                playersModel.clear();
                                return ;
                            }
                            selectedMapIndex = currentIndex;
                            selectedMapData = getMapData(currentIndex);
                            selectedMapPath = selectedMapData ? (selectedMapData.path || selectedMapData.file || "") : "";
                            initializePlayers(selectedMapData);
                        }

                        delegate: Item {
                            id: row

                            width: list.width
                            height: 72

                            MouseArea {
                                id: rowMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.PointingHandCursor
                                onClicked: list.currentIndex = index
                                onDoubleClicked: acceptSelection()
                            }

                            Rectangle {
                                id: card

                                anchors.fill: parent
                                radius: Theme.radiusLarge
                                clip: true
                                color: rowMouse.containsPress ? Theme.hoverBg : (index === list.currentIndex ? Theme.selectedBg : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0)))
                                border.width: 1
                                border.color: (index === list.currentIndex) ? Theme.selectedBr : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : Theme.thumbBr)

                                Rectangle {
                                    id: thumbWrap

                                    width: 76
                                    height: 54
                                    radius: Theme.radiusMedium
                                    color: Theme.cardBase
                                    border.color: Theme.thumbBr
                                    border.width: 1
                                    clip: true

                                    anchors {
                                        left: parent.left
                                        leftMargin: Theme.spacingSmall
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Image {
                                        id: thumbImage

                                        anchors.fill: parent
                                        source: (typeof thumbnail !== "undefined" && thumbnail !== "") ? thumbnail : ""
                                        asynchronous: true
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        color: Theme.cardBase
                                        visible: !thumbImage.visible

                                        Text {
                                            anchors.centerIn: parent
                                            text: "🗺"
                                            font.pixelSize: 24
                                            color: Theme.textDim
                                        }

                                    }

                                }

                                Item {
                                    height: 54

                                    anchors {
                                        left: thumbWrap.right
                                        right: parent.right
                                        leftMargin: Theme.spacingSmall
                                        rightMargin: Theme.spacingSmall
                                        verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        id: map_name

                                        text: (typeof name !== "undefined") ? String(name) : (typeof modelData === "string" ? modelData : (modelData && modelData.name ? String(modelData.name) : ""))
                                        color: (index === list.currentIndex) ? Theme.textMain : Theme.textBright
                                        font.pixelSize: (index === list.currentIndex) ? 17 : 15
                                        font.bold: (index === list.currentIndex)
                                        elide: Text.ElideRight

                                        anchors {
                                            top: parent.top
                                            left: parent.left
                                            right: parent.right
                                        }

                                        Behavior on font.pixelSize {
                                            NumberAnimation {
                                                duration: Theme.animNormal
                                            }

                                        }

                                    }

                                    Text {
                                        text: (typeof description !== "undefined") ? String(description) : (modelData && modelData.description ? String(modelData.description) : "")
                                        color: (index === list.currentIndex) ? Theme.accentBright : Theme.textSub
                                        font.pixelSize: 12
                                        elide: Text.ElideRight

                                        anchors {
                                            left: parent.left
                                            right: parent.right
                                            bottom: parent.bottom
                                        }

                                    }

                                    Text {
                                        text: "›"
                                        font.pointSize: 18
                                        color: (index === list.currentIndex) ? Theme.textMain : Theme.textHint

                                        anchors {
                                            right: parent.right
                                            rightMargin: 0
                                            verticalCenter: parent.verticalCenter
                                        }

                                    }

                                }

                                Behavior on color {
                                    ColorAnimation {
                                        duration: Theme.animNormal
                                    }

                                }

                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: Theme.animNormal
                                    }

                                }

                            }

                        }

                    }

                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: visible
                    visible: list.count === 0 && !mapsLoading

                    Text {
                        text: qsTr("No maps available")
                        color: Theme.textSub
                        font.pixelSize: 14
                        anchors.centerIn: parent
                    }

                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: visible
                    visible: mapsLoading

                    Column {
                        anchors.centerIn: parent
                        spacing: Theme.spacingSmall

                        Text {
                            text: "⟳"
                            font.pixelSize: 24
                            color: Theme.accent
                            anchors.horizontalCenter: parent.horizontalCenter

                            RotationAnimator on rotation {
                                from: 0
                                to: 360
                                duration: 1500
                                loops: Animation.Infinite
                                running: mapsLoading
                            }

                        }

                        Text {
                            text: qsTr("Loading maps...")
                            color: Theme.textSub
                            font.pixelSize: 12
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                    }

                }

            }

        }

        Item {
            id: right

            anchors {
                top: parent.top
                bottom: footer.top
                right: parent.right
                left: left.right
                leftMargin: Theme.spacingXLarge
                rightMargin: Theme.spacingXLarge
                topMargin: Theme.spacingXLarge
                bottomMargin: Theme.spacingMedium
            }

            Text {
                id: breadcrumb

                text: selectedMapData ? qsTr("► %1").arg(field(selectedMapData, "name")) : qsTr("Select a map to continue")
                color: selectedMapData ? Theme.accent : Theme.textHint
                font.pixelSize: 13
                font.italic: !selectedMapData
                elide: Text.ElideRight

                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                }

            }

            Item {
                id: loadingIndicator

                visible: mapsLoading && list.count === 0
                height: 100

                anchors {
                    top: breadcrumb.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: Theme.spacingXLarge * 2
                }

                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacingMedium

                    Text {
                        text: "⟳"
                        font.pixelSize: 40
                        color: Theme.accent
                        anchors.horizontalCenter: parent.horizontalCenter

                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 1500
                            loops: Animation.Infinite
                            running: loadingIndicator.visible
                        }

                    }

                    Text {
                        text: qsTr("Loading maps...")
                        color: Theme.textSub
                        font.pixelSize: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                }

            }

            Item {
                id: loadingSkeleton

                visible: !selectedMapData && !mapsLoading && list.currentIndex >= 0
                height: 200

                anchors {
                    top: breadcrumb.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: Theme.spacingMedium
                }

                Column {
                    anchors.fill: parent
                    spacing: Theme.spacingMedium

                    Rectangle {
                        width: parent.width * 0.6
                        height: 28
                        radius: Theme.radiusSmall
                        color: Theme.cardBase
                        opacity: 0.3

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: loadingSkeleton.visible

                            NumberAnimation {
                                to: 0.6
                                duration: 800
                            }

                            NumberAnimation {
                                to: 0.3
                                duration: 800
                            }

                        }

                    }

                    Rectangle {
                        width: parent.width * 0.8
                        height: 16
                        radius: Theme.radiusSmall
                        color: Theme.cardBase
                        opacity: 0.3

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: loadingSkeleton.visible

                            NumberAnimation {
                                to: 0.6
                                duration: 800
                                easing.type: Easing.InOutQuad
                            }

                            NumberAnimation {
                                to: 0.3
                                duration: 800
                                easing.type: Easing.InOutQuad
                            }

                        }

                    }

                    Rectangle {
                        width: parent.width * 0.7
                        height: 16
                        radius: Theme.radiusSmall
                        color: Theme.cardBase
                        opacity: 0.3

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: loadingSkeleton.visible

                            NumberAnimation {
                                to: 0.6
                                duration: 800
                                easing.type: Easing.InOutQuad
                            }

                            NumberAnimation {
                                to: 0.3
                                duration: 800
                                easing.type: Easing.InOutQuad
                            }

                        }

                    }

                    Text {
                        text: qsTr("Loading map details...")
                        color: Theme.textHint
                        font.pixelSize: 12
                        font.italic: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                }

            }

            Text {
                id: title

                text: {
                    var it = selectedMapData;
                    var t = field(it, "name");
                    return t || field(it, "path") || qsTr("No Map Selected");
                }
                visible: selectedMapData !== null
                color: Theme.textMain
                font.pixelSize: 24
                font.bold: true
                elide: Text.ElideRight

                anchors {
                    top: breadcrumb.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: Theme.spacingSmall
                }

            }

            Text {
                id: descr

                text: field(selectedMapData, "description")
                visible: selectedMapData !== null
                color: Theme.textSubLite
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                lineHeight: 1.3

                anchors {
                    top: title.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: Theme.spacingSmall
                }

            }

            Rectangle {
                id: playerConfigPanel

                height: Math.min(240, (playersModel.count * 60) + 90)
                radius: Theme.radiusLarge
                color: Theme.cardBaseA
                border.color: Theme.panelBr
                border.width: 1
                visible: selectedMapData !== null

                anchors {
                    top: descr.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: Theme.spacingMedium
                }

                Column {
                    spacing: Theme.spacingMedium

                    anchors {
                        fill: parent
                        margins: Theme.spacingMedium + 2
                    }

                    Row {
                        spacing: Theme.spacingSmall + 2

                        Text {
                            text: qsTr("Players")
                            color: Theme.textMain
                            font.pixelSize: 17
                            font.bold: true
                        }

                        Rectangle {
                            width: 30
                            height: 22
                            radius: Theme.radiusSmall
                            color: Theme.selectedBg
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: playersModel.count
                                color: Theme.textMain
                                font.pixelSize: 13
                                font.bold: true
                            }

                        }

                        Text {
                            text: qsTr("• Click color/team to cycle")
                            color: Theme.textSubLite
                            font.pixelSize: 11
                            font.italic: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: qsTr("• Click nation tag to change")
                            color: Theme.textSubLite
                            font.pixelSize: 11
                            font.italic: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                    }

                    ListView {
                        id: playersList

                        width: parent.width
                        height: Math.min(200, playersModel.count * 60)
                        model: playersModel
                        spacing: Theme.spacingMedium
                        clip: true

                        delegate: Rectangle {
                            id: playerCard

                            width: playersList.width
                            height: 52
                            radius: Theme.radiusMedium
                            color: playerCardMouse.containsMouse ? Qt.lighter(Theme.cardBaseB, 1.1) : Theme.cardBaseB
                            border.color: model.isHuman ? Theme.accent : (playerCardMouse.containsMouse ? Theme.selectedBr : Theme.thumbBr)
                            border.width: model.isHuman ? 1.5 : (playerCardMouse.containsMouse ? 1.5 : 1)

                            MouseArea {
                                id: playerCardMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            Rectangle {
                                height: 1
                                color: Qt.rgba(1, 1, 1, 0.05)

                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    top: parent.top
                                }

                            }

                            Item {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSmall + 2

                                Rectangle {
                                    id: enabledCheckbox

                                    width: 32
                                    height: 32
                                    radius: Theme.radiusSmall
                                    anchors.left: parent.left
                                    anchors.leftMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: enabledCheckMA.containsMouse ? Qt.lighter(Theme.cardBase, 1.2) : Theme.cardBase
                                    border.color: model.isEnabled ? Theme.accent : Theme.thumbBr
                                    border.width: enabledCheckMA.containsMouse ? 2 : 1
                                    ToolTip.visible: enabledCheckMA.containsMouse
                                    ToolTip.text: model.isEnabled ? qsTr("Disable player (spectator mode)") : qsTr("Enable player")

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.isEnabled ? "✓" : ""
                                        color: Theme.accent
                                        font.pixelSize: 18
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: enabledCheckMA

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: togglePlayerEnabled(index)
                                    }

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: Theme.animFast
                                        }
                                    }

                                    Behavior on border.color {
                                        ColorAnimation {
                                            duration: Theme.animFast
                                        }
                                    }

                                    Behavior on border.width {
                                        NumberAnimation {
                                            duration: Theme.animFast
                                        }
                                    }

                                }

                                Text {
                                    id: playerNameText

                                    anchors.left: enabledCheckbox.right
                                    anchors.leftMargin: Theme.spacingSmall
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: model.playerName || ""
                                    color: model.isEnabled ? (model.isHuman ? Theme.accentBright : Theme.textBright) : Theme.textDim
                                    font.pixelSize: model.isHuman ? 15 : 14
                                    font.bold: true
                                    opacity: model.isEnabled ? 1.0 : 0.5
                                }

                                Row {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.spacingMedium
                                    opacity: model.isEnabled ? 1.0 : 0.4

                                    Rectangle {
                                        width: 105
                                        height: playerCard.height - (Theme.spacingSmall + 2) * 2 - 4
                                        radius: Theme.radiusSmall + 1
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.cardBase
                                        border.color: model.colorHex || Theme.textDim
                                        border.width: colorMA.containsMouse ? 3 : 2
                                        ToolTip.visible: colorMA.containsMouse
                                        ToolTip.text: qsTr("Player color: %1 - Click to change").arg(model.colorName || qsTr("Color"))

                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: 1
                                            radius: parent.radius - 1
                                            color: "transparent"
                                            border.color: model.colorHex || Theme.textDim
                                            border.width: 1
                                            opacity: 0.3
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: model.colorName || "Color"
                                            color: model.colorHex || Theme.textMain
                                            font.pixelSize: 13
                                            font.bold: true
                                        }

                                        MouseArea {
                                            id: colorMA

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: cyclePlayerColor(index)
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: parent.radius
                                            color: model.colorHex || Theme.textDim
                                            opacity: colorMA.containsMouse ? 0.15 : 0

                                            Behavior on opacity {
                                                NumberAnimation {
                                                    duration: Theme.animFast
                                                }

                                            }

                                        }

                                        Behavior on border.width {
                                            NumberAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                    }

                                    Rectangle {
                                        readonly property real emblemSize: Math.min(playerCard.height - Theme.spacingSmall * 2, 72)
                                        property string emblemSource: root.nationEmblemFor(model.nationId)

                                        width: emblemSize
                                        height: emblemSize
                                        radius: Theme.radiusSmall
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: nationMA.containsMouse ? Qt.lighter(Theme.cardBaseB, 1.1) : Theme.cardBaseB
                                        border.color: nationMA.containsMouse ? Theme.selectedBr : Theme.thumbBr
                                        border.width: nationMA.containsMouse ? 2 : 1
                                        ToolTip.visible: nationMA.containsMouse
                                        ToolTip.text: qsTr("Nation: %1 - Click to change").arg(model.nationName || qsTr("Nation"))

                                        Image {
                                            anchors.centerIn: parent
                                            visible: parent.emblemSource !== ""
                                            source: parent.emblemSource
                                            width: parent.width * 0.8
                                            height: width
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            mipmap: true
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: parent.emblemSource === ""
                                            text: model.nationName || qsTr("Nation")
                                            color: Theme.textMain
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        MouseArea {
                                            id: nationMA

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: cyclePlayerNation(index)
                                        }

                                        Behavior on color {
                                            ColorAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                        Behavior on border.color {
                                            ColorAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                        Behavior on border.width {
                                            NumberAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                    }

                                    Rectangle {
                                        width: 70
                                        height: playerCard.height - (Theme.spacingSmall + 2) * 2 - 4
                                        radius: Theme.radiusSmall
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: teamMA.containsMouse ? Qt.lighter(Theme.hoverBg, 1.2) : Theme.hoverBg
                                        border.color: teamMA.containsMouse ? Theme.selectedBr : Theme.thumbBr
                                        border.width: teamMA.containsMouse ? 2 : 1
                                        ToolTip.visible: teamMA.containsMouse
                                        ToolTip.text: qsTr("Team %1 - Click to change").arg(model.team_id || 0)

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 2

                                            Text {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: model.teamIcon || "⚪"
                                                color: Theme.textMain
                                                font.pixelSize: 20
                                                font.bold: true
                                            }

                                            Text {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: qsTr("Team %1").arg(model.team_id || 0)
                                                color: Theme.textBright
                                                font.pixelSize: 10
                                                font.bold: true
                                            }

                                        }

                                        MouseArea {
                                            id: teamMA

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: cyclePlayerTeam(index)
                                        }

                                        Behavior on color {
                                            ColorAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                        Behavior on border.color {
                                            ColorAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                        Behavior on border.width {
                                            NumberAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                    }

                                    Rectangle {
                                        width: 36
                                        height: playerCard.height - (Theme.spacingSmall + 2) * 2 - 4
                                        radius: Theme.radiusSmall
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: removeMA.containsMouse ? Theme.removeColor : Theme.cardBaseA
                                        border.color: Theme.removeColor
                                        border.width: removeMA.containsMouse ? 2 : 1
                                        visible: !model.isHuman
                                        ToolTip.visible: removeMA.containsMouse
                                        ToolTip.text: qsTr("Remove player")

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✕"
                                            color: Theme.textMain
                                            font.pixelSize: 16
                                            font.bold: true
                                        }

                                        MouseArea {
                                            id: removeMA

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: removePlayer(index)
                                        }

                                        Behavior on color {
                                            ColorAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                        Behavior on border.width {
                                            NumberAnimation {
                                                duration: Theme.animFast
                                            }

                                        }

                                    }

                                }

                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animNormal
                                }

                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: Theme.animNormal
                                }

                            }

                            Behavior on border.width {
                                NumberAnimation {
                                    duration: Theme.animNormal
                                }

                            }

                        }

                    }

                    Rectangle {
                        width: parent.width
                        height: 8
                        color: "transparent"
                    }

                    Button {
                        text: qsTr("+ Add CPU")
                        enabled: playersModel.count < (selectedMapData && selectedMapData.player_ids ? selectedMapData.player_ids.length : 0)
                        onClicked: addCPU()
                        hoverEnabled: true
                        implicitHeight: 38
                        implicitWidth: 120
                        ToolTip.visible: addCpuHover.containsMouse && parent.enabled
                        ToolTip.text: qsTr("Add AI opponent")

                        MouseArea {
                            id: addCpuHover

                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                            cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 13
                            font.bold: true
                            color: parent.enabled ? Theme.textMain : Theme.textDim
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: Theme.radiusMedium
                            color: {
                                if (!parent.enabled)
                                    return Theme.cardBase;

                                if (parent.down)
                                    return Qt.darker(Theme.addColor, 1.2);

                                if (addCpuHover.containsMouse)
                                    return Qt.lighter(Theme.addColor, 1.2);

                                return Theme.addColor;
                            }
                            border.width: parent.enabled && addCpuHover.containsMouse ? 2 : 1
                            border.color: parent.enabled ? Qt.lighter(Theme.addColor, 1.3) : Theme.thumbBr

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animFast
                                }

                            }

                            Behavior on border.width {
                                NumberAnimation {
                                    duration: Theme.animFast
                                }

                            }

                        }

                    }

                }

            }

            Rectangle {
                id: playerSelectionPanel

                radius: Theme.radiusLarge
                color: Theme.cardBaseA
                border.color: Theme.panelBr
                border.width: 1
                visible: false
                height: playerSelectionContent.height + 20

                anchors {
                    top: playerConfigPanel.bottom
                    left: parent.left
                    right: parent.right
                    topMargin: Theme.spacingMedium
                }

                Column {
                    id: playerSelectionContent

                    spacing: Theme.spacingSmall

                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: Theme.spacingSmall
                    }

                    Text {
                        text: "Available Player Slots: " + (function() {
                            var it = selectedMapData;
                            return (it && typeof it.playerCount !== 'undefined') ? it.playerCount : 0;
                        })()
                        color: Theme.textMain
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        text: "Select your player ID:"
                        color: Theme.textSubLite
                        font.pixelSize: 12
                    }

                    Flow {
                        width: parent.width
                        spacing: Theme.spacingSmall

                        Repeater {
                            model: {
                                var it = selectedMapData;
                                return (it && it.player_ids) ? it.player_ids : [];
                            }

                            delegate: Rectangle {
                                width: 60
                                height: 32
                                radius: Theme.radiusMedium
                                color: {
                                    var pid = modelData;
                                    if (typeof game === 'undefined')
                                        return Theme.cardBaseB;

                                    return (game.selected_player_id === pid) ? Theme.selectedBg : Theme.cardBaseB;
                                }
                                border.color: {
                                    var pid = modelData;
                                    if (typeof game === 'undefined')
                                        return Theme.thumbBr;

                                    return (game.selected_player_id === pid) ? Theme.selectedBr : Theme.thumbBr;
                                }
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "ID " + modelData
                                    color: {
                                        var pid = modelData;
                                        if (typeof game === 'undefined')
                                            return Theme.textSub;

                                        return (game.selected_player_id === pid) ? Theme.textMain : Theme.textSub;
                                    }
                                    font.pixelSize: 12
                                    font.bold: {
                                        var pid = modelData;
                                        if (typeof game === 'undefined')
                                            return false;

                                        return game.selected_player_id === pid;
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof game !== 'undefined')
                                            game.selected_player_id = modelData;

                                    }
                                }

                            }

                        }

                    }

                    Text {
                        text: {
                            if (typeof game === 'undefined')
                                return "";

                            var it = selectedMapData;
                            if (!it || !it.player_ids)
                                return "";

                            var others = [];
                            for (var i = 0; i < it.player_ids.length; i++) {
                                if (it.player_ids[i] !== game.selected_player_id)
                                    others.push(it.player_ids[i]);

                            }
                            if (others.length === 0)
                                return "All other slots will be CPU-controlled";

                            return "CPU will control: ID " + others.join(", ID ");
                        }
                        color: Theme.textSubLite
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }

                }

            }

        }

        Rectangle {
            id: footer

            height: 60
            color: "transparent"

            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: Theme.spacingXLarge
                rightMargin: Theme.spacingXLarge
                bottomMargin: Theme.spacingMedium
            }

            Rectangle {
                height: 1
                color: Theme.panelBr

                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }

            }

            Text {
                id: validationErrorText

                text: validationError
                visible: validationError !== ""
                color: Theme.removeColor
                font.pixelSize: 13
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter

                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: 140
                    rightMargin: 140
                }

            }

            Button {
                text: qsTr("Back")
                onClicked: root.cancelled()
                hoverEnabled: true
                implicitHeight: 42
                implicitWidth: 120
                ToolTip.visible: backHover.containsMouse
                ToolTip.text: qsTr("Return to main menu (Esc)")

                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    topMargin: Theme.spacingSmall
                }

                MouseArea {
                    id: backHover

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    cursorShape: Qt.PointingHandCursor
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: backHover.containsMouse ? 14 : 13
                    font.bold: backHover.containsMouse
                    color: Theme.textBright
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    Behavior on font.pixelSize {
                        NumberAnimation {
                            duration: Theme.animFast
                        }

                    }

                }

                background: Rectangle {
                    radius: Theme.radiusLarge
                    color: {
                        if (parent.down)
                            return Theme.hover;

                        if (backHover.containsMouse)
                            return Theme.cardBase;

                        return Qt.rgba(0, 0, 0, 0);
                    }
                    border.width: backHover.containsMouse ? 2 : 1
                    border.color: backHover.containsMouse ? Theme.thumbBr : Theme.panelBr

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.animFast
                        }

                    }

                    Behavior on border.color {
                        ColorAnimation {
                            duration: Theme.animFast
                        }

                    }

                    Behavior on border.width {
                        NumberAnimation {
                            duration: Theme.animFast
                        }

                    }

                }

            }

            Button {
                text: qsTr("Play")
                enabled: list.currentIndex >= 0 && list.count > 0 && playersModel.count >= 2 && hasMinimumDistinctTeams()
                onClicked: acceptSelection()
                hoverEnabled: true
                implicitHeight: 42
                implicitWidth: 130
                ToolTip.visible: playHover.containsMouse
                ToolTip.text: {
                    if (validationError !== "")
                        return validationError;

                    return qsTr("Start game (Enter)");
                }

                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    topMargin: Theme.spacingSmall
                }

                MouseArea {
                    id: playHover

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: parent.enabled ? (playHover.containsMouse ? 15 : 14) : 14
                    font.bold: true
                    color: parent.enabled ? Theme.textMain : Theme.textDim
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    Behavior on font.pixelSize {
                        NumberAnimation {
                            duration: Theme.animFast
                        }

                    }

                }

                background: Rectangle {
                    radius: Theme.radiusLarge
                    color: {
                        if (!parent.enabled)
                            return Theme.cardBaseB;

                        if (parent.down)
                            return Theme.selectedBr;

                        if (playHover.containsMouse)
                            return Qt.lighter(Theme.selectedBg, 1.2);

                        return Theme.selectedBg;
                    }
                    border.width: parent.enabled && playHover.containsMouse ? 2 : 1
                    border.color: parent.enabled ? Theme.selectedBr : Theme.panelBr

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.animFast
                        }

                    }

                    Behavior on border.color {
                        ColorAnimation {
                            duration: Theme.animFast
                        }

                    }

                    Behavior on border.width {
                        NumberAnimation {
                            duration: Theme.animFast
                        }

                    }

                }

            }

        }

    }

}
