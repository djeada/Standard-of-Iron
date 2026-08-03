import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

// The end-of-mission overlay, bound to the running match.
// Everything about which outcome is showing and how the banner behaves lives in
// the design-system control; this file only feeds it the engine's state and
// hands it the battle report to display.
Design.IronOutcomeOverlay {
    id: victoryOverlay

    signal return_to_main_menu_requested

    function game_ready() {
        return typeof game !== 'undefined' && game !== null;
    }

    function victory_state() {
        return game_ready() ? game.victory_state : "";
    }

    function reset_state() {
        victoryOverlay.reset();
    }

    function force_hide() {
        victoryOverlay.forceHide();
    }

    victoryState: victoryOverlay.victory_state()
    isCampaignMission: victoryOverlay.game_ready() && game.is_campaign_mission
    campaignCompleted: victoryOverlay.game_ready() && game.campaign_completed === true
    factionId: victoryOverlay.game_ready() ? game.local_player_nation : ""

    onReportRequested: battleSummary.show()

    Connections {
        function onVictory_state_changed() {
            victoryOverlay.onOutcomeChanged();
        }

        target: victoryOverlay.game_ready() ? game : null
    }

    BattleSummary {
        id: battleSummary

        anchors.fill: parent
        on_close: function () {
            victoryOverlay.showingSummary = false;
        }
        on_return_to_main_menu: function () {
            victoryOverlay.reset();
            victoryOverlay.return_to_main_menu_requested();
        }
    }
}
