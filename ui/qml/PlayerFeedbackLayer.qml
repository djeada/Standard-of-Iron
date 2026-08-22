import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: root

    property var activity: null
    property int lastSequence: 0

    signal feedbackReceived(string type, var event)

    function tint_for(type) {
        switch (type) {
        case "perfect_guard":
            return "#7FD4FF";
        case "guard_broken":
            return "#E4685D";
        case "dodge_success":
            return "#B9F0C6";
        case "resource_insufficient":
        case "order_rejected":
            return "#E4A05D";
        default:
            return "";
        }
    }

    function flash(type) {
        var tint = root.tint_for(type);
        if (tint.length === 0)
            return;
        edgeFlash.color = tint;
        edgeFlashAnimation.restart();
    }

    anchors.fill: parent
    z: 999996

    Rectangle {
        id: edgeFlash

        anchors.fill: parent
        color: "transparent"
        opacity: 0.0
        visible: opacity > 0.001

        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: "#00000000"
            }

            GradientStop {
                position: 1.0
                color: edgeFlash.color
            }
        }
    }

    SequentialAnimation {
        id: edgeFlashAnimation

        NumberAnimation {
            target: edgeFlash
            property: "opacity"
            from: 0.0
            to: Design.A11y.reducedMotion ? 0.10 : 0.22
            duration: 60
        }

        NumberAnimation {
            target: edgeFlash
            property: "opacity"
            to: 0.0
            duration: 320
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: root.activity !== null && root.visible
        onTriggered: {
            if (root.activity === null || !root.activity.pop_player_feedback_events)
                return;
            var events = root.activity.pop_player_feedback_events();
            for (var i = 0; i < events.length; ++i) {
                var ev = events[i];
                root.lastSequence = Number(ev.sequence || 0);
                root.flash(String(ev.type || ""));
                root.feedbackReceived(String(ev.type || ""), ev);
            }
        }
    }
}
