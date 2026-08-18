import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

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

    held: victoryOverlay.game_ready() && game.commander_message && game.commander_message.holds_outcome
    victoryState: victoryOverlay.victory_state()
    isCampaignMission: victoryOverlay.game_ready() && game.setup.is_campaign_mission
    campaignCompleted: victoryOverlay.game_ready() && game.setup.campaign_completed === true
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
