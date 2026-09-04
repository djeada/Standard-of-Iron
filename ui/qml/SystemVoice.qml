import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

Item {
    id: systemVoice

    property var engine: null

    readonly property int repeatsBeforeSpeaking: 3
    readonly property int forgetAfterMs: 20000

    property string lastFailure: ""
    property int lastFailureCount: 0
    property real lastFailureAt: 0

    function quip_for(failure) {
        switch (failure) {
        case "no_selection":
            return qsTr("The order was excellent. It was given to nobody.");
        case "unreachable":
            return qsTr("There is no road to that place. There may never have been one.");
        case "insufficient_resources":
            return qsTr("The quartermaster has checked twice. There is nothing behind the second check.");
        case "manpower_cap":
            return qsTr("We have the barracks, the coin and the will. We lack the people.");
        case "unit_busy":
            return qsTr("They are carrying something heavier than your orders.");
        case "out_of_range":
            return qsTr("Willing, sir. Simply not that willing, and not from here.");
        case "wrong_owner":
            return qsTr("Those are not ours to command. They have been very clear about it.");
        case "invalid_target":
            return qsTr("Nothing there answers to that order. We checked.");
        }
        return "";
    }

    function note_refusal(failure) {
        if (!failure || failure === "none")
            return;
        var now = Date.now();
        if (failure !== systemVoice.lastFailure || (now - systemVoice.lastFailureAt) > systemVoice.forgetAfterMs) {
            systemVoice.lastFailure = failure;
            systemVoice.lastFailureCount = 0;
        }
        systemVoice.lastFailureAt = now;
        systemVoice.lastFailureCount += 1;
        if (systemVoice.lastFailureCount < systemVoice.repeatsBeforeSpeaking)
            return;
        var line = systemVoice.quip_for(failure);
        if (line.length === 0)
            return;
        systemVoice.lastFailureCount = 0;
        Design.Notifications.info(line, {
                "channel": "refusal-" + failure,
                "icon": Design.Icons.objective
            });
    }

    function forget() {
        systemVoice.lastFailure = "";
        systemVoice.lastFailureCount = 0;
    }

    function announce_busy(message) {
        if (!message || message.length === 0)
            return;
        Design.UiSound.warning();
        Design.Notifications.urgent(message, {
                "channel": "refusal-unit_busy",
                "icon": Design.Icons.warning
            });
    }

    Connections {
        function onOrder_feedback(kind, accepted, message, failure) {
            if (accepted) {
                systemVoice.forget();
            } else if (failure === "unit_busy") {
                systemVoice.announce_busy(message);
            } else {
                systemVoice.note_refusal(failure);
            }
        }

        target: systemVoice.engine
    }
}
