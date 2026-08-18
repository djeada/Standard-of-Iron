import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "CampaignOutcome"
    when: windowShown
    width: 800
    height: 600
    visible: true

    function makeOverlay(props) {
        return overlayComponent.createObject(testCase, props || {});
    }

    function test_outcome_kind_follows_the_match_state_data() {
        return [{
                "tag": "undecided",
                "victoryState": "",
                "isCampaignMission": false,
                "campaignCompleted": false,
                "kind": "defeat",
                "decided": false
            }, {
                "tag": "skirmish_win",
                "victoryState": "victory",
                "isCampaignMission": false,
                "campaignCompleted": false,
                "kind": "victory",
                "decided": true
            }, {
                "tag": "mid_campaign_win",
                "victoryState": "victory",
                "isCampaignMission": true,
                "campaignCompleted": false,
                "kind": "victory",
                "decided": true
            }, {
                "tag": "final_campaign_win",
                "victoryState": "victory",
                "isCampaignMission": true,
                "campaignCompleted": true,
                "kind": "campaign",
                "decided": true
            }, {
                "tag": "defeat",
                "victoryState": "defeat",
                "isCampaignMission": true,
                "campaignCompleted": false,
                "kind": "defeat",
                "decided": true
            }];
    }

    function test_outcome_kind_follows_the_match_state(data) {
        var overlay = makeOverlay({
                "victoryState": data.victoryState,
                "isCampaignMission": data.isCampaignMission,
                "campaignCompleted": data.campaignCompleted
            });
        compare(overlay.outcomeKind, data.kind);
        compare(overlay.decided, data.decided);
        compare(overlay.visible, data.decided, "an undecided match must not show a banner");
        overlay.destroy();
    }

    function test_a_skirmish_never_shows_the_campaign_banner() {
        var overlay = makeOverlay({
                "victoryState": "victory",
                "isCampaignMission": false,
                "campaignCompleted": true
            });
        compare(overlay.outcomeKind, "victory");
        overlay.destroy();
    }

    function test_every_outcome_names_itself() {
        var kinds = ["victory", "defeat", "campaign", "training"];
        var headlines = [];
        var subtitles = [];
        for (var i = 0; i < kinds.length; ++i) {
            var overlay = makeOverlay({
                    "victoryState": kinds[i] === "defeat" ? "defeat" : "victory",
                    "isCampaignMission": kinds[i] === "campaign",
                    "campaignCompleted": kinds[i] === "campaign",
                    "isTutorial": kinds[i] === "training"
                });
            compare(overlay.outcomeKind, kinds[i]);
            verify(overlay.headline.length > 0, kinds[i] + " has no headline");
            verify(overlay.subtitle.length > 0, kinds[i] + " has no subtitle");
            compare(headlines.indexOf(overlay.headline), -1, kinds[i] + " reuses a headline");
            compare(subtitles.indexOf(overlay.subtitle), -1, kinds[i] + " reuses a subtitle");
            headlines.push(overlay.headline);
            subtitles.push(overlay.subtitle);
            overlay.destroy();
        }
    }

    function test_a_finished_tutorial_offers_the_campaign() {
        var overlay = makeOverlay({
                "victoryState": "victory",
                "isTutorial": true
            });
        compare(overlay.outcomeKind, "training");
        verify(overlay.secondaryAction.length > 0, "a finished tutorial must say where to go next");
        var spy = spyComponent.createObject(testCase, {
                "target": overlay,
                "signalName": "secondaryRequested"
            });
        var banner = findChild(overlay, "outcomeBanner");
        banner.item.secondaryActivated();
        compare(spy.count, 1);
        verify(!overlay.showingSummary, "the campaign shortcut must not open the battle report");
        spy.destroy();
        overlay.destroy();
    }

    function test_a_lost_tutorial_is_still_a_defeat() {
        var overlay = makeOverlay({
                "victoryState": "defeat",
                "isTutorial": true
            });
        compare(overlay.outcomeKind, "defeat");
        compare(overlay.secondaryAction, "");
        overlay.destroy();
    }

    function test_the_banner_shows_until_the_report_is_asked_for() {
        var overlay = makeOverlay({
                "victoryState": "victory"
            });
        var banner = findChild(overlay, "outcomeBanner");
        var detail = findChild(overlay, "outcomeDetail");
        verify(banner !== null);
        verify(banner.visible, "the banner should be up as soon as the match is decided");
        verify(!detail.visible);
        var spy = spyComponent.createObject(testCase, {
                "target": overlay,
                "signalName": "reportRequested"
            });
        banner.item.primaryActivated();
        compare(spy.count, 1);
        verify(overlay.showingSummary);
        verify(!banner.visible, "the banner and the report must not stack");
        verify(detail.visible);
        spy.destroy();
        overlay.destroy();
    }

    function test_dismissing_the_overlay_keeps_it_dismissed() {
        var overlay = makeOverlay({
                "victoryState": "victory"
            });
        verify(overlay.visible);
        overlay.forceHide();
        verify(!overlay.visible);
        verify(overlay.manuallyHidden);
        overlay.victoryState = "victory";
        verify(!overlay.visible, "a duplicate victory signal reopened the overlay");
        overlay.destroy();
    }

    function test_a_new_outcome_revives_a_dismissed_overlay() {
        var overlay = makeOverlay({
                "victoryState": "victory"
            });
        overlay.forceHide();
        verify(!overlay.visible);
        overlay.victoryState = "defeat";
        verify(overlay.visible, "a fresh outcome must be shown even after a dismissal");
        compare(overlay.outcomeKind, "defeat");
        overlay.destroy();
    }

    function test_a_retry_starts_from_a_clean_overlay() {
        var overlay = makeOverlay({
                "victoryState": "defeat"
            });
        overlay.showingSummary = true;
        verify(overlay.visible);
        overlay.victoryState = "";
        verify(!overlay.visible, "a retry left the defeat banner up");
        verify(!overlay.showingSummary, "a retry left the battle report open");
        verify(!overlay.manuallyHidden);
        overlay.destroy();
    }

    function test_the_next_mission_does_not_inherit_the_last_report() {
        var overlay = makeOverlay({
                "victoryState": "victory",
                "isCampaignMission": true,
                "campaignCompleted": false
            });
        overlay.showingSummary = true;
        overlay.victoryState = "";
        overlay.victoryState = "victory";
        verify(!overlay.showingSummary, "the new mission opened straight into the report");
        var banner = findChild(overlay, "outcomeBanner");
        verify(banner.visible);
        overlay.destroy();
    }

    function test_the_campaign_banner_only_appears_on_the_last_win() {
        var overlay = makeOverlay({
                "isCampaignMission": true
            });
        var seen = [];
        for (var mission = 0; mission < 3; ++mission) {
            overlay.victoryState = "";
            overlay.campaignCompleted = mission === 2;
            overlay.victoryState = "victory";
            seen.push(overlay.outcomeKind);
        }
        compare(seen, ["victory", "victory", "campaign"]);
        overlay.destroy();
    }

    function test_the_detail_slot_holds_the_battle_report() {
        var overlay = Qt.createQmlObject('import QtQuick 2.15; import StandardOfIron.Design 1.0; ' + 'IronOutcomeOverlay { victoryState: "victory"; Rectangle { objectName: "report" } }', testCase);
        var detail = findChild(overlay, "outcomeDetail");
        var report = findChild(overlay, "report");
        verify(report !== null, "declared content did not land in the detail slot");
        compare(report.parent, detail);
        overlay.destroy();
    }

    Component {
        id: overlayComponent

        IronOutcomeOverlay {
        }
    }

    Component {
        id: spyComponent

        SignalSpy {
        }
    }
}
