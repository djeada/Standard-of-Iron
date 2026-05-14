import QtQuick 2.15

Item {
    id: root

    property var commanderInput: null
    property var gameView
    property var mainWindowRef
    property bool active: false

    signal inputCaptured()

    function scene_center() {
        return root.mapToItem(null, root.width * 0.5, root.height * 0.5);
    }

    function center_mouse() {
        if (!root.active || root.commanderInput === null || !root.commanderInput.center_mouse)
            return ;

        var center = scene_center();
        root.commanderInput.center_mouse(center.x, center.y);
    }

    function release_actions() {
        if (root.commanderInput === null)
            return ;

        if (root.commanderInput.primary_action_up)
            root.commanderInput.primary_action_up();

        if (root.commanderInput.secondary_action_up)
            root.commanderInput.secondary_action_up();

    }

    function handle_key_pressed(event) {
        if (!root.active)
            return ;

        switch (event.key) {
        case Qt.Key_Escape:
            if (typeof root.mainWindowRef !== 'undefined' && !root.mainWindowRef.menu_visible)
                root.mainWindowRef.menu_visible = true;

            event.accepted = true;
            return ;
        case Qt.Key_Space:
            if (!event.isAutoRepeat && root.commanderInput !== null && root.commanderInput.dodge)
                root.commanderInput.dodge();
            event.accepted = true;
            return ;
        case Qt.Key_Alt:
        case Qt.Key_AltGr:
            if (!event.isAutoRepeat && root.commanderInput !== null && root.commanderInput.jump)
                root.commanderInput.jump();
            event.accepted = true;
            return ;
        case Qt.Key_Tab:
            if (!event.isAutoRepeat && root.commanderInput !== null && root.commanderInput.cycle_lock_on)
                root.commanderInput.cycle_lock_on();
            event.accepted = true;
            return ;
        case Qt.Key_F:
            if (!event.isAutoRepeat && root.commanderInput !== null && root.commanderInput.special_action)
                root.commanderInput.special_action();
            event.accepted = true;
            return ;
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (root.commanderInput !== null && root.commanderInput.toggle_mode)
                root.commanderInput.toggle_mode();

            event.accepted = true;
            return ;
        case Qt.Key_R:
            if (!event.isAutoRepeat && root.commanderInput !== null && root.commanderInput.trigger_rally)
                root.commanderInput.trigger_rally();

            event.accepted = true;
            return ;
        case Qt.Key_W:
        case Qt.Key_A:
        case Qt.Key_S:
        case Qt.Key_D:
        case Qt.Key_Q:
        case Qt.Key_E:
        case Qt.Key_Shift:
            if (root.commanderInput !== null && root.commanderInput.key_down)
                root.commanderInput.key_down(event.key, event.modifiers);

            event.accepted = true;
            return ;
        }
    }

    function handle_key_released(event) {
        if (!root.active)
            return ;

        switch (event.key) {
        case Qt.Key_Alt:
        case Qt.Key_AltGr:
            event.accepted = true;
            return ;
        case Qt.Key_W:
        case Qt.Key_A:
        case Qt.Key_S:
        case Qt.Key_D:
        case Qt.Key_Q:
        case Qt.Key_E:
        case Qt.Key_Shift:
            if (root.commanderInput !== null && root.commanderInput.key_up)
                root.commanderInput.key_up(event.key, event.modifiers);

            event.accepted = true;
            return ;
        }
    }

    enabled: active
    visible: active
    z: 20
    Component.onCompleted: {
        if (active && typeof root.gameView !== 'undefined')
            root.gameView.forceActiveFocus();

    }
    onActiveChanged: {
        if (active) {
            if (typeof root.gameView !== 'undefined')
                root.gameView.forceActiveFocus();
            root.inputCaptured();
            center_mouse();
        } else {
            release_actions();
        }

    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.BlankCursor
        enabled: root.active
        hoverEnabled: true
        preventStealing: true
        onEntered: {
            root.inputCaptured();
            root.center_mouse();
        }
        onPositionChanged: function(mouse) {
            root.inputCaptured();
            mouse.accepted = true;
        }
        onWheel: function(w) {
            w.accepted = true;
        }
        onPressed: function(mouse) {
            if (typeof root.gameView !== 'undefined')
                root.gameView.forceActiveFocus();
            root.inputCaptured();
            if (mouse.button === Qt.LeftButton && root.commanderInput !== null && root.commanderInput.primary_action_down)
                root.commanderInput.primary_action_down();

            if (mouse.button === Qt.RightButton && root.commanderInput !== null && root.commanderInput.secondary_action_down)
                root.commanderInput.secondary_action_down();

            mouse.accepted = true;
        }
        onReleased: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.commanderInput !== null && root.commanderInput.primary_action_up)
                root.commanderInput.primary_action_up();

            if (mouse.button === Qt.RightButton && root.commanderInput !== null && root.commanderInput.secondary_action_up)
                root.commanderInput.secondary_action_up();

            mouse.accepted = true;
        }
        onCanceled: root.release_actions()
    }

    focus: false
    Keys.enabled: active
    Keys.onPressed: function(event) {
        root.handle_key_pressed(event);
    }

    Keys.onReleased: function(event) {
        root.handle_key_released(event);
    }
}
