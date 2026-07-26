import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "SelectionSummary"
    when: windowShown
    width: 400
    height: 300
    visible: true

    function makeGroups(specs) {
        var groups = [];
        for (var i = 0; i < specs.length; ++i) {
            groups.push({
                    "typeKey": specs[i].typeKey,
                    "name": specs[i].name || specs[i].typeKey,
                    "nation": specs[i].nation || "roman_republic",
                    "count": specs[i].count,
                    "woundedCount": specs[i].wounded || 0,
                    "health": specs[i].health === undefined ? 1 : specs[i].health
                });
        }
        return groups;
    }

    function makeSummary(unitCount, groups) {
        return summaryComponent.createObject(testCase, {
                "unitCount": unitCount,
                "groups": groups
            });
    }

    
    function test_no_selection_reports_empty() {
        var summary = makeSummary(0, []);
        verify(summary.empty);
        verify(!summary.singleUnit);
        verify(!summary.squad);
        verify(!summary.army);
        summary.destroy();
    }

    function test_one_unit_uses_the_detail_presentation() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "swordsman",
                        "count": 1,
                        "health": 0.5
                    }]));
        verify(summary.singleUnit);
        verify(!summary.squad);
        verify(!summary.army);
        summary.destroy();
    }

    function test_a_squad_stays_in_per_unit_chips() {
        var summary = makeSummary(6, makeGroups([{
                        "typeKey": "archer",
                        "count": 6
                    }]));
        verify(summary.squad);
        verify(!summary.army);
        summary.destroy();
    }

    
    
    function test_an_army_switches_to_the_grouped_roster() {
        var summary = makeSummary(60, makeGroups([{
                        "typeKey": "spearman",
                        "count": 40
                    }, {
                        "typeKey": "archer",
                        "count": 20
                    }]));
        verify(summary.army);
        verify(!summary.squad);
        summary.destroy();
    }

    function test_the_detail_cap_is_the_boundary_between_chips_and_roster() {
        var atCap = makeSummary(12, makeGroups([{
                        "typeKey": "archer",
                        "count": 12
                    }]));
        compare(atCap.detailCap, 12);
        verify(atCap.squad, "a selection at the cap should still use chips");
        atCap.destroy();
        var overCap = makeSummary(13, makeGroups([{
                        "typeKey": "archer",
                        "count": 13
                    }]));
        verify(overCap.army, "one unit past the cap should switch to the roster");
        overCap.destroy();
    }

    function test_health_colour_bands_are_distinct() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "archer",
                        "count": 1
                    }]));
        var healthy = summary.healthColor(0.9).toString();
        var hurt = summary.healthColor(0.5).toString();
        var critical = summary.healthColor(0.1).toString();
        verify(healthy !== hurt);
        verify(hurt !== critical);
        verify(healthy !== critical);
        summary.destroy();
    }

    function test_icons_resolve_from_the_type_key_or_the_display_name() {
        var summary = makeSummary(1, []);
        var byKey = summary.iconFor("horse_archer", "roman_republic", "");
        var byName = summary.iconFor("", "roman_republic", "Horse Archer");
        verify(byKey.toString() !== "");
        compare(byName.toString(), byKey.toString());
        summary.destroy();
    }

    Component {
        id: summaryComponent

        IronSelectionSummary {
        }
    }
}
