import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "MapThumbnails"
    when: windowShown
    width: 200
    height: 200
    visible: true

    property int generatedCount: 0
    property var storedIds: []

    function makeThumbnails() {
        generatedCount = 0;
        storedIds = [];
        var thumbs = thumbnailComponent.createObject(testCase);
        verify(thumbs !== null, "the thumbnail source was not created");
        thumbs.previewSource = {
            "map_preview": function (path, configs) {
                testCase.generatedCount += 1;
                return "image-for:" + path;
            }
        };
        thumbs.previewStore = {
            "set_preview_image": function (id, image) {
                testCase.storedIds.push(id);
            }
        };
        thumbs.cache = ({});
        return thumbs;
    }

    function test_a_map_without_an_authored_thumbnail_gets_a_rendered_one() {
        var thumbs = makeThumbnails();
        var source = thumbs.source_for("", "assets/maps/map_rivers.json");
        compare(source, "image://mappreview/maplist:assets/maps/map_rivers.json");
        compare(generatedCount, 1, "the preview was not rendered");
        compare(storedIds.length, 1);
        thumbs.destroy();
    }

    function test_an_authored_thumbnail_is_used_as_is() {
        var thumbs = makeThumbnails();
        var source = thumbs.source_for(":/assets/maps/hand_drawn.png", "assets/maps/map_rivers.json");
        compare(source, ":/assets/maps/hand_drawn.png");
        compare(generatedCount, 0, "an authored thumbnail was rendered over");
        thumbs.destroy();
    }

    function test_scrolling_back_to_a_row_does_not_render_it_again() {
        var thumbs = makeThumbnails();
        thumbs.source_for("", "assets/maps/map_rivers.json");
        thumbs.source_for("", "assets/maps/map_rivers.json");
        thumbs.source_for("", "assets/maps/map_spanish_grove.json");
        thumbs.source_for("", "assets/maps/map_rivers.json");
        compare(generatedCount, 2, "a recycled row re-rendered a map it had already drawn");
        thumbs.destroy();
    }

    function test_a_row_with_no_map_path_asks_for_nothing() {
        var thumbs = makeThumbnails();
        compare(thumbs.source_for("", ""), "");
        compare(generatedCount, 0);
        thumbs.destroy();
    }

    function test_a_missing_generator_leaves_the_row_blank_rather_than_failing() {
        var thumbs = makeThumbnails();
        thumbs.previewSource = null;
        thumbs.previewStore = null;
        compare(thumbs.source_for("", "assets/maps/map_rivers.json"), "");
        thumbs.destroy();
    }

    function test_the_battlefield_list_screen_carries_a_thumbnail_source() {
        var screen = mapSelectComponent.createObject(testCase);
        verify(screen !== null, "MapSelect was not created");
        var found = null;
        for (var i = 0; i < screen.children.length; ++i) {
            if (screen.children[i].source_for !== undefined) {
                found = screen.children[i];
                break;
            }
        }
        verify(found !== null, "the battlefield list screen has no thumbnail source");
        screen.destroy();
    }

    Component {
        id: thumbnailComponent

        MapThumbnails {
        }
    }

    Component {
        id: mapSelectComponent

        MapSelect {
        }
    }
}
