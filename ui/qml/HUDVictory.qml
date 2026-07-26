import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: victoryOverlay

    property bool showing_summary: false
    property bool manually_hidden: false

    signal return_to_main_menu_requested

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    function victory_state() {
        return game_ready() ? game.victory_state : "";
    }

    function campaign_completed() {
        return game_ready() && game.is_campaign_mission && victory_state() === "victory" && (game.campaign_completed === true);
    }

    function outcome_kind() {
        if (victory_state() === "victory")
            return campaign_completed() ? "campaign" : "victory";
        return "defeat";
    }

    function headline_text() {
        switch (outcome_kind()) {
        case "campaign":
            return qsTr("The Campaign is Won");
        case "victory":
            return qsTr("Victory Secured");
        default:
            return qsTr("Army Broken");
        }
    }

    function subtitle_text() {
        switch (outcome_kind()) {
        case "campaign":
            return qsTr("Every mission has fallen to your standard.");
        case "victory":
            return qsTr("Enemy command has fallen.");
        default:
            return qsTr("Your command has collapsed.");
        }
    }

    function reset_state() {
        showing_summary = false;
        manually_hidden = false;
    }

    function force_hide() {
        reset_state();
        manually_hidden = true;
    }

    anchors.fill: parent
    visible: !manually_hidden && victory_state() !== ""
    z: 100
    onVisibleChanged: {
        if (!visible)
            reset_state();
    }

    Connections {
        function onVictory_state_changed() {
            if (victoryOverlay.victory_state() === "")
                victoryOverlay.reset_state();
            else
                victoryOverlay.manually_hidden = false;
        }

        target: victoryOverlay.game_ready() ? game : null
    }

    Loader {
        anchors.fill: parent
        active: !victoryOverlay.showing_summary
        visible: active

        sourceComponent: Design.OutcomeLayout {
            outcome: victoryOverlay.outcome_kind()
            factionId: victoryOverlay.game_ready() ? game.local_player_nation : ""
            headline: victoryOverlay.headline_text()
            subtitle: victoryOverlay.subtitle_text()
            primaryAction: qsTr("Battle Report")
            onPrimaryActivated: {
                victoryOverlay.showing_summary = true;
                battleSummary.show();
            }
        }
    }

    BattleSummary {
        id: battleSummary

        anchors.fill: parent
        visible: victoryOverlay.showing_summary
        on_close: function () {
            victoryOverlay.showing_summary = false;
        }
        on_return_to_main_menu: function () {
            victoryOverlay.reset_state();
            victoryOverlay.return_to_main_menu_requested();
        }
    }
}
