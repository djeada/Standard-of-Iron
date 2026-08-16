import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Rectangle {
    id: root

    property var mission_data: null
    property string campaign_id: ""
    property var mission_definition: null
    property var terrain_colors: ({
            "mountain": "#8b7355",
            "plains": "#9db68f",
            "forest": "#4a7c59",
            "river": "#4a90b5",
            "desert": "#d4a574",
            "hills": "#a89968"
        })
    property string player_faction: resolve_player_faction()
    property bool is_player_carthaginian: player_faction.indexOf("carth") !== -1
    property string command_glyph: is_player_carthaginian ? StyleGuide.historical.carthageGlyph : StyleGuide.historical.romanGlyph
    property string commander_title: is_player_carthaginian ? qsTr("Suffete Command") : qsTr("Consular Command")
    property string command_banner_text: is_player_carthaginian ? qsTr("Carthaginian High Command") : qsTr("Roman High Command")
    readonly property int base_casualty_forecast: 420
    readonly property int casualty_per_difficulty_step: 95
    readonly property int tactical_rating_min: 1
    readonly property int tactical_rating_max: 5
    property string tactical_rating: calculate_tactical_rating()
    property string casualty_forecast: calculate_casualty_forecast()
    property string success_estimate: calculate_success_estimate()
    property string reward_summary: reward_summary_text()
    property var player_commander: mission_definition && mission_definition.player_setup ? mission_definition.player_setup.commander : null
    property var opposing_forces: mission_definition && mission_definition.ai_setups ? mission_definition.ai_setups : []

    readonly property var speed_options: GameSpeeds.options

    signal start_mission_clicked

    function speed_label(speed) {
        return GameSpeeds.label(speed);
    }

    function speed_shortcut(action_id) {
        var shortcut = InputBindings.shortcut_for(action_id);
        return shortcut.length > 0 ? InputBindings.describe(shortcut) : qsTr("its shortcut");
    }

    function terrain_label(key) {
        switch (key) {
        case "apulian_river_plain":
            return qsTr("Apulian river plain");
        case "campanian_plain":
            return qsTr("Campanian plain");
        case "lakeside_hill_country":
            return qsTr("Lakeside hill country");
        case "mountain":
            return qsTr("Mountain");
        case "north_african_plain":
            return qsTr("North African plain");
        case "po_valley_river_plain":
            return qsTr("Po valley river plain");
        case "river_valley":
            return qsTr("River valley");
        case "winter_river_plain":
            return qsTr("Winter river plain");
        }
        return Design.Icons.humanise(key);
    }

    function resolve_player_faction() {
        if (mission_definition && mission_definition.player_setup) {
            var player_setup = mission_definition.player_setup;
            if (player_setup.faction)
                return player_setup.faction.toLowerCase();
            if (player_setup.nation)
                return player_setup.nation.toLowerCase();
        }
        if (campaign_id && campaign_id.toLowerCase().indexOf("carth") !== -1)
            return "carthaginian";
        return "roman";
    }

    function resolve_setup_faction(setup) {
        if (!setup)
            return "";
        if (setup.faction)
            return setup.faction.toLowerCase();
        if (setup.nation)
            return setup.nation.toLowerCase();
        return "";
    }

    function command_glyph_for_setup(setup) {
        return resolve_setup_faction(setup).indexOf("carth") !== -1 ? StyleGuide["historical"]["carthageGlyph"] : StyleGuide["historical"]["romanGlyph"];
    }

    function command_banner_for_setup(setup) {
        return resolve_setup_faction(setup).indexOf("carth") !== -1 ? qsTr("Carthaginian High Command") : qsTr("Roman High Command");
    }

    function setup_summary(setup) {
        if (!setup)
            return "";
        var parts = [];
        if (setup.faction)
            parts.push(titleize(setup.faction));
        else if (setup.nation)
            parts.push(titleize(setup.nation));
        if (setup.difficulty)
            parts.push(titleize(setup.difficulty));
        return parts.join(" • ");
    }

    function load_mission_definition() {
        if (mission_data && mission_data.mission_id && typeof game !== "undefined" && game.setup.mission_definition)
            mission_definition = game.setup.mission_definition(mission_data.mission_id);
    }

    function titleize(value) {
        if (!value)
            return "";
        var parts = value.split("_");
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i];
            if (part.length === 0)
                continue;
            parts[i] = part.charAt(0).toUpperCase() + part.slice(1);
        }
        return parts.join(" ");
    }

    function objectives_empty() {
        var victory_count = mission_definition && mission_definition.victory_conditions ? mission_definition.victory_conditions.length : 0;
        var optional_count = mission_definition && mission_definition.optional_objectives ? mission_definition.optional_objectives.length : 0;
        var defeat_count = mission_definition && mission_definition.defeat_conditions ? mission_definition.defeat_conditions.length : 0;
        return (victory_count + optional_count + defeat_count) === 0;
    }

    function calculate_tactical_rating() {
        if (!mission_data || !mission_data.difficulty_modifier)
            return Design.Numerals.roman(3) + "/" + Design.Numerals.roman(tactical_rating_max);
        return Design.Numerals.roman(Math.min(tactical_rating_max, Math.max(tactical_rating_min, Math.round(mission_data.difficulty_modifier)))) + "/" + Design.Numerals.roman(tactical_rating_max);
    }

    function calculate_casualty_forecast() {
        if (!mission_data || !mission_data.difficulty_modifier)
            return Design.Numerals.roman(610);
        return Design.Numerals.roman(base_casualty_forecast + mission_data.difficulty_modifier * casualty_per_difficulty_step);
    }

    function calculate_success_estimate() {
        if (!mission_data)
            return qsTr("Unknown");
        if (mission_data.completed)
            return Design.Numerals.percent(100);
        if (mission_data.unlocked === false)
            return qsTr("Sealed");
        return qsTr("In Progress");
    }

    function reward_summary_text() {
        if (!mission_data || !mission_data.unlocked)
            return qsTr("Rewards: Sealed until prior victories");
        if (mission_data.completed)
            return qsTr("Rewards: Laurels Inscribed • Veteran Honors Claimed");
        return qsTr("Rewards: Bronze Standard • Veteran Cohort");
    }

    onMission_dataChanged: load_mission_definition()
    radius: Theme.radiusMedium
    gradient: Gradient {
        GradientStop {
            position: 0
            color: "#3a2d22"
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

        ScrollView {
            id: mission_content_scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.minimumWidth: 0
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Item {
                id: mission_content_container

                width: mission_content_scroll.availableWidth > 0 ? mission_content_scroll.availableWidth : mission_content_scroll.width
                implicitWidth: width
                implicitHeight: mission_content_column.implicitHeight

                ColumnLayout {
                    id: mission_content_column

                    width: parent.width
                    spacing: Theme.spacingMedium

                    Label {
                        text: {
                            if (mission_definition && mission_definition.title)
                                return mission_definition.title;
                            return mission_data && mission_data.mission_id ? titleize(mission_data.mission_id) : "";
                        }
                        color: Theme.textMain
                        font.pixelSize: Design.Typography.heading
                        font.bold: true
                        font.family: "serif"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: Theme.spacingSmall

                        Rectangle {
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: wax_seal_text.implicitWidth + Theme.spacingMedium
                            radius: 12
                            color: "#7a1f1d"
                            border.color: "#c29555"
                            border.width: 1

                            Label {
                                id: wax_seal_text

                                anchors.centerIn: parent
                                text: command_glyph
                                color: "#f2dfba"
                                font.pixelSize: Design.Typography.label
                                font.bold: true
                            }
                        }

                        Rectangle {
                            visible: !!(mission_definition && mission_definition.terrain_type)
                            Layout.preferredWidth: terrain_badge_layout.implicitWidth + 12
                            Layout.preferredHeight: 24
                            radius: Theme.radiusSmall
                            color: {
                                if (!mission_definition || !mission_definition.terrain_type)
                                    return Theme.disabledBg;
                                return terrain_colors[mission_definition.terrain_type] || Theme.disabledBg;
                            }
                            border.color: Theme.border
                            border.width: 1
                            opacity: 0.85

                            RowLayout {
                                id: terrain_badge_layout

                                anchors.centerIn: parent
                                spacing: 4

                                Label {
                                    text: {
                                        if (!mission_definition || !mission_definition.terrain_type)
                                            return "";
                                        return Design.Icons.terrain(mission_definition.terrain_type);
                                    }
                                    font.pixelSize: Design.Typography.body
                                }

                                Label {
                                    text: {
                                        if (!mission_definition || !mission_definition.terrain_type)
                                            return "";
                                        return terrain_label(mission_definition.terrain_type);
                                    }
                                    color: "#ffffff"
                                    font.pixelSize: Design.Typography.label
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Label {
                        visible: !!(mission_data && mission_data.intro_text)
                        text: qsTr("Dispatch: ") + (mission_data && mission_data.intro_text ? mission_data.intro_text : "")
                        color: Theme.textSubLite
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        font.pixelSize: Design.Typography.bodyLarge
                        font.family: "serif"
                    }

                    Rectangle {
                        visible: !!(mission_definition && mission_definition.historical_context)
                        Layout.fillWidth: true
                        implicitHeight: historical_context_text.implicitHeight + Theme.spacingSmall * 2
                        Layout.preferredHeight: implicitHeight
                        radius: Theme.radiusSmall
                        color: "#2f241a"
                        border.color: "#8f6d43"
                        border.width: 1
                        opacity: 0.7

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSmall
                            spacing: Theme.spacingSmall

                            Label {
                                text: Design.Icons.briefing
                                font.pixelSize: Design.Typography.bodyLarge
                                Layout.alignment: Qt.AlignTop
                            }

                            Label {
                                id: historical_context_text

                                text: mission_definition && mission_definition.historical_context ? mission_definition.historical_context : ""
                                color: Theme.textSubLite
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                font.pixelSize: Design.Typography.label
                                font.italic: true
                                font.family: "serif"
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny

                        Label {
                            text: {
                                var count = mission_definition && mission_definition.victory_conditions ? mission_definition.victory_conditions.length : 0;
                                if (count > 1)
                                    return mission_definition.victory_mode === "all" ? qsTr("Campaign Objectives (complete all):") : qsTr("Campaign Objectives (complete any):");
                                return qsTr("Campaign Objectives:");
                            }
                            color: Theme.textDim
                            font.pixelSize: Design.Typography.body
                            font.bold: true
                        }

                        Repeater {
                            model: {
                                if (!mission_definition || !mission_definition.victory_conditions)
                                    return [];
                                return mission_definition.victory_conditions;
                            }

                            delegate: Label {
                                text: "• " + (modelData.description || qsTr("Complete mission objective"))
                                color: Theme.textSubLite
                                font.pixelSize: Design.Typography.body
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            visible: !!(mission_definition && mission_definition.optional_objectives && mission_definition.optional_objectives.length > 0)
                            text: qsTr("Optional:")
                            color: Theme.textDim
                            font.pixelSize: Design.Typography.label
                            font.bold: true
                        }

                        Repeater {
                            model: {
                                if (!mission_definition || !mission_definition.optional_objectives)
                                    return [];
                                return mission_definition.optional_objectives;
                            }

                            delegate: Label {
                                text: "• " + (modelData.description || qsTr("Complete optional objective"))
                                color: Theme.textSubLite
                                font.pixelSize: Design.Typography.body
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            visible: !!(mission_definition && mission_definition.defeat_conditions && mission_definition.defeat_conditions.length > 0)
                            text: qsTr("Failure:")
                            color: Theme.textDim
                            font.pixelSize: Design.Typography.label
                            font.bold: true
                        }

                        Repeater {
                            model: {
                                if (!mission_definition || !mission_definition.defeat_conditions)
                                    return [];
                                return mission_definition.defeat_conditions;
                            }

                            delegate: Label {
                                text: "• " + (modelData.description || qsTr("Avoid mission failure"))
                                color: Theme.textSubLite
                                font.pixelSize: Design.Typography.body
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            visible: !mission_definition || objectives_empty()
                            text: qsTr("• Complete the mission successfully")
                            color: Theme.textSubLite
                            font.pixelSize: Design.Typography.body
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny
                        visible: !!(mission_definition && mission_definition.player_setup && mission_definition.player_setup.starting_units)

                        Label {
                            text: qsTr("Army Composition:")
                            color: Theme.textDim
                            font.pixelSize: Design.Typography.body
                            font.bold: true
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.spacingTiny

                            Repeater {
                                model: {
                                    if (!mission_definition || !mission_definition.player_setup || !mission_definition.player_setup.starting_units)
                                        return [];
                                    var units = mission_definition.player_setup.starting_units;
                                    var grouped = {};
                                    for (var i = 0; i < units.length; i++) {
                                        var unit = units[i];
                                        var type = unit.type || "unknown";
                                        grouped[type] = (grouped[type] || 0) + (unit.count || 1);
                                    }
                                    var result = [];
                                    for (var key in grouped) {
                                        result.push({
                                                "type": key,
                                                "count": grouped[key]
                                            });
                                    }
                                    return result;
                                }

                                delegate: Rectangle {
                                    width: unit_label.implicitWidth + 12
                                    height: 22
                                    radius: Theme.radiusSmall
                                    color: Theme.infoBg
                                    border.color: Theme.infoBr
                                    border.width: 1

                                    Label {
                                        id: unit_label

                                        anchors.centerIn: parent
                                        text: Design.Numerals.roman(modelData.count) + "x " + titleize(modelData.type)
                                        color: Theme.infoText
                                        font.pixelSize: Design.Typography.label
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingTiny

                        Label {
                            text: qsTr("Historical Ledger:")
                            color: Theme.textDim
                            font.pixelSize: Design.Typography.body
                            font.bold: true
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSmall

                            Repeater {
                                model: [{
                                        "text": qsTr("Casualties Forecast: ") + casualty_forecast
                                    }, {
                                        "text": qsTr("Tactical Rating: ") + tactical_rating
                                    }, {
                                        "text": qsTr("Campaign Success: ") + success_estimate
                                    }]

                                delegate: Rectangle {
                                    width: stat_label.implicitWidth + 16
                                    height: 24
                                    radius: Theme.radiusSmall
                                    color: "#2f241a"
                                    border.color: "#8f6d43"
                                    border.width: 1

                                    Label {
                                        id: stat_label

                                        anchors.centerIn: parent
                                        text: modelData.text
                                        color: Theme.textSubLite
                                        font.pixelSize: Design.Typography.label
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.minimumWidth: 248
            Layout.preferredWidth: 248
            Layout.maximumWidth: 248
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.alignment: Qt.AlignTop
            spacing: Theme.spacingSmall

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Item {
                    width: parent.width
                    implicitHeight: command_column.implicitHeight

                    ColumnLayout {
                        id: command_column

                        width: parent.width
                        spacing: Theme.spacingSmall

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: command_panel_layout.implicitHeight + Theme.spacingMedium * 2
                            Layout.preferredHeight: implicitHeight
                            radius: Theme.radiusSmall
                            color: "#281e16"
                            border.color: "#8f6d43"
                            border.width: 1

                            ColumnLayout {
                                id: command_panel_layout

                                anchors.fill: parent
                                anchors.margins: Theme.spacingMedium
                                spacing: Theme.spacingSmall

                                Label {
                                    text: commander_title
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.body
                                    font.bold: true
                                    font.family: "serif"
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    text: root.player_commander && root.player_commander.display_name ? root.player_commander.display_name : root.command_banner_text
                                    color: Theme.textMain
                                    font.pixelSize: Design.Typography.bodyLarge
                                    font.bold: true
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    text: root.player_commander && root.player_commander.battlefield_role ? root.player_commander.battlefield_role : root.commander_title
                                    color: Theme.textDim
                                    font.pixelSize: Design.Typography.label
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                Label {
                                    visible: !!(root.player_commander && root.player_commander.bonus_summary)
                                    text: root.player_commander && root.player_commander.bonus_summary ? root.player_commander.bonus_summary : ""
                                    color: Theme.textSubLite
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Design.Typography.label
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.reward_summary
                                    color: Theme.textSubLite
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: Design.Typography.label
                                    Layout.fillWidth: true
                                }

                                Label {
                                    visible: root.opposing_forces.length > 0
                                    text: qsTr("Opposition Command")
                                    color: Theme.textDim
                                    font.pixelSize: Design.Typography.body
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Repeater {
                                    model: root.opposing_forces

                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: opposition_force_layout.implicitHeight + Theme.spacingSmall * 2
                                        Layout.preferredHeight: implicitHeight
                                        radius: Theme.radiusSmall
                                        color: "#2f241a"
                                        border.color: "#8f6d43"
                                        border.width: 1

                                        ColumnLayout {
                                            id: opposition_force_layout

                                            anchors.fill: parent
                                            anchors.margins: Theme.spacingSmall
                                            spacing: Theme.spacingTiny

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.spacingSmall

                                                Label {
                                                    text: root.command_glyph_for_setup(modelData)
                                                    color: "#c29555"
                                                    font.pixelSize: Design.Typography.body
                                                    font.bold: true
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: modelData.commander && modelData.commander.display_name ? modelData.commander.display_name : qsTr("Field Command Pending")
                                                    color: Theme.textMain
                                                    font.pixelSize: Design.Typography.body
                                                    font.bold: true
                                                    wrapMode: Text.WordWrap
                                                }
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.setup_summary(modelData)
                                                color: Theme.textDim
                                                font.pixelSize: Design.Typography.label
                                                wrapMode: Text.WordWrap
                                                visible: text.length > 0
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: modelData.commander && modelData.commander.battlefield_role ? modelData.commander.battlefield_role : root.command_banner_for_setup(modelData)
                                                color: Theme.textSubLite
                                                font.pixelSize: Design.Typography.label
                                                wrapMode: Text.WordWrap
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: modelData.commander && modelData.commander.bonus_summary ? modelData.commander.bonus_summary : qsTr("No named commander assigned to this force.")
                                                color: Theme.textSubLite
                                                font.pixelSize: Design.Typography.label
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: readiness_text.implicitHeight + Theme.spacingSmall * 2
                Layout.preferredHeight: implicitHeight
                radius: Theme.radiusSmall
                color: "#2f241a"
                border.color: "#8f6d43"
                border.width: 1

                Label {
                    id: readiness_text

                    anchors.fill: parent
                    anchors.margins: Theme.spacingSmall
                    text: mission_data && !mission_data.unlocked ? qsTr("Orders sealed until prior victories are secured.") : (mission_data && mission_data.completed ? qsTr("Victory recorded. You may replay this engagement at will.") : qsTr("Orders issued. Deploy when ready."))
                    color: Theme.textSubLite
                    wrapMode: Text.WordWrap
                    font.pixelSize: Design.Typography.label
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle {
                visible: !!(mission_data && mission_data.unlocked)
                Layout.fillWidth: true
                implicitHeight: tempo_row.implicitHeight + Theme.spacingSmall * 2
                Layout.preferredHeight: implicitHeight
                radius: Theme.radiusSmall
                color: "#2f241a"
                border.color: "#8f6d43"
                border.width: 1

                RowLayout {
                    id: tempo_row

                    anchors.fill: parent
                    anchors.margins: Theme.spacingSmall
                    spacing: Theme.spacingSmall

                    Label {
                        text: Design.Icons.objective
                        font.pixelSize: Design.Typography.bodyLarge
                        Layout.alignment: Qt.AlignTop
                    }

                    Label {
                        text: qsTr("Battle tempo: the speed buttons sit on the top bar beside pause, from %1 up to %2. Press %3 or %4 to change speed without leaving the field.").arg(root.speed_label(root.speed_options[0])).arg(root.speed_label(root.speed_options[root.speed_options.length - 1])).arg(root.speed_shortcut("rts.speed_down")).arg(root.speed_shortcut("rts.speed_up"))
                        color: Theme.textSubLite
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        font.pixelSize: Design.Typography.label
                    }
                }
            }

            StyledButton {
                Layout.fillWidth: true
                text: mission_data && mission_data.completed ? qsTr("Replay Mission") : qsTr("Start Mission")
                blocked: !(mission_data && mission_data.unlocked)
                onClicked: root.start_mission_clicked()
                ToolTip.visible: !interactive && hovered
                ToolTip.text: {
                    if (!mission_data)
                        return "";
                    if (!mission_data.unlocked)
                        return qsTr("Complete previous missions to unlock");
                    return "";
                }
                ToolTip.delay: 500
            }

            Label {
                visible: !!(mission_data && !mission_data.unlocked)
                text: Design.Icons.locked + " " + qsTr("Locked")
                color: Theme.textDim
                font.pixelSize: Design.Typography.body
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                visible: !!(mission_data && mission_data.completed)
                text: qsTr("✓ Completed")
                color: Theme.successText
                font.pixelSize: Design.Typography.body
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
