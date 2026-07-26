import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "CampaignFlow"
    when: windowShown
    width: 800
    height: 600
    visible: true

    
    function test_objective_state_is_carried_by_marker_not_only_colour_data() {
        return [{
                "tag": "active",
                "objectiveState": "active",
                "marker": "◇"
            }, {
                "tag": "complete",
                "objectiveState": "complete",
                "marker": "✓"
            }, {
                "tag": "failed",
                "objectiveState": "failed",
                "marker": "✕"
            }, {
                "tag": "optional",
                "objectiveState": "optional",
                "marker": Icons.objective
            }];
    }

    function test_objective_state_is_carried_by_marker_not_only_colour(data) {
        var row = objectiveComponent.createObject(testCase, {
                "objectiveState": data.objectiveState,
                "objectiveText": "Capture the barracks"
            });
        compare(row.marker, data.marker);
        row.destroy();
    }

    function test_objective_tones_are_distinct_per_state() {
        var states = ["active", "complete", "failed", "optional"];
        var seen = [];
        for (var i = 0; i < states.length; ++i) {
            var row = objectiveComponent.createObject(testCase, {
                    "objectiveState": states[i]
                });
            var tone = row.tone.toString();
            compare(seen.indexOf(tone), -1, states[i] + " reuses another state's colour");
            seen.push(tone);
            row.destroy();
        }
    }

    function test_objective_exposes_its_text_and_state_to_assistive_tech() {
        var row = objectiveComponent.createObject(testCase, {
                "objectiveState": "complete",
                "objectiveText": "Hold the ford"
            });
        compare(row.Accessible.name, "Hold the ford");
        compare(row.Accessible.description, "complete");
        row.destroy();
    }

    
    function test_briefing_reports_when_there_is_nothing_to_brief() {
        var briefing = briefingComponent.createObject(testCase, {});
        verify(!briefing.hasObjectives);
        briefing.destroy();
    }

    function test_briefing_counts_objectives_from_every_section() {
        var briefing = briefingComponent.createObject(testCase, {
                "victoryConditions": [{
                        "description": "Destroy the enemy commander"
                    }]
            });
        verify(briefing.hasObjectives);
        briefing.destroy();
        var defeatOnly = briefingComponent.createObject(testCase, {
                "defeatConditions": [{
                        "description": "Lose your commander"
                    }]
            });
        verify(defeatOnly.hasObjectives);
        defeatOnly.destroy();
        var optionalOnly = briefingComponent.createObject(testCase, {
                "optionalObjectives": [{
                        "description": "Take no losses"
                    }]
            });
        verify(optionalOnly.hasObjectives);
        optionalOnly.destroy();
    }

    function test_briefing_renders_with_a_full_mission_payload() {
        var briefing = briefingComponent.createObject(testCase, {
                "title": "The Ford at Trebia",
                "summary": "Hold the crossing until dusk.",
                "factionId": "carthage",
                "victoryConditions": [{
                        "description": "Hold the ford"
                    }, {
                        "description": "Survive until dusk",
                        "progress": 0.4
                    }],
                "defeatConditions": [{
                        "description": "Lose the commander"
                    }],
                "optionalObjectives": [{
                        "description": "Take no losses"
                    }]
            });
        verify(briefing !== null);
        verify(briefing.hasObjectives);
        briefing.destroy();
    }

    
    function test_outcome_kinds_are_visually_distinct_data() {
        return [{
                "tag": "victory",
                "outcome": "victory",
                "triumphant": true
            }, {
                "tag": "defeat",
                "outcome": "defeat",
                "triumphant": false
            }, {
                "tag": "campaign",
                "outcome": "campaign",
                "triumphant": true
            }];
    }

    function test_outcome_kinds_are_visually_distinct(data) {
        var screen = outcomeComponent.createObject(testCase, {
                "outcome": data.outcome,
                "headline": "Headline",
                "subtitle": "Subtitle"
            });
        compare(screen.triumphant, data.triumphant);
        verify(screen.crest.length > 0);
        screen.destroy();
    }

    function test_victory_and_defeat_never_share_a_tone() {
        var victory = outcomeComponent.createObject(testCase, {
                "outcome": "victory"
            });
        var defeat = outcomeComponent.createObject(testCase, {
                "outcome": "defeat"
            });
        verify(victory.tone.toString() !== defeat.tone.toString());
        verify(victory.crest !== defeat.crest);
        victory.destroy();
        defeat.destroy();
    }

    
    function test_campaign_completion_uses_the_faction_crest() {
        var screen = outcomeComponent.createObject(testCase, {
                "outcome": "campaign",
                "factionId": "roman_republic"
            });
        compare(screen.crest, FactionTheme.glyphFor("roman_republic"));
        compare(screen.tone.toString(), FactionTheme.accentFor("roman_republic").toString());
        screen.destroy();
    }

    function test_outcome_announces_itself_to_assistive_tech() {
        var screen = outcomeComponent.createObject(testCase, {
                "outcome": "defeat",
                "headline": "Army Broken",
                "subtitle": "Your command has collapsed."
            });
        compare(screen.Accessible.name, "Army Broken");
        compare(screen.Accessible.description, "Your command has collapsed.");
        screen.destroy();
    }

    function test_outcome_actions_are_emitted() {
        var screen = outcomeComponent.createObject(testCase, {
                "outcome": "victory",
                "secondaryAction": "Return to Menu"
            });
        var primary = signalSpyComponent.createObject(testCase, {
                "target": screen,
                "signalName": "primaryActivated"
            });
        screen.primaryActivated();
        compare(primary.count, 1);
        primary.destroy();
        screen.destroy();
    }

    Component {
        id: objectiveComponent

        IronObjectiveRow {
        }
    }

    Component {
        id: briefingComponent

        BriefingLayout {
        }
    }

    Component {
        id: outcomeComponent

        OutcomeLayout {
        }
    }

    Component {
        id: signalSpyComponent

        SignalSpy {
        }
    }
}
