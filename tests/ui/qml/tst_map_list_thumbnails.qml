import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

// No shipped map authors a thumbnail and there is no *_thumb.png anywhere in
// the tree, so every row of the battlefield list showed the same empty square
// while the real preview rendered only in the detail panel. The rows now draw
// the same preview, generated once per map and held by path - a ListView
// recycles its delegates, and re-rendering a map every time it scrolls back
// into view would make the list crawl.
TestCase {
    id: testCase

    name: "MapListThumbnails"
    when: windowShown
    width: 400
    height: 400
    visible: true

    property int generatedCount: 0
    property var storedIds: []

    function makePanel() {
        generatedCount = 0;
        storedIds = [];
        var panel = panelComponent.createObject(testCase, {
                "width": 360,
                "height": 320
            });
        verify(panel !== null, "the map list panel was not created");
        panel.previewSource = {
            "map_preview": function (path, configs) {
                testCase.generatedCount += 1;
                return "image-for:" + path;
            }
        };
        panel.previewStore = {
            "set_preview_image": function (id, image) {
                testCase.storedIds.push(id);
            }
        };
        panel.thumbnailCache = ({});
        return panel;
    }

    function test_a_map_without_an_authored_thumbnail_gets_a_rendered_one() {
        var panel = makePanel();
        var source = panel.thumbnail_source("", "assets/maps/map_rivers.json");
        compare(source, "image://mappreview/maplist:assets/maps/map_rivers.json");
        compare(generatedCount, 1, "the preview was not rendered");
        compare(storedIds.length, 1);
        panel.destroy();
    }

    function test_an_authored_thumbnail_is_used_as_is() {
        var panel = makePanel();
        var source = panel.thumbnail_source(":/assets/maps/hand_drawn.png", "assets/maps/map_rivers.json");
        compare(source, ":/assets/maps/hand_drawn.png");
        compare(generatedCount, 0, "an authored thumbnail was rendered over");
        panel.destroy();
    }

    function test_scrolling_back_to_a_row_does_not_render_it_again() {
        var panel = makePanel();
        panel.thumbnail_source("", "assets/maps/map_rivers.json");
        panel.thumbnail_source("", "assets/maps/map_rivers.json");
        panel.thumbnail_source("", "assets/maps/map_spanish_grove.json");
        panel.thumbnail_source("", "assets/maps/map_rivers.json");
        compare(generatedCount, 2, "a recycled row re-rendered a map it had already drawn");
        panel.destroy();
    }

    function test_a_row_with_no_map_path_asks_for_nothing() {
        var panel = makePanel();
        compare(panel.thumbnail_source("", ""), "");
        compare(generatedCount, 0);
        panel.destroy();
    }

    // The panel has to survive being used outside the running game, where
    // neither the generator nor the image store exists.
    function test_a_missing_generator_leaves_the_row_blank_rather_than_failing() {
        var panel = makePanel();
        panel.previewSource = null;
        panel.previewStore = null;
        compare(panel.thumbnail_source("", "assets/maps/map_rivers.json"), "");
        panel.destroy();
    }

    Component {
        id: panelComponent

        MapListPanel {
            colors: ({
                    "textMain": "#ffffff",
                    "textSub": "#cccccc",
                    "textSubLite": "#aaaaaa",
                    "panelBr": "#333333",
                    "thumbBr": "#444444",
                    "rowBg": "#111111",
                    "rowBgAlt": "#151515",
                    "selectedBg": "#222222",
                    "selectedBr": "#c9a227"
                })
        }
    }
}
