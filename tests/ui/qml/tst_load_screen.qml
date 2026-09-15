import QtQuick 2.15
import QtTest 1.15
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "LoadScreen"
    when: windowShown
    width: 800
    height: 400
    visible: true

    function test_loading_content_stays_above_tip_on_short_viewport() {
        var host = hostComponent.createObject(testCase, {
                "width": 800,
                "height": 360
            });
        verify(host !== null, "the loading screen host was not created");
        wait(1);

        var content = findChild(host, "loading_content");
        var tip = findChild(host, "tip_plate");
        verify(content !== null, "the loading content was not found");
        verify(tip !== null, "the loading tip plate was not found");
        verify(content.y + content.height <= tip.y,
               "the loading content overlaps the tip plate");

        host.destroy();
    }

    Component {
        id: hostComponent

        Item {
            property alias screen: loadScreen

            LoadScreen {
                id: loadScreen

                anchors.fill: parent
                is_loading: true
            }
        }
    }
}
