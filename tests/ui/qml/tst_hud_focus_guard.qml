import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "HudFocusGuard"
    when: windowShown
    width: 800
    height: 600
    visible: true

    function test_a_click_survives_the_guard_stealing_focus() {
        var host = guardedHost.createObject(testCase, {
                "width": 800,
                "height": 600
            });
        waitForRendering(host);
        var button = host.button;
        mousePress(button, button.width / 2, button.height / 2);
        wait(80);
        mouseRelease(button, button.width / 2, button.height / 2);
        wait(80);
        compare(host.clicks, 1, "the focus guard cancelled the press and the click was lost");
        compare(host.steals, 0, "a click must not hand the keyboard to a HUD button");
        host.destroy();
    }

    function test_repeated_clicks_all_land() {
        var host = guardedHost.createObject(testCase, {
                "width": 800,
                "height": 600
            });
        waitForRendering(host);
        var button = host.button;
        for (var i = 0; i < 3; ++i) {
            mousePress(button, button.width / 2, button.height / 2);
            wait(40);
            mouseRelease(button, button.width / 2, button.height / 2);
            wait(40);
        }
        compare(host.clicks, 3, "clicks stopped landing once the guard had run");
        host.destroy();
    }

    function test_the_keyboard_can_still_reach_the_button() {
        var host = guardedHost.createObject(testCase, {
                "width": 800,
                "height": 600
            });
        waitForRendering(host);
        verify((host.button.focusPolicy & Qt.TabFocus) === Qt.TabFocus, "tab navigation lost its way to the button");
        host.destroy();
    }

    Component {
        id: guardedHost

        Item {
            id: host

            property alias button: btn
            property int clicks: 0
            property int steals: 0

            Item {
                id: battlefield

                anchors.fill: parent
                focus: true
            }

            IronButton {
                id: btn

                anchors.centerIn: parent
                text: "Battle Report"
                tone: "primary"
                onClicked: host.clicks += 1
                onActiveFocusChanged: {
                    if (!btn.activeFocus)
                        return;
                    host.steals += 1;
                    Qt.callLater(function () {
                            battlefield.forceActiveFocus();
                        });
                }
            }
        }
    }
}
