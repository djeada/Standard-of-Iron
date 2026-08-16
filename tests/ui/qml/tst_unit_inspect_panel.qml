import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0 as Core
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "UnitInspectPanel"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    function init() {
        Core.UiPreferences.reset_to_defaults();
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function spearmanProfile() {
        return {
            "valid": true,
            "unit_type": "spearman",
            "display_name": "Triarius",
            "role": "Anti-cavalry spear line",
            "role_tags": ["Line infantry", "Spear infantry", "Shielded"],
            "attack_damage": 19,
            "attack_range": 2.5,
            "damage_per_second": 25.3,
            "health": 900,
            "speed": 2.5,
            "vision_range": 15.0,
            "population_cost": 75,
            "build_time": 6.0,
            "individuals_per_unit": 24,
            "resource_costs": {
                "wood": 8,
                "iron": 2
            },
            "abilities": [],
            "strengths": "Repays its cost against cavalry.",
            "weaknesses": "Loses the straight infantry fight to swords.",
            "history": "The spear was the cheapest way to make a horse refuse a charge.",
            "has_lore": true
        };
    }

    function priestProfile() {
        var profile = spearmanProfile();
        profile.display_name = "Grave Priest";
        profile.abilities = [{
                "id": "fireball",
                "name": "Fireball",
                "effect": "Hurls fire that bursts across a formation."
            }];
        return profile;
    }

    Component {
        id: hostComponent

        Item {
            property alias panel: inspectPanel

            UnitInspectPanel {
                id: inspectPanel

                anchors.fill: parent
            }
        }
    }

    function makePanel(w, h, profile, orderStates) {
        var host = hostComponent.createObject(testCase, {
                "width": w,
                "height": h
            });
        verify(host !== null, "inspect panel host was not created");
        host.panel.profile = profile;
        if (orderStates !== undefined) {
            host.panel.order_states = orderStates;
            host.panel.show_availability = true;
        }
        host.panel.visible = true;
        wait(1);
        return host;
    }

    function rectIn(root, item) {
        var topLeft = root.mapFromItem(item, 0, 0);
        return {
            "left": topLeft.x,
            "top": topLeft.y,
            "right": topLeft.x + item.width,
            "bottom": topLeft.y + item.height
        };
    }

    function test_the_card_never_overflows_the_viewport_data() {
        var sizes = [{
                "w": 1920,
                "h": 1080
            }, {
                "w": 1280,
                "h": 720
            }, {
                "w": 1024,
                "h": 768
            }, {
                "w": 800,
                "h": 600
            }, {
                "w": 640,
                "h": 480
            }];
        var scales = [1.0, 1.5, 2.0];
        var rows = [];
        for (var i = 0; i < sizes.length; ++i) {
            for (var s = 0; s < scales.length; ++s) {
                rows.push({
                        "tag": sizes[i].w + "x" + sizes[i].h + "@" + scales[s],
                        "w": sizes[i].w,
                        "h": sizes[i].h,
                        "scale": scales[s]
                    });
            }
        }
        return rows;
    }

    function test_the_card_never_overflows_the_viewport(data) {
        Core.UiPreferences.uiScale = data.scale;
        var host = makePanel(data.w, data.h, testCase.spearmanProfile());
        var card = findChild(host.panel, "unitInspectCard");
        verify(card !== null, "the inspect card is missing");
        var r = rectIn(host.panel, card);
        var tolerance = 1.0;
        verify(r.left >= -tolerance, "card overflows the left edge at " + data.tag);
        verify(r.top >= -tolerance, "card overflows the top edge at " + data.tag);
        verify(r.right <= host.panel.width + tolerance, "card overflows the right edge at " + data.tag + " (" + r.right + " > " + host.panel.width + ")");
        verify(r.bottom <= host.panel.height + tolerance, "card overflows the bottom edge at " + data.tag + " (" + r.bottom + " > " + host.panel.height + ")");
        host.destroy();
    }

    function test_long_content_scrolls_rather_than_spilling_data() {
        return test_the_card_never_overflows_the_viewport_data();
    }

    function test_long_content_scrolls_rather_than_spilling(data) {
        Core.UiPreferences.uiScale = data.scale;
        var host = makePanel(data.w, data.h, testCase.priestProfile());
        var scroller = findChild(host.panel, "unitInspectScroll");
        verify(scroller !== null, "the detail list is missing its scroll view");
        verify(scroller.clip, "the scroll view must clip, or sections paint over the card at " + data.tag);
        host.destroy();
    }

    function test_every_section_the_profile_fills_is_shown() {
        var host = makePanel(1280, 720, testCase.priestProfile());
        var names = ["unitInspectCombat", "unitInspectAbilities", "unitInspectCost", "unitInspectTactics", "unitInspectHistory"];
        for (var i = 0; i < names.length; ++i) {
            var section = findChild(host.panel, names[i]);
            verify(section !== null, names[i] + " is missing");
            verify(section.visible, names[i] + " is populated but hidden");
        }
        compare(findChild(host.panel, "unitInspectName").text, "Grave Priest");
        host.destroy();
    }

    function test_a_unit_without_lore_hides_the_prose_sections() {
        var bare = testCase.spearmanProfile();
        bare.strengths = "";
        bare.weaknesses = "";
        bare.history = "";
        bare.has_lore = false;
        var host = makePanel(1280, 720, bare);
        verify(findChild(host.panel, "unitInspectCombat").visible, "stats must survive missing lore");
        verify(!findChild(host.panel, "unitInspectTactics").visible, "an empty strengths section must not draw an empty box");
        verify(!findChild(host.panel, "unitInspectHistory").visible, "an empty history section must not draw an empty box");
        host.destroy();
    }

    function test_an_invalid_profile_says_so_instead_of_drawing_zeroes() {
        var host = makePanel(1280, 720, {
                "valid": false
            });
        verify(!host.panel.has_profile);
        verify(!findChild(host.panel, "unitInspectCombat").visible, "an unknown unit must not report zeroed stats as fact");
        host.destroy();
    }

    function test_order_availability_is_only_claimed_when_a_selection_is_open() {
        var states = {
            "attack": {
                "enabled": true,
                "eligibleCount": 3
            },
            "build": {
                "enabled": false,
                "eligibleCount": 0
            },
            "guard": {
                "enabled": false,
                "eligibleCount": 2
            }
        };
        var fromSelection = makePanel(1280, 720, testCase.spearmanProfile(), states);
        compare(fromSelection.panel.available_orders.length, 2, "only orders the selection is eligible for should be listed");
        verify(fromSelection.panel.order_available("attack"));
        verify(!fromSelection.panel.order_available("guard"), "an eligible but currently blocked order must not read as ready");
        fromSelection.destroy();
        var fromRecruitCard = makePanel(1280, 720, testCase.spearmanProfile());
        compare(fromRecruitCard.panel.available_orders.length, 0, "opened from a recruit card there is no selection to report on");
        fromRecruitCard.destroy();
    }
}
