import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "SaveProgressOverlay"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    Component {
        id: overlayComponent

        SaveProgressOverlay {
        }
    }

    function make_overlay() {
        var overlay = createTemporaryObject(overlayComponent, testCase);
        verify(overlay !== null, "SaveProgressOverlay failed to instantiate");
        overlay.show_notice("Saved \"autosave_3\"", false);
        wait(20);
        return overlay;
    }

    function test_the_notice_stays_a_corner_card_after_commander_mode() {
        var overlay = make_overlay();
        var card = findChild(overlay, "saveProgressCard");
        verify(card !== null);
        compare(card.width, 320);
        verify(card.x + card.width > overlay.width * 0.75, "the army-view card sits at the right edge");
        verify(card.y > overlay.height * 0.5, "the army-view card sits at the bottom");
        overlay.top_margin = 60;
        overlay.top_anchored = true;
        wait(20);
        compare(card.width, 320);
        verify(Math.abs(card.x + card.width / 2 - overlay.width / 2) <= 1, "commander mode centres the card");
        verify(card.y < overlay.height * 0.25, "commander mode lifts the card to the top");
        overlay.top_anchored = false;
        wait(20);
        compare(card.width, 320, "leaving commander mode stretched the card across the screen");
        verify(card.height < overlay.height * 0.4, "leaving commander mode stretched the card down the screen");
        verify(card.x + card.width > overlay.width * 0.75);
        verify(card.y > overlay.height * 0.5);
    }
}
