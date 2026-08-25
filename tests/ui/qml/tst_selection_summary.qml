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
                    "soldiers": specs[i].soldiers === undefined ? 0 : specs[i].soldiers,
                    "maxSoldiers": specs[i].maxSoldiers === undefined ? 0 : specs[i].maxSoldiers,
                    "health": specs[i].health === undefined ? 1 : specs[i].health,
                    "stamina": specs[i].stamina === undefined ? 1 : specs[i].stamina,
                    "canRun": specs[i].canRun === undefined ? true : specs[i].canRun
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

    function test_a_selection_that_churns_never_empties_the_bars() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "swordsman",
                        "count": 1,
                        "health": 0.8,
                        "stamina": 0.6
                    }]));
        var churn = [{
                "count": 2,
                "groups": makeGroups([{
                            "typeKey": "swordsman",
                            "count": 1,
                            "health": 0.8,
                            "stamina": 0.6
                        }, {
                            "typeKey": "archer",
                            "count": 1,
                            "health": 0.4,
                            "stamina": 0.9
                        }])
            }, {
                "count": 1,
                "groups": makeGroups([{
                            "typeKey": "swordsman",
                            "count": 1,
                            "health": 0.7,
                            "stamina": 0.5
                        }])
            }, {
                "count": 2,
                "groups": makeGroups([{
                            "typeKey": "swordsman",
                            "count": 1,
                            "health": 0.7,
                            "stamina": 0.5
                        }, {
                            "typeKey": "archer",
                            "count": 1,
                            "health": 0.2,
                            "stamina": 0.8
                        }])
            }, {
                "count": 1,
                "groups": makeGroups([{
                            "typeKey": "swordsman",
                            "count": 1,
                            "health": 0.6,
                            "stamina": 0.4
                        }])
            }];
        for (var i = 0; i < churn.length; ++i) {
            summary.groups = churn[i].groups;
            summary.unitCount = churn[i].count;
            var healthBar = findChild(summary, "selectionHealthBar");
            if (healthBar !== null) {
                verify(healthBar.value !== undefined, "health bar value went undefined at step " + i);
                verify(healthBar.value > 0.0, "health bar collapsed to empty at step " + i + " while a unit was still selected");
            }
            var staminaBar = findChild(summary, "selectionStaminaBar");
            if (staminaBar !== null) {
                verify(staminaBar.value !== undefined, "stamina bar value went undefined at step " + i);
                verify(staminaBar.value > 0.0, "stamina bar collapsed to empty at step " + i + " while a unit was still selected");
            }
        }
        summary.destroy();
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

    function test_a_single_type_squad_stays_in_per_unit_chips() {
        var summary = makeSummary(6, makeGroups([{
                        "typeKey": "archer",
                        "count": 6
                    }]));
        verify(summary.squad);
        verify(!summary.groupedSquad);
        verify(!summary.army);
        summary.destroy();
    }

    function test_a_mixed_squad_switches_to_group_cards() {
        var summary = makeSummary(9, makeGroups([{
                        "typeKey": "spearman",
                        "count": 2
                    }, {
                        "typeKey": "archer",
                        "count": 3
                    }, {
                        "typeKey": "healer",
                        "count": 1
                    }, {
                        "typeKey": "swordsman",
                        "count": 3
                    }]));
        verify(summary.squad);
        verify(summary.groupedSquad);
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

    function test_single_unit_health_bar_reads_health_not_stamina() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "swordsman",
                        "count": 1,
                        "health": 0.5,
                        "stamina": 0.2
                    }]));
        var healthBar = findChild(summary, "selectionHealthBar");
        var staminaBar = findChild(summary, "selectionStaminaBar");
        verify(healthBar !== null, "single-unit view should expose a health bar");
        verify(staminaBar !== null, "single-unit view should expose a stamina bar");
        compare(findChild(summary, "selectionHealthLabel").text, "HEALTH");
        compare(findChild(summary, "selectionStaminaLabel").text, "STAMINA");
        fuzzyCompare(healthBar.value, 0.5, 0.001, "health bar must show health, not stamina");
        compare(healthBar.fillColor.toString(), summary.healthColor(0.5).toString());
        fuzzyCompare(staminaBar.value, 0.2, 0.001, "stamina bar must show stamina");
        verify(healthBar.height > staminaBar.height, "health bar should be the primary, taller bar");
        summary.destroy();
    }

    function test_stamina_is_omitted_for_units_without_run_stamina() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "catapult",
                        "count": 1,
                        "health": 0.65,
                        "stamina": 0.1,
                        "canRun": false
                    }]));
        var healthBar = findChild(summary, "selectionHealthBar");
        var staminaBar = findChild(summary, "selectionStaminaBar");
        var staminaSection = findChild(summary, "selectionStaminaSection");
        fuzzyCompare(healthBar.value, 0.65, 0.001);
        verify(staminaBar !== null);
        verify(staminaSection !== null);
        verify(!staminaSection.visible, "units that cannot run should not show an irrelevant stamina meter");
        compare(findChild(summary, "selectionHealthLabel").text, "HEALTH");
        summary.destroy();
    }

    function test_stamina_changes_do_not_touch_the_health_bar() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "swordsman",
                        "count": 1,
                        "health": 0.75,
                        "stamina": 0.9
                    }]));
        var healthBar = findChild(summary, "selectionHealthBar");
        var before = healthBar.value;
        summary.groups = makeGroups([{
                    "typeKey": "swordsman",
                    "count": 1,
                    "health": 0.75,
                    "stamina": 0.1
                }]);
        compare(healthBar.value, before, "movement-driven stamina changes must not move the health bar");
        compare(healthBar.fillColor.toString(), summary.healthColor(0.75).toString());
        summary.groups = makeGroups([{
                    "typeKey": "swordsman",
                    "count": 1,
                    "health": 0.1,
                    "stamina": 0.1
                }]);
        verify(healthBar.value < before, "real damage should lower the health bar");
        compare(healthBar.fillColor.toString(), summary.healthColor(0.1).toString());
        summary.destroy();
    }

    function test_roster_group_health_bars_read_health_not_stamina() {
        var summary = makeSummary(60, makeGroups([{
                        "typeKey": "spearman",
                        "count": 40,
                        "health": 0.9,
                        "stamina": 0.1
                    }, {
                        "typeKey": "archer",
                        "count": 20,
                        "health": 0.4,
                        "stamina": 0.9
                    }]));
        verify(summary.army);
        var spearBar = findChild(summary, "selectionGroupHealthBar_spearman");
        var archerBar = findChild(summary, "selectionGroupHealthBar_archer");
        verify(spearBar !== null, "roster should expose a health bar per group");
        verify(archerBar !== null);
        fuzzyCompare(spearBar.value, 0.9, 0.001, "group health bar must read health, not stamina");
        fuzzyCompare(archerBar.value, 0.4, 0.001);
        summary.groups = makeGroups([{
                    "typeKey": "spearman",
                    "count": 40,
                    "health": 0.9,
                    "stamina": 0.9
                }, {
                    "typeKey": "archer",
                    "count": 20,
                    "health": 0.4,
                    "stamina": 0.1
                }]);
        spearBar = findChild(summary, "selectionGroupHealthBar_spearman");
        archerBar = findChild(summary, "selectionGroupHealthBar_archer");
        fuzzyCompare(spearBar.value, 0.9, 0.001, "stamina changes while marching must not move roster health bars");
        fuzzyCompare(archerBar.value, 0.4, 0.001);
        summary.destroy();
    }

    function test_full_health_unit_shows_a_full_health_bar() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "swordsman",
                        "count": 1,
                        "health": 1,
                        "stamina": 0.3
                    }]));
        var healthBar = findChild(summary, "selectionHealthBar");
        fuzzyCompare(healthBar.value, 1, 0.001, "a healthy unit must show a full health bar");
        compare(healthBar.fillColor.toString(), summary.healthColor(1).toString());
        var staminaBar = findChild(summary, "selectionStaminaBar");
        fuzzyCompare(staminaBar.value, 0.3, 0.001, "fatigued but healthy: stamina may drop while health stays full");
        summary.destroy();
    }

    function test_casualties_lower_the_group_health_bar_only() {
        var summary = makeSummary(60, makeGroups([{
                        "typeKey": "spearman",
                        "count": 40,
                        "health": 0.9,
                        "stamina": 0.1
                    }]));
        var spearBar = findChild(summary, "selectionGroupHealthBar_spearman");
        fuzzyCompare(spearBar.value, 0.9, 0.001);
        summary.groups = makeGroups([{
                    "typeKey": "spearman",
                    "count": 39,
                    "health": 0.6,
                    "stamina": 0.1
                }]);
        spearBar = findChild(summary, "selectionGroupHealthBar_spearman");
        verify(spearBar !== null);
        fuzzyCompare(spearBar.value, 0.6, 0.001, "losing soldiers must lower the group health bar");
        summary.destroy();
    }

    function test_a_single_unit_reads_its_surviving_soldiers_out_of_its_roster() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "archer",
                        "count": 1,
                        "health": 0.5,
                        "soldiers": 15,
                        "maxSoldiers": 30
                    }]));
        compare(findChild(summary, "selectionHealthLabel").text, "SOLDIERS");
        compare(findChild(summary, "selectionHealthValue").text, "15 / 30", "a mauled unit must say how many men it has left");
        summary.groups = makeGroups([{
                    "typeKey": "archer",
                    "count": 1,
                    "health": 0.2,
                    "soldiers": 6,
                    "maxSoldiers": 30
                }]);
        compare(findChild(summary, "selectionHealthValue").text, "6 / 30", "casualties must move the readout");
        summary.destroy();
    }

    function test_a_one_body_unit_keeps_the_percentage_readout() {
        var summary = makeSummary(1, makeGroups([{
                        "typeKey": "catapult",
                        "count": 1,
                        "health": 0.65,
                        "soldiers": 1,
                        "maxSoldiers": 1,
                        "canRun": false
                    }]));
        compare(findChild(summary, "selectionHealthLabel").text, "HEALTH");
        compare(findChild(summary, "selectionHealthValue").text, "65%", "a single body has no roster to count out of");
        summary.destroy();
    }

    function test_roster_cards_read_the_men_left_in_each_group() {
        var summary = makeSummary(60, makeGroups([{
                        "typeKey": "spearman",
                        "count": 40,
                        "health": 0.9,
                        "wounded": 12,
                        "soldiers": 648,
                        "maxSoldiers": 720
                    }, {
                        "typeKey": "archer",
                        "count": 20,
                        "health": 0.4,
                        "soldiers": 240,
                        "maxSoldiers": 600
                    }]));
        verify(summary.army);
        compare(findChild(summary, "selectionGroupStrength_spearman").text, "648/720");
        compare(findChild(summary, "selectionGroupStrength_archer").text, "240/600");
        compare(summary.soldierCount, 888);
        compare(summary.soldierMax, 1320);
        compare(findChild(summary, "selectionSubtitle").text, "888 soldiers ready", "the force header must count men, not unit cards");
        summary.destroy();
    }

    function test_a_selection_without_soldier_counts_still_reads_cleanly() {
        var summary = makeSummary(60, makeGroups([{
                        "typeKey": "spearman",
                        "count": 40,
                        "health": 0.9,
                        "wounded": 12
                    }]));
        var strength = findChild(summary, "selectionGroupStrength_spearman");
        compare(strength.text, "12 wounded", "with no roster data the card falls back to the wounded line");
        compare(findChild(summary, "selectionSubtitle").text, "60 soldiers ready");
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

    function makeFocus(overrides) {
        var info = {
            "valid": true,
            "id": 42,
            "name": "Enemy Archer",
            "nation": "carthage",
            "typeKey": "archer",
            "ownerId": 2,
            "isBuilding": false,
            "isEnemy": true,
            "isOwn": false,
            "health": 30,
            "maxHealth": 100,
            "healthRatio": 0.3,
            "activity": "attack",
            "activityState": "active",
            "attackedBySelection": 0,
            "attackedByLocal": 0,
            "attackersIncoming": 0
        };
        for (var key in overrides)
            info[key] = overrides[key];
        return info;
    }

    function test_clicking_an_enemy_shows_an_inspect_card_with_its_health() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 0,
                "groups": [],
                "inspected": makeFocus({
                        "attackedByLocal": 3
                    })
            });
        verify(summary.inspecting, "an inspected enemy with no own selection is the inspect state");
        var card = findChild(summary, "selectionInspectCard");
        verify(card !== null, "inspect view should be loaded");
        var bar = findChild(summary, "inspectHealthBar");
        verify(bar !== null);
        fuzzyCompare(bar.value, 0.3, 0.001, "inspect health bar must show the enemy ratio");
        compare(bar.fillColor.toString(), Theme.danger.toString(), "enemy health reads in the danger colour regardless of ratio");
        compare(findChild(summary, "inspectHealthValue").text, "30 / 100");
        verify(!findChild(summary, "inspectSoldiersValue").visible, "an enemy with no roster data must not print a soldier line");
        compare(summary.inspectHeader(), "ENEMY UNIT");
        verify(findChild(summary, "inspectAttackSummary").text.indexOf("3") >= 0, "the card says how many of your units are attacking it");
        summary.destroy();
    }

    function test_inspecting_a_formation_shows_the_men_it_has_left() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 0,
                "groups": [],
                "inspected": makeFocus({
                        "soldiers": 9,
                        "maxSoldiers": 30
                    })
            });
        var soldiers = findChild(summary, "inspectSoldiersValue");
        verify(soldiers.visible);
        compare(soldiers.text, "9 / 30 soldiers");
        summary.destroy();
    }

    function test_own_building_inspect_uses_the_friendly_header_and_health_bands() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 0,
                "groups": [],
                "inspected": makeFocus({
                        "name": "Barracks",
                        "typeKey": "barracks",
                        "isBuilding": true,
                        "isEnemy": false,
                        "isOwn": true,
                        "healthRatio": 0.8,
                        "health": 800,
                        "maxHealth": 1000
                    })
            });
        compare(summary.inspectHeader(), "YOUR BUILDING");
        var bar = findChild(summary, "inspectHealthBar");
        compare(bar.fillColor.toString(), summary.healthColor(0.8).toString());
        summary.destroy();
    }

    function test_inspecting_is_dropped_when_own_units_are_selected() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 1,
                "groups": makeGroups([{
                            "typeKey": "swordsman",
                            "count": 1
                        }]),
                "inspected": makeFocus({})
            });
        verify(!summary.inspecting, "own selection takes precedence over an inspect card");
        verify(findChild(summary, "selectionInspectCard") === null);
        summary.destroy();
    }

    function test_invalid_focus_objects_are_ignored() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 0,
                "groups": [],
                "inspected": makeFocus({
                        "valid": false
                    })
            });
        verify(!summary.inspecting);
        verify(summary.empty);
        summary.destroy();
    }

    function spearmanLookup(typeKey, nation) {
        if (typeKey !== "spearman")
            return {
                "valid": false
            };
        return {
            "valid": true,
            "attack_damage": 19,
            "attack_range": 2.5,
            "speed": 2.5
        };
    }

    function test_a_unit_with_a_profile_offers_the_details_button() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 1,
                "groups": makeGroups([{
                            "typeKey": "spearman",
                            "count": 1
                        }]),
                "profileLookup": testCase.spearmanLookup
            });
        verify(summary.canShowProfile, "a unit the readout knows must offer its details");
        compare(summary.focusTypeKey, "spearman");
        var button = findChild(summary, "selectionProfileButton");
        verify(button !== null, "the details button is missing");
        verify(button.visible);
        var asked = [];
        summary.profileRequested.connect(function (typeKey, nation) {
                asked.push(typeKey);
            });
        button.clicked();
        compare(asked, ["spearman"], "the button must name the focused unit");
        summary.destroy();
    }

    function test_a_unit_the_readout_does_not_know_hides_the_details_button() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 1,
                "groups": makeGroups([{
                            "typeKey": "sheep",
                            "count": 1
                        }]),
                "profileLookup": testCase.spearmanLookup
            });
        verify(!summary.canShowProfile);
        verify(!findChild(summary, "selectionProfileButton").visible, "offering details that do not exist is worse than offering none");
        summary.destroy();
    }

    function test_without_a_lookup_the_summary_behaves_exactly_as_before() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 1,
                "groups": makeGroups([{
                            "typeKey": "spearman",
                            "count": 1
                        }])
            });
        verify(!summary.canShowProfile);
        verify(!findChild(summary, "selectionProfileButton").visible);
        verify(!findChild(summary, "selectionStatStrip").visible);
        summary.destroy();
    }

    function test_the_stat_strip_reads_the_profile_not_the_selection() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 1,
                "groups": makeGroups([{
                            "typeKey": "spearman",
                            "count": 1
                        }]),
                "profileLookup": testCase.spearmanLookup
            });
        var strip = findChild(summary, "selectionStatStrip");
        verify(strip !== null, "the compact stat strip is missing");
        verify(strip.visible);
        compare(summary.focusProfile.attack_damage, 19);
        summary.destroy();
    }

    function test_an_inspected_enemy_offers_its_details_too() {
        var summary = summaryComponent.createObject(testCase, {
                "unitCount": 0,
                "groups": [],
                "inspected": makeFocus({
                        "valid": true,
                        "typeKey": "spearman",
                        "name": "Triarius",
                        "isEnemy": true
                    }),
                "profileLookup": testCase.spearmanLookup
            });
        verify(summary.inspecting);
        verify(summary.canShowProfile, "an enemy under the cursor is exactly when its counters matter");
        compare(summary.focusTypeKey, "spearman");
        summary.destroy();
    }

    Component {
        id: summaryComponent

        IronSelectionSummary {
        }
    }
}
