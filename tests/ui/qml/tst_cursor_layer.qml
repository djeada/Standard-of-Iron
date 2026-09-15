import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "CursorLayer"
    when: windowShown
    width: 800
    height: 600
    visible: true

    property var cursors: null

    function init() {
        Core.UiPreferences.reset_to_defaults();
        cursors = cursorComponent.createObject(testCase, {
                "mode": "collect",
                "placingConstruction": true,
                "pointerX": 200,
                "pointerY": 200
            });
        verify(cursors !== null, "the cursor layer was not created");
    }

    function cleanup() {
        if (cursors) {
            cursors.destroy();
            cursors = null;
        }
    }

    function find(item, name) {
        if (!item)
            return null;
        if (item.objectName === name)
            return item;
        var kids = item.children;
        for (var i = 0; i < kids.length; ++i) {
            var hit = find(kids[i], name);
            if (hit)
                return hit;
        }
        return null;
    }

    function chipLabel() {
        var chip = find(cursors, "cursorChip");
        verify(chip !== null, "the cursor chip is missing");
        return chip.visible ? chip.label : "";
    }

    function test_collect_mode_over_nothing_tells_the_player_what_to_point_at() {
        compare(cursors.gatherReady, false);
        verify(!find(cursors, "gatherHandCursor").visible);
        verify(chipLabel().length > 0, "an armed Collect cursor over bare ground must say what it wants");
    }

    function test_a_valid_node_grabs_with_the_hand_and_names_the_work() {
        cursors.constructionPreviewActive = true;
        cursors.constructionPreviewValid = true;
        cursors.interactionHint = {
            "action": "gather",
            "resource": "stone"
        };
        compare(cursors.gatherReady, true);
        verify(find(cursors, "gatherHandCursor").visible);
        compare(chipLabel(), "Quarry this boulder");
        cursors.interactionHint = {
            "action": "gather",
            "resource": "wood"
        };
        compare(chipLabel(), "Chop this tree");
    }

    function test_an_invalid_node_shows_the_reason_not_the_hand() {
        cursors.constructionPreviewActive = true;
        cursors.constructionPreviewValid = false;
        cursors.constructionPreviewReason = "No walkable spot near that boulder.";
        verify(!find(cursors, "gatherHandCursor").visible);
        compare(chipLabel(), "No walkable spot near that boulder.");
    }

    function test_a_refused_click_shakes_the_cursor_then_settles() {
        cursors.constructionPreviewActive = true;
        cursors.constructionPreviewValid = true;
        cursors.report_order_feedback("gather", false, "Select a tree, boulder, ore deposit, ripe farm or sheep.");
        verify(cursors.rejectFlash, "a refusal must flash the cursor");
        verify(!find(cursors, "gatherHandCursor").visible, "the hand must not promise a grab while refusing");
        tryCompare(cursors, "rejectFlash", false, 2000);
        verify(find(cursors, "gatherHandCursor").visible);
    }

    Component {
        id: cursorComponent

        CursorLayer {
            width: 800
            height: 600
        }
    }
}
