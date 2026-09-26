import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    property var answers: []
    property var looks: []
    property bool accept_answers: true

    name: "AllyAppealPanel"
    when: windowShown
    width: 900
    height: 500
    visible: true

    function init() {
        testCase.answers = [];
        testCase.looks = [];
        testCase.accept_answers = true;
    }

    function appeal(id, kind) {
        return {
            "id": id,
            "from": 2,
            "name": "Publius Cornelius Scipio",
            "kind": kind,
            "text": "Scipio needs you.",
            "accept": "Send men",
            "decline": "Refuse",
            "x": 12,
            "z": -4,
            "seconds": 45
        };
    }

    function make_panel() {
        var panel = panelComponent.createObject(testCase, {
                "engine": engine
            });
        verify(panel !== null);
        return panel;
    }

    function cards(panel) {
        var found = [];
        var stack = [panel];
        while (stack.length > 0) {
            var item = stack.pop();
            if (item.objectName === "allyAppealCard" && item.visible)
                found.push(item);
            for (var i = 0; i < item.children.length; ++i)
                stack.push(item.children[i]);
        }
        return found;
    }

    function button(card, name) {
        var stack = [card];
        while (stack.length > 0) {
            var item = stack.pop();
            if (item.objectName === name)
                return item;
            for (var i = 0; i < item.children.length; ++i)
                stack.push(item.children[i]);
        }
        return null;
    }

    function test_an_appeal_opens_a_card_and_accepting_answers_it() {
        var panel = make_panel();
        verify(!panel.visible, "nothing to answer, nothing shown");
        engine.ally_appeal_opened(appeal(7, "defend"));
        tryCompare(panel, "visible", true);
        var shown = cards(panel);
        compare(shown.length, 1);
        mouseClick(button(shown[0], "allyAppealAccept"));
        compare(testCase.answers.length, 1);
        compare(testCase.answers[0], "7:true");
        tryCompare(panel, "visible", false);
        panel.destroy();
    }

    function test_refusing_answers_no_and_show_moves_the_camera() {
        var panel = make_panel();
        engine.ally_appeal_opened(appeal(8, "attack"));
        tryCompare(panel, "visible", true);
        var card = cards(panel)[0];
        mouseClick(button(card, "allyAppealShow"));
        compare(testCase.looks.length, 1);
        compare(testCase.looks[0], "12,-4");
        mouseClick(button(card, "allyAppealDecline"));
        compare(testCase.answers[0], "8:false");
        panel.destroy();
    }

    function test_a_resource_appeal_has_no_show_button_and_a_withdrawn_one_closes() {
        var panel = make_panel();
        engine.ally_appeal_opened(appeal(9, "resources"));
        engine.ally_appeal_opened(appeal(9, "resources"));
        tryCompare(panel, "visible", true);
        compare(cards(panel).length, 1, "the same appeal is shown once");
        verify(!button(cards(panel)[0], "allyAppealShow").visible);
        engine.ally_appeal_closed(9);
        tryCompare(panel, "visible", false);
        panel.destroy();
    }

    function test_a_refused_answer_keeps_the_card() {
        var panel = make_panel();
        testCase.accept_answers = false;
        engine.ally_appeal_opened(appeal(10, "resources"));
        tryCompare(panel, "visible", true);
        mouseClick(button(cards(panel)[0], "allyAppealAccept"));
        compare(cards(panel).length, 1, "a gift the player cannot afford leaves the appeal open");
        panel.destroy();
    }

    QtObject {
        id: engine

        property QtObject production: QtObject {
            function answer_ally_appeal(id, accept) {
                testCase.answers.push(id + ":" + accept);
                return testCase.accept_answers;
            }
        }
        property QtObject camera: QtObject {
            function look_at_world(x, z) {
                testCase.looks.push(x + "," + z);
            }
        }

        signal ally_appeal_opened(var appeal)
        signal ally_appeal_closed(var appeal_id)
    }

    Component {
        id: panelComponent

        AllyAppealPanel {
        }
    }
}
