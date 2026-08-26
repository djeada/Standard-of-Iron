import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var maps_model: (typeof game !== "undefined" && game.setup.maps) ? game.setup.maps : []
    property bool maps_loading: (typeof game !== "undefined" && game.setup.maps_loading) ? game.setup.maps_loading : false
    property int selected_map_index: -1
    property var selected_map_data: null
    property string selected_map_path: ""
    property var map_slots: []
    property string validation_error: ""
    property var available_nations: []
    property int roster_revision: 0
    property string human_nation_name: ""
    property string human_commander_name: ""
    property string human_commander_role: ""
    property string human_commander_bonus: ""
    property string human_commander_aura: ""
    property string human_commander_rally: ""

    readonly property bool has_selection: selected_map_data !== null
    property alias roster: players_model

    signal map_chosen(string map_path, var player_configs)
    signal observe_requested(string map_path)
    signal cancelled

    // Row images for the battlefield list. No map ships a thumbnail, so without
    // this every row falls back to the same generic glyph.
    MapThumbnails {
        id: map_thumbnails
    }

    onMaps_modelChanged: Qt.callLater(function () {
            if (selected_map_data !== null || !maps_model || (maps_model.length || 0) <= 0)
                return;
            list.currentIndex = 0;
            select_map(0);
        })

    function refresh_available_nations() {
        if (typeof game !== "undefined" && game.setup.nations)
            available_nations = game.setup.nations;
        else
            available_nations = [];
    }

    function default_nation_entry() {
        if (available_nations && available_nations.length > 0)
            return available_nations[0];
        return {
            "id": "roman_republic",
            "name": qsTr("Roman Republic")
        };
    }

    function commanders_for_nation(nationId) {
        if (typeof game !== "undefined" && game.setup && game.setup.commanders_for_nation)
            return variant_list_to_array(game.setup.commanders_for_nation(nationId || ""));
        return [];
    }

    function default_commander_entry(nationId) {
        let commanders = commanders_for_nation(nationId);
        if (commanders.length > 0)
            return commanders[0];
        return {
            "troop": "",
            "display_name": qsTr("Commander"),
            "battlefield_role": ""
        };
    }

    function commander_field(entry, key) {
        if (!entry)
            return "";
        return String(entry[key] || "");
    }

    function nation_entry_at(offset) {
        if (!available_nations || available_nations.length === 0)
            return default_nation_entry();
        return available_nations[offset % available_nations.length];
    }

    function refresh_commander_dossier() {
        for (let i = 0; i < players_model.count; i++) {
            let p = players_model.get(i);
            if (!p.isHuman)
                continue;
            human_nation_name = p.nationName || "";
            human_commander_name = p.commanderName || "";
            human_commander_role = p.commanderRole || "";
            human_commander_bonus = p.commanderBonus || "";
            human_commander_aura = p.commanderAura || "";
            human_commander_rally = p.commanderRally || "";
            return;
        }
        human_nation_name = "";
        human_commander_name = "";
        human_commander_role = "";
        human_commander_bonus = "";
        human_commander_aura = "";
        human_commander_rally = "";
    }

    function distinct_team_count() {
        let teams = new Set();
        for (let i = 0; i < players_model.count; i++) {
            let p = players_model.get(i);
            if (p.isEnabled)
                teams.add(p.team_id);
        }
        return teams.size;
    }

    function has_minimum_distinct_teams() {
        return enabled_player_count() >= 2 && distinct_team_count() >= 2;
    }

    function map_is_solo_playable(mapData) {
        let solo = value_for(mapData, "soloPlayable");
        return solo === true || String(solo) === "true";
    }

    function enabled_player_count() {
        let enabledCount = 0;
        for (let i = 0; i < players_model.count; i++) {
            if (players_model.get(i).isEnabled)
                enabledCount++;
        }
        return enabledCount;
    }

    function can_start() {
        let enabledCount = enabled_player_count();
        if (enabledCount < 1)
            return false;
        if (enabledCount < 2)
            return map_is_solo_playable(selected_map_data);
        return has_minimum_distinct_teams();
    }

    function update_validation_error() {
        roster_revision++;
        refresh_commander_dossier();
        let enabledCount = enabled_player_count();
        let soloPlayable = map_is_solo_playable(selected_map_data);
        if (enabledCount < 1)
            validation_error = qsTr("Need at least 1 enabled player to start");
        else if (enabledCount < 2 && !soloPlayable)
            validation_error = qsTr("Need at least 2 enabled players to start");
        else if (enabledCount >= 2 && !has_minimum_distinct_teams())
            validation_error = qsTr("At least two teams must be selected to start a match");
        else
            validation_error = "";
    }

    function field(obj, key) {
        return (obj && obj[key] !== undefined) ? String(obj[key]) : "";
    }

    function nation_emblem_for(nationId) {
        if (!nationId || !Theme.nationEmblems)
            return "";
        let emblem = Theme.nationEmblems[nationId];
        if (emblem === undefined || emblem === null)
            return "";
        return String(emblem);
    }

    function get_map_data(index) {
        if (!maps_model || index < 0 || index >= (maps_model.length || list.count))
            return null;
        let m = (maps_model.length !== undefined) ? maps_model[index] : null;
        if (m && m.get)
            return m.get(index);
        return m;
    }

    function value_for(obj, key) {
        if (!obj)
            return undefined;
        if (obj[key] !== undefined)
            return obj[key];
        if (obj.get !== undefined) {
            try {
                return obj.get(key);
            } catch (e) {
            }
        }
        return undefined;
    }

    function variant_list_to_array(value) {
        if (!value)
            return [];
        if (Array.isArray(value))
            return value;
        let result = [];
        if (typeof value.length === "number") {
            for (let i = 0; i < value.length; i++)
                result.push(value[i]);
            return result;
        }
        if (typeof value.count === "number") {
            for (let j = 0; j < value.count; j++) {
                if (value.get !== undefined)
                    result.push(value.get(j));
                else
                    result.push(value[j]);
            }
            return result;
        }
        return [];
    }

    function player_ids_for_map(mapData) {
        let ids = variant_list_to_array(value_for(mapData, "player_ids"));
        if (ids.length > 0)
            return ids.map(function (id) {
                    return Number(id);
                });
        let count = Number(value_for(mapData, "playerCount") || 0);
        for (let i = 1; i <= count; i++)
            ids.push(i);
        return ids;
    }

    function refresh_map_preview() {
        Qt.callLater(function () {
                if (map_preview && map_preview.refresh_preview) {
                    map_preview.player_configs = get_player_configs();
                    map_preview.refresh_preview();
                }
            });
    }

    function select_map(index) {
        if (index < 0 || index >= list.count) {
            selected_map_index = -1;
            selected_map_data = null;
            selected_map_path = "";
            map_slots = [];
            players_model.clear();
            update_validation_error();
            return;
        }
        selected_map_index = index;
        selected_map_data = get_map_data(index);
        selected_map_path = selected_map_data ? (selected_map_data.path || selected_map_data.file || "") : "";
        map_slots = player_ids_for_map(selected_map_data);
        initialize_players(selected_map_data);
    }

    function initialize_players(mapData) {
        players_model.clear();
        let playerIds = map_slots;
        if (!mapData || playerIds.length === 0) {
            update_validation_error();
            refresh_map_preview();
            return;
        }
        let requestedPlayerId = (typeof game !== "undefined") ? Number(game.selected_player_id) : Number(playerIds[0]);
        let humanPlayerId = playerIds.indexOf(requestedPlayerId) !== -1 ? requestedPlayerId : Number(playerIds[0]);
        let defaultNation = nation_entry_at(0);
        let defaultCommander = default_commander_entry(defaultNation.id);
        players_model.append({
                "player_id": humanPlayerId,
                "playerName": qsTr("You"),
                "colorIndex": 0,
                "colorHex": Theme.playerColors[0].hex,
                "colorName": Theme.playerColors[0].name,
                "team_id": 0,
                "teamIcon": Theme.teamIcons[1],
                "nationId": defaultNation.id,
                "nationName": defaultNation.name,
                "commanderTroop": defaultCommander.troop,
                "commanderName": defaultCommander.display_name,
                "commanderRole": commander_field(defaultCommander, "battlefield_role"),
                "commanderBonus": commander_field(defaultCommander, "bonus_summary"),
                "commanderAura": commander_field(defaultCommander, "passive_aura"),
                "commanderRally": commander_field(defaultCommander, "rally_ability"),
                "isHuman": true,
                "isEnabled": true
            });
        if (playerIds.length > 1)
            add_cpu();
        update_validation_error();
        refresh_map_preview();
    }

    function add_cpu() {
        if (!selected_map_data || map_slots.length === 0)
            return;
        if (players_model.count >= map_slots.length)
            return;
        let usedIds = [];
        for (let i = 0; i < players_model.count; i++)
            usedIds.push(players_model.get(i).player_id);
        let nextId = -1;
        for (let j = 0; j < map_slots.length; j++) {
            if (usedIds.indexOf(Number(map_slots[j])) === -1) {
                nextId = Number(map_slots[j]);
                break;
            }
        }
        if (nextId === -1)
            return;
        let usedColors = [];
        for (let k = 0; k < players_model.count; k++)
            usedColors.push(players_model.get(k).colorIndex);
        let colorIdx = 0;
        for (let c = 0; c < Theme.playerColors.length; c++) {
            if (usedColors.indexOf(c) === -1) {
                colorIdx = c;
                break;
            }
        }
        let defaultTeamId = players_model.count > 0 ? 1 : 0;
        let defaultNation = nation_entry_at(players_model.count);
        let defaultCommander = default_commander_entry(defaultNation.id);
        players_model.append({
                "player_id": nextId,
                "playerName": qsTr("CPU %1").arg(Design.Numerals.roman(nextId)),
                "colorIndex": colorIdx,
                "colorHex": Theme.playerColors[colorIdx].hex,
                "colorName": Theme.playerColors[colorIdx].name,
                "team_id": defaultTeamId,
                "teamIcon": Theme.teamIcons[defaultTeamId + 1],
                "nationId": defaultNation.id,
                "nationName": defaultNation.name,
                "commanderTroop": defaultCommander.troop,
                "commanderName": defaultCommander.display_name,
                "commanderRole": commander_field(defaultCommander, "battlefield_role"),
                "commanderBonus": commander_field(defaultCommander, "bonus_summary"),
                "commanderAura": commander_field(defaultCommander, "passive_aura"),
                "commanderRally": commander_field(defaultCommander, "rally_ability"),
                "isHuman": false,
                "isEnabled": true
            });
        update_validation_error();
        refresh_map_preview();
    }

    function remove_player(index) {
        if (index < 0 || index >= players_model.count)
            return;
        let p = players_model.get(index);
        if (p.isHuman)
            return;
        players_model.remove(index);
        update_validation_error();
        refresh_map_preview();
    }

    function cycle_player_color(index) {
        if (index < 0 || index >= players_model.count)
            return;
        let p = players_model.get(index);
        let usedColors = [];
        for (let i = 0; i < players_model.count; i++) {
            if (i !== index)
                usedColors.push(players_model.get(i).colorIndex);
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
        players_model.setProperty(index, "colorIndex", newIdx);
        players_model.setProperty(index, "colorHex", Theme.playerColors[newIdx].hex);
        players_model.setProperty(index, "colorName", Theme.playerColors[newIdx].name);
        refresh_map_preview();
    }

    function cycle_player_team(index) {
        if (index < 0 || index >= players_model.count)
            return;
        let p = players_model.get(index);
        let teamCount = Math.max(2, Math.min(8, players_model.count));
        let newTeamId = (p.team_id + 1) % teamCount;
        players_model.setProperty(index, "team_id", newTeamId);
        players_model.setProperty(index, "teamIcon", Theme.teamIcons[newTeamId + 1]);
        update_validation_error();
        refresh_map_preview();
    }

    function cycle_player_nation(index) {
        if (index < 0 || index >= players_model.count)
            return;
        if (!available_nations || available_nations.length === 0)
            return;
        let p = players_model.get(index);
        let currentId = p.nationId || available_nations[0].id;
        let nextIndex = 0;
        for (let i = 0; i < available_nations.length; i++) {
            if (available_nations[i].id === currentId) {
                nextIndex = (i + 1) % available_nations.length;
                break;
            }
        }
        let nextNation = available_nations[nextIndex];
        let nextCommander = default_commander_entry(nextNation.id);
        players_model.setProperty(index, "nationId", nextNation.id);
        players_model.setProperty(index, "nationName", nextNation.name);
        apply_commander(index, nextCommander);
        refresh_map_preview();
    }

    function apply_commander(index, entry) {
        players_model.setProperty(index, "commanderTroop", commander_field(entry, "troop"));
        players_model.setProperty(index, "commanderName", commander_field(entry, "display_name"));
        players_model.setProperty(index, "commanderRole", commander_field(entry, "battlefield_role"));
        players_model.setProperty(index, "commanderBonus", commander_field(entry, "bonus_summary"));
        players_model.setProperty(index, "commanderAura", commander_field(entry, "passive_aura"));
        players_model.setProperty(index, "commanderRally", commander_field(entry, "rally_ability"));
        refresh_commander_dossier();
    }

    function cycle_player_commander(index) {
        if (index < 0 || index >= players_model.count)
            return;
        let p = players_model.get(index);
        let commanders = commanders_for_nation(p.nationId);
        if (commanders.length === 0)
            return;
        let currentTroop = p.commanderTroop || commanders[0].troop;
        let nextIndex = 0;
        for (let i = 0; i < commanders.length; i++) {
            if (commanders[i].troop === currentTroop) {
                nextIndex = (i + 1) % commanders.length;
                break;
            }
        }
        apply_commander(index, commanders[nextIndex]);
        refresh_map_preview();
    }

    function cycle_human_slot() {
        if (map_slots.length < 2 || players_model.count === 0)
            return;
        let humanIndex = -1;
        for (let i = 0; i < players_model.count; i++) {
            if (players_model.get(i).isHuman) {
                humanIndex = i;
                break;
            }
        }
        if (humanIndex < 0)
            return;
        let humanId = players_model.get(humanIndex).player_id;
        let slotIndex = map_slots.indexOf(humanId);
        let nextId = Number(map_slots[(slotIndex + 1) % map_slots.length]);
        if (nextId === humanId)
            return;
        let occupantIndex = -1;
        for (let j = 0; j < players_model.count; j++) {
            if (players_model.get(j).player_id === nextId) {
                occupantIndex = j;
                break;
            }
        }
        if (occupantIndex >= 0) {
            players_model.setProperty(occupantIndex, "player_id", humanId);
            players_model.setProperty(occupantIndex, "playerName", qsTr("CPU %1").arg(Design.Numerals.roman(humanId)));
        }
        players_model.setProperty(humanIndex, "player_id", nextId);
        if (typeof game !== "undefined")
            game.selected_player_id = nextId;
        update_validation_error();
        refresh_map_preview();
    }

    function toggle_player_enabled(index) {
        if (index < 0 || index >= players_model.count)
            return;
        let p = players_model.get(index);
        players_model.setProperty(index, "isEnabled", !p.isEnabled);
        update_validation_error();
        refresh_map_preview();
    }

    function get_player_configs() {
        let configs = [];
        for (let i = 0; i < players_model.count; i++) {
            let p = players_model.get(i);
            if (!p.isEnabled)
                continue;
            configs.push({
                    "player_id": p.player_id,
                    "colorHex": p.colorHex,
                    "team_id": p.team_id,
                    "nationId": p.nationId,
                    "commanderTroop": p.commanderTroop,
                    "isHuman": p.isHuman
                });
        }
        return configs;
    }

    function accept_selection() {
        if (selected_map_index < 0 || !selected_map_path)
            return;
        if (!can_start()) {
            update_validation_error();
            return;
        }
        validation_error = "";
        root.map_chosen(selected_map_path, get_player_configs());
    }

    function can_observe() {
        return root.has_selection && map_slots.length >= 2;
    }

    function observe_selection() {
        if (!can_observe())
            return;
        validation_error = "";
        root.observe_requested(selected_map_path);
    }

    Component.onCompleted: refresh_available_nations()
    anchors.fill: parent
    focus: true
    onVisibleChanged: {
        if (visible) {
            root.focus = true;
            select_map(-1);
            refresh_available_nations();
            if (typeof game !== "undefined" && game.setup.start_loading_maps)
                game.setup.start_loading_maps();
        }
    }
    Keys.onPressed: function (event) {
        if (!visible)
            return;
        if (event.key === Qt.Key_Escape) {
            root.cancelled();
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            accept_selection();
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
        id: players_model
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.dim
    }

    Rectangle {
        id: container

        width: Math.min(parent.width * 0.975, 1660)
        height: Math.min(parent.height * 0.975, 1040)
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#2b2118"
            }

            GradientStop {
                position: 1
                color: "#1a140f"
            }
        }
        border.color: "#8f6d43"
        border.width: 2
        opacity: 0.98
        clip: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            color: "transparent"
            border.color: "#4a3722"
            border.width: 1
            radius: Math.max(2, container.radius - 4)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny

                    Label {
                        text: qsTr("Skirmish")
                        color: Theme.textMain
                        font.pixelSize: Design.Typography.hero
                        font.bold: true
                        font.family: "serif"
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Pick a battlefield, then set the colours, nations and commanders that take the field.")
                        color: Theme.textSubLite
                        font.pixelSize: Design.Typography.bodyLarge
                        font.family: "serif"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        Layout.fillWidth: true
                    }
                }

                StyledButton {
                    text: qsTr("← Back")
                    onClicked: root.cancelled()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingLarge

                Rectangle {
                    id: maps_panel

                    Layout.preferredWidth: Math.max(340, container.width * 0.31)
                    Layout.fillHeight: true
                    radius: Theme.radiusMedium
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#3a2f23"
                        }

                        GradientStop {
                            position: 1
                            color: "#241b14"
                        }
                    }
                    border.color: "#a7814a"
                    border.width: 2

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMedium
                        spacing: Theme.spacingSmall

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSmall

                            Label {
                                text: qsTr("Battlefields")
                                color: Theme.textMain
                                font.pixelSize: Design.Typography.heading
                                font.bold: true
                                font.family: "serif"
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Design.IronBadge {
                                text: Design.Numerals.roman(list.count)
                                visible: list.count > 0
                            }
                        }

                        Rectangle {
                            id: list_frame

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#241c14"
                            radius: Theme.radiusSmall
                            border.color: "#8f6d43"
                            border.width: 1
                            clip: true

                            ListView {
                                id: list

                                anchors.fill: parent
                                anchors.margins: Theme.spacingSmall
                                model: maps_model
                                spacing: Theme.spacingSmall
                                currentIndex: (count > 0 ? 0 : -1)
                                keyNavigationWraps: false
                                boundsBehavior: Flickable.StopAtBounds
                                visible: !maps_loading && count > 0
                                onCurrentIndexChanged: select_map(currentIndex)

                                ScrollBar.vertical: ScrollBar {
                                    policy: list.contentHeight > list.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                                }

                                delegate: Rectangle {
                                    id: map_card

                                    readonly property bool current: index === list.currentIndex
                                    readonly property int slot_count: Number((typeof playerCount !== "undefined") ? playerCount : ((modelData && modelData.playerCount !== undefined) ? modelData.playerCount : 0))

                                    width: list.width - (list.ScrollBar.vertical.visible ? Theme.spacingMedium : 0)
                                    height: 76
                                    radius: Theme.radiusSmall
                                    clip: true
                                    color: map_card.current ? Theme.selectedBg : (map_mouse.containsMouse ? Theme.hoverBg : "#2c231a")
                                    border.width: map_card.current ? 2 : 1
                                    border.color: map_card.current ? Theme.selectedBr : (map_mouse.containsMouse ? Theme.accentBr : Theme.thumbBr)

                                    Rectangle {
                                        id: thumb_wrap

                                        width: 80
                                        height: 56
                                        radius: Theme.radiusSmall
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
                                            id: thumb_image

                                            anchors.fill: parent
                                            source: map_thumbnails.source_for((typeof thumbnail !== "undefined") ? thumbnail : "", (typeof path !== "undefined") ? String(path) : "")
                                            asynchronous: true
                                            fillMode: Image.PreserveAspectCrop
                                            visible: status === Image.Ready
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: !thumb_image.visible
                                            text: Design.Icons.map
                                            font.pixelSize: Design.Typography.glyphSmall
                                            color: map_card.current ? Theme.accentBright : Theme.textDim
                                        }
                                    }

                                    Item {
                                        anchors {
                                            left: thumb_wrap.right
                                            right: parent.right
                                            top: parent.top
                                            bottom: parent.bottom
                                            leftMargin: Theme.spacingSmall
                                            rightMargin: Theme.spacingSmall
                                            topMargin: Theme.spacingSmall
                                            bottomMargin: Theme.spacingSmall
                                        }

                                        Text {
                                            id: map_name

                                            text: (typeof name !== "undefined") ? String(name) : (typeof modelData === "string" ? modelData : (modelData && modelData.name ? String(modelData.name) : ""))
                                            color: map_card.current ? Theme.textMain : Theme.textBright
                                            font.pixelSize: Design.Typography.body
                                            font.bold: true
                                            font.family: "serif"
                                            elide: Text.ElideRight

                                            anchors {
                                                top: parent.top
                                                left: parent.left
                                                right: slot_badge.left
                                                rightMargin: Theme.spacingSmall
                                            }
                                        }

                                        Design.IronBadge {
                                            id: slot_badge

                                            text: Design.Icons.population + " " + Design.Numerals.roman(map_card.slot_count)
                                            tone: map_card.current ? Theme.accentBright : Theme.textSub
                                            visible: map_card.slot_count > 0

                                            anchors {
                                                top: parent.top
                                                right: parent.right
                                            }
                                        }

                                        Text {
                                            text: (typeof description !== "undefined") ? String(description) : (modelData && modelData.description ? String(modelData.description) : "")
                                            color: map_card.current ? Theme.textSubLite : Theme.textSub
                                            font.pixelSize: Design.Typography.caption
                                            wrapMode: Text.WordWrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight

                                            anchors {
                                                left: parent.left
                                                right: parent.right
                                                top: map_name.bottom
                                                bottom: parent.bottom
                                                topMargin: Theme.spacingTiny
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: map_mouse

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            Design.UiSound.activate();
                                            list.currentIndex = index;
                                        }
                                        onDoubleClicked: accept_selection()
                                        onContainsMouseChanged: {
                                            if (containsMouse)
                                                Design.UiSound.hover();
                                        }
                                    }

                                    Behavior on color  {
                                        ColorAnimation {
                                            duration: Theme.animNormal
                                        }
                                    }

                                    Behavior on border.color  {
                                        ColorAnimation {
                                            duration: Theme.animNormal
                                        }
                                    }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: Theme.spacingSmall
                                visible: maps_loading

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: Design.Icons.autoGather
                                    font.pixelSize: Design.Typography.glyph
                                    color: Theme.accent

                                    RotationAnimator on rotation  {
                                        from: 0
                                        to: 360
                                        duration: 1500
                                        loops: Animation.Infinite
                                        running: maps_loading
                                    }
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("Loading maps...")
                                    color: Theme.textSubLite
                                    font.pixelSize: Design.Typography.label
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !maps_loading && list.count === 0
                                text: qsTr("No maps available")
                                color: Theme.textSub
                                font.pixelSize: Design.Typography.label
                            }
                        }
                    }
                }

                ColumnLayout {
                    id: right_column

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacingMedium

                    Rectangle {
                        id: briefing_panel

                        Layout.fillWidth: true
                        Layout.preferredHeight: 236
                        radius: Theme.radiusMedium
                        gradient: Gradient {
                            GradientStop {
                                position: 0
                                color: "#3a2f23"
                            }

                            GradientStop {
                                position: 1
                                color: "#241b14"
                            }
                        }
                        border.color: "#a7814a"
                        border.width: 2

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMedium
                            spacing: Theme.spacingMedium
                            visible: root.has_selection

                            MapPreview {
                                id: map_preview

                                Layout.preferredWidth: briefing_panel.height - Theme.spacingMedium * 2
                                Layout.fillHeight: true
                                map_path: selected_map_path
                                player_configs: get_player_configs()
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: Theme.spacingSmall

                                Label {
                                    text: field(selected_map_data, "name")
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.heading
                                    font.bold: true
                                    font.family: "serif"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: field(selected_map_data, "description")
                                    color: Theme.textSubLite
                                    font.pixelSize: Design.Typography.label
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 4
                                    elide: Text.ElideRight
                                    lineHeight: 1.25
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                }

                                Row {
                                    spacing: Theme.spacingSmall
                                    Layout.fillWidth: true

                                    SkirmishChip {
                                        width: 92
                                        height: implicitHeight
                                        caption: qsTr("Slots")
                                        value: Design.Numerals.roman(map_slots.length)
                                        tooltip_text: qsTr("Player slots this battlefield was authored for")
                                    }

                                    SkirmishChip {
                                        width: 92
                                        height: implicitHeight
                                        caption: qsTr("In play")
                                        value: Design.Numerals.roman(roster_revision >= 0 ? enabled_player_count() : 0)
                                        outline: validation_error === "" ? Theme.thumbBr : Theme.removeColor
                                        tooltip_text: qsTr("Players that will take the field")
                                    }

                                    SkirmishChip {
                                        readonly property int sides: roster_revision >= 0 ? distinct_team_count() : 0

                                        width: 92
                                        height: implicitHeight
                                        caption: qsTr("Sides")
                                        value: Design.Numerals.roman(sides)
                                        outline: (sides >= 2 || map_is_solo_playable(selected_map_data)) ? Theme.thumbBr : Theme.removeColor
                                        tooltip_text: qsTr("Distinct teams among the players in play")
                                    }

                                    SkirmishChip {
                                        width: 132
                                        height: implicitHeight
                                        caption: qsTr("Opposition")
                                        value: map_is_solo_playable(selected_map_data) ? qsTr("Scripted") : qsTr("Players only")
                                        tooltip_text: map_is_solo_playable(selected_map_data) ? qsTr("This battlefield brings its own enemies, so you can start alone") : qsTr("This battlefield needs at least two opposing players")
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: Theme.spacingSmall
                            visible: !root.has_selection

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: Design.Icons.map
                                font.pixelSize: Design.Typography.glyph
                                color: Theme.textDim
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: maps_loading ? qsTr("Loading maps...") : qsTr("Select a battlefield to continue")
                                color: Theme.textHint
                                font.pixelSize: Design.Typography.label
                                font.italic: true
                            }
                        }
                    }

                    Rectangle {
                        id: roster_panel

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: Theme.radiusMedium
                        gradient: Gradient {
                            GradientStop {
                                position: 0
                                color: "#3a2f23"
                            }

                            GradientStop {
                                position: 1
                                color: "#241b14"
                            }
                        }
                        border.color: "#a7814a"
                        border.width: 2
                        visible: root.has_selection

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMedium
                            spacing: Theme.spacingSmall

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Label {
                                    text: qsTr("Order of Battle")
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.heading
                                    font.bold: true
                                    font.family: "serif"
                                }

                                Label {
                                    text: qsTr("Click any chip to change it")
                                    color: Theme.textSubLite
                                    font.pixelSize: Design.Typography.caption
                                    font.italic: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                StyledButton {
                                    id: add_cpu_button

                                    readonly property bool allowed: players_model.count < map_slots.length

                                    text: qsTr("+ Add CPU")
                                    button_style: "small"
                                    blocked: !allowed
                                    disabledReason: qsTr("Every slot on this battlefield is taken")
                                    ToolTip.visible: hovered && allowed
                                    ToolTip.text: qsTr("Add an AI opponent")
                                    ToolTip.delay: Design.Metrics.tooltipDelay
                                    onClicked: add_cpu()
                                }
                            }

                            ListView {
                                id: players_list

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: players_model
                                spacing: Theme.spacingSmall
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds

                                ScrollBar.vertical: ScrollBar {
                                    policy: players_list.contentHeight > players_list.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                                }

                                delegate: Rectangle {
                                    id: player_card

                                    objectName: "rosterSeatCard"
                                    width: players_list.width - (players_list.ScrollBar.vertical.visible ? Theme.spacingMedium : 0)
                                    height: 68
                                    radius: Theme.radiusSmall
                                    color: model.isHuman ? "#33261a" : "#2c231a"
                                    border.color: model.isEnabled ? (model.isHuman ? Theme.accent : Theme.thumbBr) : Theme.thumbBr
                                    border.width: (model.isHuman && model.isEnabled) ? 2 : 1
                                    opacity: model.isEnabled ? 1 : 0.55
                                    enabled: true

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: Theme.spacingSmall
                                        spacing: Theme.spacingSmall

                                        Design.IronCheckBox {
                                            checked: model.isEnabled
                                            ToolTip.visible: hovered
                                            ToolTip.text: model.isEnabled ? qsTr("Leave this slot empty") : qsTr("Bring this slot into the battle")
                                            ToolTip.delay: Design.Metrics.tooltipDelay
                                            onToggled: toggle_player_enabled(index)
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 8
                                            Layout.preferredHeight: player_card.height - Theme.spacingMedium
                                            radius: 4
                                            color: model.colorHex || Theme.textDim
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 96
                                            spacing: 0

                                            Label {
                                                text: model.playerName || ""
                                                color: model.isHuman ? Theme.accentBright : Theme.textBright
                                                font.pixelSize: Design.Typography.body
                                                font.bold: true
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Label {
                                                text: model.isHuman ? qsTr("Slot %1 • click to move seat").arg(Design.Numerals.roman(model.player_id)) : qsTr("Slot %1 • AI").arg(Design.Numerals.roman(model.player_id))
                                                color: Theme.textSubLite
                                                font.pixelSize: Design.Typography.caption
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true

                                                MouseArea {
                                                    anchors.fill: parent
                                                    enabled: (model.isHuman === true) && map_slots.length > 1
                                                    hoverEnabled: enabled
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        Design.UiSound.toggle();
                                                        cycle_human_slot();
                                                    }
                                                }
                                            }
                                        }

                                        SkirmishChip {
                                            Layout.preferredWidth: 104
                                            caption: qsTr("Colour")
                                            value: model.colorName || qsTr("Colour")
                                            value_color: model.colorHex || Theme.textMain
                                            outline: model.colorHex || Theme.thumbBr
                                            interactive: true
                                            tooltip_text: qsTr("Player colour — click to change")
                                            onActivated: cycle_player_color(index)
                                        }

                                        SkirmishChip {
                                            Layout.preferredWidth: 192
                                            caption: qsTr("Nation")
                                            value: model.nationName || qsTr("Nation")
                                            emblem_source: root.nation_emblem_for(model.nationId)
                                            interactive: true
                                            tooltip_text: qsTr("Nation — click to change")
                                            onActivated: cycle_player_nation(index)
                                        }

                                        SkirmishChip {
                                            Layout.preferredWidth: 190
                                            caption: qsTr("Commander")
                                            value: model.commanderName || qsTr("Commander")
                                            interactive: true
                                            tooltip_text: model.commanderRole !== "" ? qsTr("%1 — click to change commander").arg(model.commanderRole) : qsTr("Commander — click to change")
                                            onActivated: cycle_player_commander(index)
                                        }

                                        SkirmishChip {
                                            Layout.preferredWidth: 92
                                            caption: model.teamIcon || Theme.teamIcons[1]
                                            value: qsTr("Team %1").arg(Design.Numerals.roman(model.team_id + 1))
                                            interactive: true
                                            tooltip_text: qsTr("Team — click to change sides")
                                            onActivated: cycle_player_team(index)
                                        }

                                        Design.IronIconButton {
                                            iconText: Design.Icons.close
                                            tooltip: qsTr("Remove this opponent")
                                            visible: !model.isHuman
                                            tone: "destructive"
                                            onClicked: remove_player(index)
                                        }
                                    }

                                    Behavior on opacity  {
                                        NumberAnimation {
                                            duration: Theme.animFast
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: dossier

                                Layout.fillWidth: true
                                Layout.preferredHeight: dossier_column.implicitHeight + Theme.spacingMedium * 2
                                radius: Theme.radiusSmall
                                color: "#241c14"
                                border.color: "#8f6d43"
                                border.width: 1
                                visible: human_commander_name !== ""

                                ColumnLayout {
                                    id: dossier_column

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: Theme.spacingMedium
                                    spacing: Theme.spacingTiny

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingSmall

                                        Label {
                                            text: Design.Icons.commander + " " + human_commander_name
                                            color: Theme.accentBright
                                            font.pixelSize: Design.Typography.body
                                            font.bold: true
                                            font.family: "serif"
                                        }

                                        Design.IronBadge {
                                            text: human_nation_name
                                            tone: Theme.textSub
                                            visible: human_nation_name !== ""
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Label {
                                        text: human_commander_role
                                        visible: human_commander_role !== ""
                                        color: Theme.textSub
                                        font.pixelSize: Design.Typography.caption
                                        font.italic: true
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: human_commander_bonus
                                        visible: human_commander_bonus !== ""
                                        color: Theme.textSubLite
                                        font.pixelSize: Design.Typography.caption
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: human_commander_aura !== "" ? Design.Icons.aura + " " + human_commander_aura : ""
                                        visible: human_commander_aura !== ""
                                        color: Theme.textSub
                                        font.pixelSize: Design.Typography.caption
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: human_commander_rally !== "" ? Design.Icons.rally + " " + human_commander_rally : ""
                                        visible: human_commander_rally !== ""
                                        color: Theme.textSub
                                        font.pixelSize: Design.Typography.caption
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.panelBr
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMedium

                Row {
                    spacing: Theme.spacingSmall

                    Design.IronHotkeyLabel {
                        text: qsTr("↑ ↓ Choose")
                    }

                    Design.IronHotkeyLabel {
                        text: qsTr("Enter Start")
                    }

                    Design.IronHotkeyLabel {
                        text: qsTr("Esc Back")
                    }
                }

                Label {
                    text: validation_error
                    visible: validation_error !== "" && root.has_selection
                    color: Theme.removeColor
                    font.pixelSize: Design.Typography.label
                    font.bold: true
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Item {
                    Layout.fillWidth: true
                    visible: validation_error === "" || !root.has_selection
                }

                StyledButton {
                    id: observe_button

                    objectName: "observeButton"
                    readonly property bool allowed: root.can_observe()

                    text: qsTr("Observe")
                    button_style: "secondary"
                    implicitWidth: Design.Metrics.space24 * 6
                    implicitHeight: Design.Metrics.controlHeight + Design.Metrics.space8
                    blocked: !allowed
                    disabledReason: qsTr("Pick a battlefield with at least two camps to watch")
                    ToolTip.visible: hovered && allowed
                    ToolTip.text: qsTr("Watch the computer fight itself on this battlefield")
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    onClicked: observe_selection()
                }

                StyledButton {
                    id: play_button

                    readonly property bool allowed: root.has_selection && players_model.count > 0 && validation_error === ""

                    text: qsTr("Play ▶")
                    button_style: "primary"
                    implicitWidth: Design.Metrics.space24 * 8
                    implicitHeight: Design.Metrics.controlHeight + Design.Metrics.space8
                    blocked: !allowed
                    disabledReason: validation_error !== "" ? validation_error : qsTr("Select a battlefield to continue")
                    ToolTip.visible: hovered && allowed
                    ToolTip.text: qsTr("Start the battle (Enter)")
                    ToolTip.delay: Design.Metrics.tooltipDelay
                    onClicked: accept_selection()
                }
            }
        }
    }
}
