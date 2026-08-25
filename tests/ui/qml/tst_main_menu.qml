import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "MainMenu"
    when: windowShown
    width: 1280
    height: 800
    visible: true

    function make_menu(game_started) {
        var menu = menuComponent.createObject(testCase, {
                "game_started": game_started
            });
        verify(menu !== null, "MainMenu failed to instantiate");
        menu.forceActiveFocus();
        verify(menu.activeFocus, "the menu never took the keyboard");
        return menu;
    }

    function test_escape_asks_to_resume_the_battle() {
        var menu = make_menu(true);
        var spy = spyComponent.createObject(testCase, {
                "target": menu,
                "signalName": "resume_requested"
            });
        keyClick(Qt.Key_Escape);
        compare(spy.count, 1, "Esc did not ask to resume the battle");
        spy.destroy();
        menu.destroy();
    }

    function test_escape_keeps_asking_every_time_the_menu_is_reopened() {
        var menu = make_menu(true);
        var spy = spyComponent.createObject(testCase, {
                "target": menu,
                "signalName": "resume_requested"
            });
        for (var round = 0; round < 3; ++round) {
            menu.visible = false;
            menu.visible = true;
            menu.forceActiveFocus();
            keyClick(Qt.Key_Escape);
        }
        compare(spy.count, 3, "Esc stopped resuming after the menu was reopened");
        spy.destroy();
        menu.destroy();
    }

    function test_escape_does_nothing_without_a_battle_to_return_to() {
        var menu = make_menu(false);
        var spy = spyComponent.createObject(testCase, {
                "target": menu,
                "signalName": "resume_requested"
            });
        keyClick(Qt.Key_Escape);
        compare(spy.count, 0, "Esc resumed a battle that was never started");
        spy.destroy();
        menu.destroy();
    }

    function test_the_menu_swallows_clicks_meant_for_nothing_behind_it() {
        var below = catcherComponent.createObject(testCase);
        below.z = -1;
        var menu = make_menu(true);
        mouseClick(testCase, 6, 6);
        compare(below.hits, 0, "a click on the menu reached the battlefield behind it");
        menu.destroy();
        below.destroy();
    }

    function test_a_click_on_the_menu_leaves_the_keyboard_with_the_menu() {
        var below = catcherComponent.createObject(testCase);
        below.z = -1;
        var menu = make_menu(true);
        mouseClick(testCase, 6, 6);
        verify(menu.activeFocus, "clicking the menu handed the keyboard back to the battlefield");
        menu.destroy();
        below.destroy();
    }

    Component {
        id: menuComponent

        MainMenu {
        }
    }

    Component {
        id: catcherComponent

        MouseArea {
            property int hits: 0

            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: {
                hits += 1;
                forceActiveFocus();
            }
        }
    }

    Component {
        id: spyComponent

        SignalSpy {
        }
    }
}
