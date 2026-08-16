import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "GameSpeed"
    when: windowShown
    width: 1280
    height: 200
    visible: true

    function makeTop(props) {
        return topComponent.createObject(testCase, props);
    }

    function test_the_bar_offers_three_and_four_times_speed() {
        var top = makeTop({
                "width": testCase.width,
                "height": testCase.height
            });
        wait(50);
        var options = top.speedOptions;
        verify(options.length >= 5);
        compare(options[0], 0.5);
        compare(options[options.length - 1], 4);
        verify(options.indexOf(3) !== -1);
        verify(options.indexOf(4) !== -1);
        top.destroy();
    }

    function test_speeds_are_labelled_without_trailing_noise() {
        var top = makeTop({});
        compare(top.speed_label(0.5), "0.5×");
        compare(top.speed_label(1), "1×");
        compare(top.speed_label(3), "3×");
        compare(top.speed_label(4), "4×");
        top.destroy();
    }

    function test_the_compact_selector_points_at_the_live_speed() {
        var top = makeTop({});
        var labels = top.speed_labels();
        compare(labels.length, top.speedOptions.length);
        for (var i = 0; i < top.speedOptions.length; ++i) {
            compare(top.speed_index(top.speedOptions[i]), i);
            compare(labels[i], top.speed_label(top.speedOptions[i]));
        }
        top.destroy();
    }

    function test_an_unlisted_speed_still_resolves_to_a_selector_entry() {
        var top = makeTop({});
        compare(top.speed_index(0), 0);
        compare(top.speed_index(99), top.speedOptions.length - 1);
        compare(top.speed_index(2.9), top.speedOptions.indexOf(3));
        top.destroy();
    }

    function test_pausing_does_not_hide_which_speed_is_active() {
        var top = makeTop({
                "width": testCase.width,
                "height": testCase.height,
                "current_speed": 3
            });
        wait(50);
        var running = testCase.findSpeedButtons(top);
        verify(running.length === top.speedOptions.length);
        compare(testCase.activeTone(running, top), "3×");
        top.game_is_paused = true;
        wait(50);
        var paused = testCase.findSpeedButtons(top);
        compare(testCase.activeTone(paused, top), "3×");
        top.destroy();
    }

    function findSpeedButtons(top) {
        var found = [];
        testCase.collectSpeedButtons(top, top, found);
        return found;
    }

    function collectSpeedButtons(top, item, found) {
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];
            if (child.text !== undefined && child.tone !== undefined && testCase.isSpeedLabel(top, child.text))
                found.push(child);
            testCase.collectSpeedButtons(top, child, found);
        }
    }

    function isSpeedLabel(top, text) {
        for (var i = 0; i < top.speedOptions.length; ++i) {
            if (top.speed_label(top.speedOptions[i]) === text)
                return true;
        }
        return false;
    }

    function activeTone(buttons, top) {
        for (var i = 0; i < buttons.length; ++i) {
            if (buttons[i].tone === "primary")
                return buttons[i].text;
        }
        return "";
    }

    Component {
        id: topComponent

        HUDTop {
        }
    }
}
