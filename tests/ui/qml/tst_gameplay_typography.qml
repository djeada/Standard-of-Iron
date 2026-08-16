import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0 as Core
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "GameplayTypography"
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

    QtObject {
        id: stubEngine

        function has_selected_type(type) {
            return type === "barracks";
        }

        function get_selected_production_state() {
            return {
                "has_barracks": true,
                "produced_count": 3,
                "max_units": 500,
                "queue_size": 2,
                "in_progress": true,
                "production_queue": ["archer", "swordsman"],
                "product_type": "archer",
                "villager_cost": 1,
                "manpower_available": 120,
                "build_time": 10,
                "time_remaining": 4.5,
                "nation_id": "roman_republic",
                "has_home": true
            };
        }

        function get_unit_production_info(unitType, nationId) {
            return {
                "cost": 50,
                "population_cost": 50,
                "resource_costs": {
                    "food": 120,
                    "wood": 40
                },
                "build_time": 10
            };
        }

        function selected_player_state() {
            return {
                "food": 500,
                "wood": 500,
                "stone": 500,
                "iron": 500,
                "gold": 500
            };
        }
    }

    property var panelComponent: null

    function panel_component() {
        if (panelComponent === null) {
            panelComponent = Qt.createComponent(Qt.resolvedUrl("../../../ui/qml/ProductionPanel.qml"));
            compare(panelComponent.status, Component.Ready, "ProductionPanel did not compile: " + panelComponent.errorString());
        }
        return panelComponent;
    }

    function makePanel() {
        var host = hostComponent.createObject(testCase, {
                "width": 320,
                "height": 720
            });
        verify(host !== null, "panel host was not created");
        var panel = panel_component().createObject(host, {
                "width": 320,
                "game_instance": stubEngine
            });
        verify(panel !== null, "ProductionPanel was not created");
        host.panel = panel;
        wait(1);
        return host;
    }

    Component {
        id: hostComponent

        Item {
            property var panel: null
        }
    }

    function collectVisibleText(item, out) {
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];
            if (child === null || child.visible === false) {
                continue;
            }
            if (child.contentHeight !== undefined && child.text !== undefined && String(child.text).length > 0) {
                out.push(child);
            }
            collectVisibleText(child, out);
        }
    }

    function panelText() {
        var host = makePanel();
        var found = [];
        collectVisibleText(host.panel, found);
        verify(found.length > 0, "the production panel rendered no text to measure");
        return {
            "host": host,
            "items": found
        };
    }

    function test_no_gameplay_text_falls_below_the_legibility_floor_data() {
        return [{
                "tag": "min",
                "scale": Core.UiPreferences.minUiScale
            }, {
                "tag": "default",
                "scale": 1.0
            }, {
                "tag": "max",
                "scale": Core.UiPreferences.maxUiScale
            }];
    }

    function test_no_gameplay_text_falls_below_the_legibility_floor(data) {
        Core.UiPreferences.uiScale = data.scale;
        var probe = panelText();
        for (var i = 0; i < probe.items.length; ++i) {
            var item = probe.items[i];
            verify(item.font.pixelSize >= Typography.minimumSize, "\"" + String(item.text).substring(0, 24) + "\" renders at " + item.font.pixelSize + "px at scale " + data.tag + ", under the " + Typography.minimumSize + "px floor");
        }
        probe.host.destroy();
    }

    function test_every_label_follows_the_interface_scale() {
        Core.UiPreferences.uiScale = 1.0;
        var small = panelText();
        var baseline = [];
        for (var i = 0; i < small.items.length; ++i) {
            baseline.push({
                    "text": String(small.items[i].text),
                    "px": small.items[i].font.pixelSize
                });
        }
        small.host.destroy();
        Core.UiPreferences.uiScale = 2.0;
        var large = panelText();
        compare(large.items.length, baseline.length, "the panel changed shape between scales");
        for (var j = 0; j < large.items.length; ++j) {
            var grew = large.items[j].font.pixelSize > baseline[j].px;
            verify(grew, "\"" + baseline[j].text.substring(0, 24) + "\" stayed at " + baseline[j].px + "px when the interface scale doubled");
        }
        large.host.destroy();
    }

    function test_labels_stay_inside_the_box_drawn_around_them_data() {
        return test_no_gameplay_text_falls_below_the_legibility_floor_data();
    }

    function test_labels_stay_inside_the_box_drawn_around_them(data) {
        Core.UiPreferences.uiScale = data.scale;
        var probe = panelText();
        var tolerance = 1.0;
        for (var i = 0; i < probe.items.length; ++i) {
            var item = probe.items[i];
            var box = item.parent;
            if (box === null || box.width <= 0 || box.height <= 0) {
                continue;
            }
            if (item.elide !== Text.ElideNone || item.wrapMode !== Text.NoWrap) {
                continue;
            }
            verify(item.contentHeight <= box.height + tolerance, "\"" + String(item.text).substring(0, 24) + "\" is " + item.contentHeight + "px tall inside a " + box.height + "px box at scale " + data.tag);
        }
        probe.host.destroy();
    }
}
