import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Design.IronOutcomeOverlay {
    id: victoryOverlay

    signal return_to_main_menu_requested
    signal campaign_requested

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
    outcomeReason: victoryOverlay.game_ready() ? game.defeat_reason : ""
    isTutorial: victoryOverlay.game_ready() && !!game.tutorial && game.tutorial.finished
    isCampaignMission: victoryOverlay.game_ready() && game.setup.is_mission_match
    campaignCompleted: victoryOverlay.game_ready() && game.setup.campaign_completed === true
    factionId: victoryOverlay.game_ready() ? game.local_player_nation : ""

    onReportRequested: battleSummary.show()
    onSecondaryRequested: {
        victoryOverlay.reset();
        victoryOverlay.campaign_requested();
    }

    Connections {
        function onVictory_state_changed() {
            victoryOverlay.onOutcomeChanged();
            battleSummary.reset_data();
            if (victoryOverlay.victory_state() !== "")
                battleSummary.prepare();
        }

        target: victoryOverlay.game_ready() ? game : null
    }

    BattleSummary {
        id: battleSummary

        anchors.fill: parent
        outcome: victoryOverlay.outcomeKind
        factionId: victoryOverlay.factionId
        headline: victoryOverlay.headline
        subtitle: victoryOverlay.subtitle
        onClosed: {
            victoryOverlay.showingSummary = false;
        }
        onReturn_to_main_menu_requested: {
            victoryOverlay.reset();
            victoryOverlay.return_to_main_menu_requested();
        }
    }
}
