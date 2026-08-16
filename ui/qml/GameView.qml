import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0
import StandardOfIron.Design 1.0 as Design

Item {
    id: game_view

    property bool is_paused: false
    property real game_speed: 1
    property string cursor_mode: "normal"
    property bool is_placing_formation: false
    property bool is_placing_construction: false
    property bool construction_preview_active: typeof game !== 'undefined' && game.placement.construction_preview_active
    property bool construction_preview_valid: typeof game !== 'undefined' && game.placement.construction_preview_valid
    property var pressed_keys: ({})
    property var pan_axis: ({})

    readonly property bool camera_pan_active: renderArea.key_pan_count > 0 || renderArea.mouse_pan_active

    onConstruction_preview_activeChanged: {
        if (constructionCursor)
            constructionCursor.requestPaint();
        if (collectCursorOverlay)
            collectCursorOverlay.requestPaint();
    }

    onConstruction_preview_validChanged: {
        if (constructionCursor)
            constructionCursor.requestPaint();
        if (collectCursorOverlay)
            collectCursorOverlay.requestPaint();
    }

    onIs_placing_constructionChanged: {
        if (constructionCursor)
            constructionCursor.requestPaint();
        if (collectCursorOverlay)
            collectCursorOverlay.requestPaint();
    }

    signal map_clicked(real x, real y)
    signal unit_selected(int unitId)
    signal area_selected(real x1, real y1, real x2, real y2)

    function set_paused(paused) {
        is_paused = paused;
        if (typeof game !== 'undefined' && game.set_paused)
            game.set_paused(paused);
    }

    function set_game_speed(speed) {
        game_speed = speed;
        if (typeof game !== 'undefined' && game.set_game_speed)
            game.set_game_speed(speed);
    }

    function issue_command(command) {
    }

    function action_enabled(actionId) {
        if (typeof game === 'undefined' || !game.orders.action_states)
            return false;
        var states = game.orders.action_states();
        var state = states ? states[actionId] : null;
        return state && state.enabled === true;
    }

    function input_context() {
        return is_commander_mode() ? "commander" : "rts";
    }

    function begin_pan_action(actionId, e) {
        if (e.isAutoRepeat || pan_axis[actionId])
            return;
        pan_axis[actionId] = true;
        pressed_keys[e.key] = actionId;
        renderArea.key_pan_count += 1;
    }

    function end_pan_action(actionId) {
        if (!pan_axis[actionId])
            return;
        delete pan_axis[actionId];
        renderArea.key_pan_count = Math.max(0, renderArea.key_pan_count - 1);
        if (!any_pan_held()) {
            if (keyPanTimer.running)
                keyPanTimer.stop();
        }
    }

    function any_pan_held() {
        for (var held in pan_axis) {
            if (pan_axis[held])
                return true;
        }
        return false;
    }

    function ensure_pan_timer_running() {
        if (!keyPanTimer.running)
            keyPanTimer.start();
    }

    function is_commander_mode() {
        return typeof game !== 'undefined' && game.commander.control_mode === "commander";
    }

    function is_commander_rally_placement() {
        return cursor_mode === "place_commander_rally";
    }

    function is_barracks_rally_placement() {
        return cursor_mode === "place_barracks_rally";
    }

    function is_rally_placement() {
        return is_commander_rally_placement() || is_barracks_rally_placement();
    }

    function cancel_rally_placement() {
        if (typeof game === 'undefined')
            return;
        if (is_commander_rally_placement()) {
            if (game.commander.cancel_flag_rally)
                game.commander.cancel_flag_rally();
        } else if (is_barracks_rally_placement()) {
            if (game.commander.cancel_barracks_rally)
                game.commander.cancel_barracks_rally();
        }
    }

    function reset_rts_pan_keys() {
        pressed_keys = ({});
        pan_axis = ({});
        if (keyPanTimer.running)
            keyPanTimer.stop();
        if (typeof renderArea !== 'undefined')
            renderArea.key_pan_count = 0;
    }

    function select_formation_intent_slot(slot_index) {
        if (typeof game === 'undefined' || !game.placement.set_formation_intent)
            return;
        var intents = game.placement.formation_intents;
        if (slot_index < 0 || slot_index >= intents.length)
            return;
        var intent_id = intents[slot_index];
        if (game.placement.formation_intent_unavailable_reason && game.placement.formation_intent_unavailable_reason(intent_id).length > 0)
            return;
        game.placement.set_formation_intent(intent_id);
    }

    function perform_global_action(actionId, event) {
        switch (actionId) {
        case "global.toggle_control_mode":
            reset_rts_pan_keys();
            if (game.commander.toggle_mode)
                game.commander.toggle_mode();
            return true;
        case "global.menu":
            if (game.placement.is_placing_construction && game.placement.on_construction_cancel)
                game.placement.on_construction_cancel();
            else if (game.placement.is_placing_formation && game.placement.on_formation_cancel)
                game.placement.on_formation_cancel();
            else if (game_view.is_rally_placement())
                game_view.cancel_rally_placement();
            else if (typeof mainWindow !== 'undefined' && !mainWindow.menu_visible)
                mainWindow.menu_visible = true;
            return true;
        case "global.quicksave":
            if (game.saves.quicksave)
                game.saves.quicksave();
            return true;
        case "global.quickload":
            if (game.saves.has_save_slot && game.saves.has_save_slot("quicksave"))
                game.saves.load_from_slot("quicksave");
            return true;
        }
        return false;
    }

    function perform_rts_action(actionId, event) {
        var shiftHeld = (event.modifiers & Qt.ShiftModifier) !== 0;
        var yawStep = shiftHeld ? 8 : 4;
        var inputStep = shiftHeld ? 2 : 1;
        var zoomStep = shiftHeld ? 2 : 1;
        switch (actionId) {
        case "rts.pause":
            if (typeof mainWindow === 'undefined')
                return false;
            mainWindow.game_paused = !mainWindow.game_paused;
            game_view.set_paused(mainWindow.game_paused);
            return true;
        case "rts.order_stop":
            if (!game.has_units_selected)
                return false;
            if (game.orders.stop)
                game.orders.stop();
            return true;
        case "rts.order_attack":
            if (!game.has_units_selected || !action_enabled("attack"))
                return false;
            game.cursor_mode = "attack";
            return true;
        case "rts.order_move":
            if (!game.has_units_selected)
                return false;
            game.cursor_mode = "normal";
            return true;
        case "rts.order_patrol":
            if (!game.has_units_selected || !action_enabled("patrol"))
                return false;
            game.cursor_mode = "patrol";
            return true;
        case "rts.order_guard":
            if (!game.has_units_selected || !action_enabled("guard"))
                return false;
            game.cursor_mode = "guard";
            return true;
        case "rts.order_hold":
            if (!game.has_units_selected || !game.orders.hold)
                return false;
            game.orders.hold();
            return true;
        case "rts.order_formation":
            if (!game.has_units_selected || !game.placement.on_formation_command)
                return false;
            game.placement.on_formation_command();
            return true;
        case "rts.camera_pan_up":
            begin_pan_action(actionId, event);
            game.camera.move(0, inputStep);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_down":
            begin_pan_action(actionId, event);
            game.camera.move(0, -inputStep);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_left":
            begin_pan_action(actionId, event);
            game.camera.move(-inputStep, 0);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_pan_right":
            begin_pan_action(actionId, event);
            game.camera.move(inputStep, 0);
            ensure_pan_timer_running();
            return true;
        case "rts.camera_rotate_left":
            game.camera.yaw(-yawStep);
            return true;
        case "rts.camera_rotate_right":
            game.camera.yaw(yawStep);
            return true;
        case "rts.camera_tilt_up":
            game.camera.tilt(1, shiftHeld);
            return true;
        case "rts.camera_tilt_down":
            game.camera.tilt(-1, shiftHeld);
            return true;
        case "rts.camera_zoom_in":
            game.camera.zoom(zoomStep);
            return true;
        case "rts.camera_zoom_out":
            game.camera.zoom(-zoomStep);
            return true;
        case "rts.camera_reset":
            if (!game.camera.reset)
                return false;
            game.camera.reset();
            return true;
        case "rts.commander_rally":
            if (!action_enabled("rally") || !game.commander.start_flag_rally)
                return false;
            game.commander.start_flag_rally();
            return true;
        case "rts.select_all_troops":
            if (!game.orders.select_all_troops)
                return false;
            game.orders.select_all_troops();
            return true;
        }
        return false;
    }

    function perform_commander_action(actionId, event) {
        var canonical = InputBindings.canonical_key_for(actionId);
        if (canonical !== 0 && game.commander.key_down) {
            switch (actionId) {
            case "commander.move_forward":
            case "commander.move_back":
            case "commander.strafe_left":
            case "commander.strafe_right":
            case "commander.turn_left":
            case "commander.turn_right":
            case "commander.sprint":
                pressed_keys[event.key] = actionId;
                game.commander.key_down(canonical, event.modifiers);
                return true;
            }
        }
        if (event.isAutoRepeat && actionId !== "commander.dodge")
            return true;
        switch (actionId) {
        case "commander.dodge":
            if (game.commander.dodge)
                game.commander.dodge();
            return true;
        case "commander.jump":
            if (game.commander.jump)
                game.commander.jump();
            return true;
        case "commander.cycle_lock_on":
            if (game.commander.cycle_lock_on)
                game.commander.cycle_lock_on();
            return true;
        case "commander.ability_vanguard_rush":
            if (game.commander.vanguard_rush)
                game.commander.vanguard_rush();
            return true;
        case "commander.ability_second_wind":
            if (game.commander.second_wind)
                game.commander.second_wind();
            return true;
        case "commander.ability_aura":
            if (game.commander.trigger_aura)
                game.commander.trigger_aura();
            return true;
        case "commander.special_action":
            if (game.commander.special_action)
                game.commander.special_action();
            return true;
        case "commander.rally":
            if (game.commander.trigger_rally)
                game.commander.trigger_rally();
            return true;
        case "commander.toggle_weapon":
            if (game.commander && game.commander.toggle_weapon_stance)
                game.commander.toggle_weapon_stance();
            return true;
        case "commander.toggle_camera_mode":
            if (game.commander.toggle_camera_mode)
                game.commander.toggle_camera_mode();
            return true;
        }
        return false;
    }

    function dispatch_key_action(actionId, event) {
        if (actionId.indexOf("global.") === 0)
            return perform_global_action(actionId, event);
        if (is_commander_mode())
            return perform_commander_action(actionId, event);
        return perform_rts_action(actionId, event);
    }

    objectName: "GameView"
    focus: true
    Keys.onPressed: function (event) {
        if (typeof game === 'undefined')
            return;
        if (game.placement.is_placing_formation && event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
            game_view.select_formation_intent_slot(event.key - Qt.Key_1);
            event.accepted = true;
            return;
        }
        var candidates = InputBindings.actions_for_key(event.key, event.modifiers, input_context());
        for (var i = 0; i < candidates.length; ++i) {
            if (dispatch_key_action(candidates[i], event)) {
                event.accepted = true;
                return;
            }
        }
    }
    Keys.onReleased: function (event) {
        if (typeof game === 'undefined')
            return;
        var actionId = pressed_keys[event.key];
        if (actionId === undefined)
            return;
        delete pressed_keys[event.key];
        if (actionId.indexOf("rts.camera_pan_") === 0) {
            end_pan_action(actionId);
            event.accepted = true;
            return;
        }
        var canonical = InputBindings.canonical_key_for(actionId);
        if (canonical !== 0 && game.commander.key_up) {
            game.commander.key_up(canonical, event.modifiers);
            event.accepted = true;
        }
    }

    GLView {
        id: renderArea

        property int key_pan_count: 0
        property bool mouse_pan_active: false

        anchors.fill: parent
        engine: game
        focus: false
        Component.onCompleted: {
            if (typeof game !== 'undefined' && game.cursor_mode)
                game_view.cursor_mode = game.cursor_mode;
            if (typeof game !== 'undefined')
                game_view.forceActiveFocus();
        }

        Connections {
            function onCursor_mode_changed() {
                if (typeof game !== 'undefined' && game.cursor_mode)
                    game_view.cursor_mode = game.cursor_mode;
            }

            function onControl_mode_changed() {
                if (typeof game === 'undefined')
                    return;
                if (game.commander.control_mode === "commander") {
                    mouseArea.is_selecting = false;
                    selectionBox.visible = false;
                    if (game_view.cursor_mode === "place_barracks_rally" && game.commander.cancel_barracks_rally)
                        game.commander.cancel_barracks_rally();
                    game_view.forceActiveFocus();
                    commanderLayer.center_mouse();
                } else {
                    renderArea.mouse_pan_active = false;
                    game_view.forceActiveFocus();
                }
            }

            target: game
        }

        Connections {
            function onPlacing_formation_changed() {
                if (typeof game !== 'undefined') {
                    game_view.is_placing_formation = game.placement.is_placing_formation;
                    if (!game.placement.is_placing_formation)
                        mouseArea.is_selecting = false;
                }
            }

            function onPlacing_construction_changed() {
                if (typeof game !== 'undefined')
                    game_view.is_placing_construction = game.placement.is_placing_construction;
            }

            target: typeof game !== 'undefined' ? game.placement : null
        }

        MouseArea {
            id: mouseArea

            property bool is_selecting: false
            property real start_x: 0
            property real start_y: 0
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            propagateComposedEvents: true
            preventStealing: true
            cursorShape: game_view.is_rally_placement() ? Qt.CrossCursor : (game_view.cursor_mode !== "normal" ? Qt.BlankCursor : Qt.ArrowCursor)
            enabled: game_view.visible && typeof game !== 'undefined' && (game.commander.control_mode !== "commander" || game_view.is_rally_placement())
            onEntered: {
                if (typeof game !== 'undefined' && game.orders.set_hover_at_screen)
                    game.orders.set_hover_at_screen(0, 0);
            }
            onExited: {
                if (typeof game !== 'undefined' && game.orders.set_hover_at_screen)
                    game.orders.set_hover_at_screen(-1, -1);
            }
            onPositionChanged: function (mouse) {
                if (is_selecting) {
                    var endX = mouse.x;
                    var endY = mouse.y;
                    selectionBox.x = Math.min(start_x, endX);
                    selectionBox.y = Math.min(start_y, endY);
                    selectionBox.width = Math.abs(endX - start_x);
                    selectionBox.height = Math.abs(endY - start_y);
                } else {
                    if (typeof game !== 'undefined' && game.orders.set_hover_at_screen)
                        game.orders.set_hover_at_screen(mouse.x, mouse.y);
                    if ((mouse.buttons & Qt.RightButton) && typeof game !== 'undefined' && game.orders.on_right_move) {
                        if (!game_view.is_rally_placement())
                            game.orders.on_right_move(mouse.x, mouse.y);
                    } else if (game_view.is_placing_formation) {
                        if (typeof game !== 'undefined' && game.placement.is_dragging_formation && game.placement.is_dragging_formation())
                            game.placement.on_formation_drag_update(mouse.x, mouse.y);
                        else if (typeof game !== 'undefined' && game.placement.on_formation_mouse_move)
                            game.placement.on_formation_mouse_move(mouse.x, mouse.y);
                    }
                    if (game_view.is_placing_construction) {
                        if (typeof game !== 'undefined' && game.placement.on_construction_mouse_move)
                            game.placement.on_construction_mouse_move(mouse.x, mouse.y);
                    }
                }
            }
            onWheel: function (w) {
                var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120);
                if (typeof game !== 'undefined' && game.placement.construction_preview_rotatable && game.placement.construction_preview_rotatable()) {
                    if (game.placement.on_construction_scroll)
                        game.placement.on_construction_scroll(dy);
                    w.accepted = true;
                    return;
                }
                if (typeof game !== 'undefined' && game.placement.is_placing_formation) {
                    if ((w.modifiers & Qt.ControlModifier) && game.placement.adjust_formation_depth)
                        game.placement.adjust_formation_depth(dy);
                    else if (game.placement.on_formation_scroll)
                        game.placement.on_formation_scroll(dy);
                    w.accepted = true;
                    return;
                }
                if (dy !== 0 && typeof game !== 'undefined' && game.camera.zoom)
                    game.camera.zoom(dy * 0.8);
                w.accepted = true;
            }
            onPressed: function (mouse) {
                game_view.forceActiveFocus();
                if (mouse.button === Qt.LeftButton) {
                    if (game_view.cursor_mode === "attack") {
                        if (typeof game !== 'undefined' && game.orders.attack_at)
                            game.orders.attack_at(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "guard") {
                        if (typeof game !== 'undefined' && game.orders.guard_at)
                            game.orders.guard_at(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "deliver") {
                        if (typeof game !== 'undefined' && game.orders.deliver_civilians_at)
                            game.orders.deliver_civilians_at(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "repair") {
                        if (typeof game !== 'undefined' && game.activity)
                            game.activity.confirm_repair_at(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "patrol") {
                        if (typeof game !== 'undefined' && game.orders.patrol_at)
                            game.orders.patrol_at(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "place_building") {
                        if (typeof game !== 'undefined' && game.placement.is_placing_construction && game.placement.on_construction_pointer_pressed)
                            game.placement.on_construction_pointer_pressed(mouse.x, mouse.y);
                        else if (typeof game !== 'undefined' && game.placement.place_building_at_screen)
                            game.placement.place_building_at_screen(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "place_commander_rally") {
                        if (typeof game !== 'undefined' && game.commander.confirm_flag_rally)
                            game.commander.confirm_flag_rally(mouse.x, mouse.y);
                        return;
                    }
                    if (game_view.cursor_mode === "place_barracks_rally") {
                        if (typeof game !== 'undefined' && game.commander.confirm_barracks_rally)
                            game.commander.confirm_barracks_rally(mouse.x, mouse.y);
                        return;
                    }
                    if (typeof game !== 'undefined' && game.placement.is_placing_formation) {
                        if (game.placement.on_formation_drag_begin)
                            game.placement.on_formation_drag_begin(mouse.x, mouse.y);
                        return;
                    }
                    if (typeof game !== 'undefined' && game.placement.is_placing_construction) {
                        if (game.placement.on_construction_pointer_pressed)
                            game.placement.on_construction_pointer_pressed(mouse.x, mouse.y);
                        return;
                    }
                    is_selecting = true;
                    start_x = mouse.x;
                    start_y = mouse.y;
                    selectionBox.x = start_x;
                    selectionBox.y = start_y;
                    selectionBox.width = 0;
                    selectionBox.height = 0;
                    selectionBox.visible = true;
                } else if (mouse.button === Qt.RightButton) {
                    if (game_view.is_rally_placement()) {
                        game_view.cancel_rally_placement();
                        return;
                    }
                    if (typeof game !== 'undefined' && game.placement.is_placing_formation && game.placement.on_formation_cancel) {
                        game.placement.on_formation_cancel();
                        return;
                    }
                    renderArea.mouse_pan_active = true;
                    mainWindow.edge_scroll_disabled = true;
                    if (typeof game !== 'undefined' && game.orders.on_right_press)
                        game.orders.on_right_press(mouse.x, mouse.y);
                }
            }
            onDoubleClicked: function (mouse) {
                if (mouse.button === Qt.RightButton) {
                    if (typeof game !== 'undefined' && game.orders.on_right_double_click)
                        game.orders.on_right_double_click(mouse.x, mouse.y);
                }
            }
            onReleased: function (mouse) {
                if (mouse.button === Qt.LeftButton && typeof game !== 'undefined' && game.placement.is_placing_formation) {
                    if (game.placement.is_dragging_formation && game.placement.is_dragging_formation())
                        game.placement.on_formation_drag_end();
                    if (game.placement.on_formation_confirm)
                        game.placement.on_formation_confirm();
                    return;
                }
                if (mouse.button === Qt.LeftButton && is_selecting) {
                    is_selecting = false;
                    selectionBox.visible = false;
                    if (selectionBox.width > 5 && selectionBox.height > 5) {
                        area_selected(selectionBox.x, selectionBox.y, selectionBox.x + selectionBox.width, selectionBox.y + selectionBox.height);
                        if (typeof game !== 'undefined' && game.orders.on_area_selected)
                            game.orders.on_area_selected(selectionBox.x, selectionBox.y, selectionBox.x + selectionBox.width, selectionBox.y + selectionBox.height, false);
                    } else {
                        map_clicked(mouse.x, mouse.y);
                        if (typeof game !== 'undefined' && game.orders.on_click_select)
                            game.orders.on_click_select(mouse.x, mouse.y, false);
                    }
                } else if (mouse.button === Qt.LeftButton && typeof game !== 'undefined' && game.placement.is_placing_construction) {
                    if (game.placement.on_construction_pointer_released)
                        game.placement.on_construction_pointer_released(mouse.x, mouse.y);
                }
                if (mouse.button === Qt.RightButton) {
                    if (typeof game !== 'undefined' && game.orders.on_right_release)
                        game.orders.on_right_release(mouse.x, mouse.y);
                    renderArea.mouse_pan_active = false;
                }
            }
            onCanceled: {
                is_selecting = false;
                selectionBox.visible = false;
                renderArea.mouse_pan_active = false;
            }
        }
    }

    CommanderInputLayer {
        id: commanderLayer

        anchors.fill: parent
        active: game_view.visible && typeof game !== 'undefined' && game.commander.control_mode === "commander" && !game_view.is_rally_placement()
        commander: typeof game !== 'undefined' ? game.commander : null
        gameView: game_view
        mainWindowRef: mainWindow
        onInputCaptured: {
            mouseArea.is_selecting = false;
            selectionBox.visible = false;
        }
    }

    Rectangle {
        id: selectionBox

        visible: false
        border.color: "white"
        border.width: 1
        color: "transparent"
    }

    Item {
        id: commanderReticle

        visible: typeof game !== 'undefined' && game.commander.control_mode === "commander" && game.commander.game_mode !== "rpg" && !game_view.is_rally_placement()
        width: 22
        height: 22
        anchors.centerIn: parent
        z: 999998

        Rectangle {
            width: 8
            height: 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: 4
            color: "#F4E6C0"
        }

        Rectangle {
            width: 8
            height: 2
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: 4
            color: "#F4E6C0"
        }

        Rectangle {
            width: 2
            height: 8
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.verticalCenter
            anchors.bottomMargin: 4
            color: "#F4E6C0"
        }

        Rectangle {
            width: 2
            height: 8
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.verticalCenter
            anchors.topMargin: 4
            color: "#F4E6C0"
        }
    }

    Item {
        id: customCursorContainer

        visible: game_view.cursor_mode !== "normal"
        width: 32
        height: 32
        z: 999999
        x: (typeof game !== 'undefined' && game.global_cursor_x) ? game.global_cursor_x - 16 : 0
        y: (typeof game !== 'undefined' && game.global_cursor_y) ? game.global_cursor_y - 16 : 0

        Item {
            id: attackCursorContainer

            property real pulse_scale: 1

            visible: game_view.cursor_mode === "attack"
            anchors.fill: parent

            Canvas {
                id: attackCursor

                anchors.fill: parent
                scale: attackCursorContainer.pulse_scale
                transformOrigin: Item.Center
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = Theme.accentBright;
                    ctx.lineWidth = 3;
                    ctx.beginPath();
                    ctx.moveTo(16, 4);
                    ctx.lineTo(16, 28);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(4, 16);
                    ctx.lineTo(28, 16);
                    ctx.stroke();
                    ctx.fillStyle = Theme.accentBright;
                    ctx.beginPath();
                    ctx.arc(16, 16, 4, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.strokeStyle = Qt.rgba(0.75, 0.27, 0.18, 0.5);
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.arc(16, 16, 7, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.strokeStyle = Theme.accentBright;
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(8, 12);
                    ctx.lineTo(8, 8);
                    ctx.lineTo(12, 8);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(20, 8);
                    ctx.lineTo(24, 8);
                    ctx.lineTo(24, 12);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(8, 20);
                    ctx.lineTo(8, 24);
                    ctx.lineTo(12, 24);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(20, 24);
                    ctx.lineTo(24, 24);
                    ctx.lineTo(24, 20);
                    ctx.stroke();
                }
                Component.onCompleted: requestPaint()
            }

            SequentialAnimation on pulse_scale  {
                running: attackCursorContainer.visible
                loops: Animation.Infinite

                NumberAnimation {
                    from: 1
                    to: 1.2
                    duration: 400
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    from: 1.2
                    to: 1
                    duration: 400
                    easing.type: Easing.InOutQuad
                }
            }
        }

        Canvas {
            id: guardCursor

            visible: game_view.cursor_mode === "guard"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.fillStyle = Theme.thumbBr;
                ctx.strokeStyle = Theme.panelBr;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(16, 6);
                ctx.lineTo(24, 10);
                ctx.lineTo(24, 18);
                ctx.lineTo(16, 26);
                ctx.lineTo(8, 18);
                ctx.lineTo(8, 10);
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
                ctx.strokeStyle = Theme.textMain;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(13, 16);
                ctx.lineTo(15, 18);
                ctx.lineTo(19, 12);
                ctx.stroke();
            }
            Component.onCompleted: requestPaint()
        }

        Canvas {
            id: patrolCursor

            visible: game_view.cursor_mode === "patrol"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = Theme.accent;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.arc(16, 16, 10, 0, Math.PI * 2);
                ctx.stroke();
                ctx.fillStyle = Theme.accent;
                ctx.beginPath();
                ctx.moveTo(26, 16);
                ctx.lineTo(22, 13);
                ctx.lineTo(22, 19);
                ctx.closePath();
                ctx.fill();
                ctx.beginPath();
                ctx.moveTo(6, 16);
                ctx.lineTo(10, 13);
                ctx.lineTo(10, 19);
                ctx.closePath();
                ctx.fill();
                ctx.beginPath();
                ctx.arc(16, 16, 3, 0, Math.PI * 2);
                ctx.fill();
            }
            Component.onCompleted: requestPaint()
        }

        Canvas {
            id: deliverCursor

            visible: game_view.cursor_mode === "deliver"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = Theme.accentBright;
                ctx.fillStyle = StyleGuide.historical.wax;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.arc(16, 16, 11, 0, Math.PI * 2);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(9, 16);
                ctx.lineTo(23, 16);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(16, 9);
                ctx.lineTo(16, 23);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(24, 16);
                ctx.lineTo(18, 11);
                ctx.lineTo(18, 21);
                ctx.closePath();
                ctx.fill();
            }
            Component.onCompleted: requestPaint()
        }

        Item {
            id: rallyPlacementCursor

            visible: game_view.is_rally_placement()
            anchors.fill: parent

            Image {
                anchors.centerIn: parent
                width: 28
                height: 28
                source: StyleGuide.icon_path("rally_mode.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }
        }

        Canvas {
            id: constructionCursor

            visible: game_view.is_placing_construction && game_view.cursor_mode !== "collect"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                var active = game_view.construction_preview_active;
                var ok = active && game_view.construction_preview_valid;
                var primary = !active ? "#D8C17A" : (ok ? "#75D36B" : "#D36060");
                var secondary = !active ? "#51401A" : (ok ? "#163A16" : "#4A1717");
                ctx.strokeStyle = primary;
                ctx.lineWidth = 2.5;
                ctx.beginPath();
                ctx.arc(16, 16, 10, 0, Math.PI * 2);
                ctx.stroke();
                ctx.fillStyle = Qt.rgba(0, 0, 0, 0.25);
                ctx.beginPath();
                ctx.arc(16, 16, 6, 0, Math.PI * 2);
                ctx.fill();
                ctx.strokeStyle = secondary;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(16, 5);
                ctx.lineTo(16, 11);
                ctx.moveTo(16, 21);
                ctx.lineTo(16, 27);
                ctx.moveTo(5, 16);
                ctx.lineTo(11, 16);
                ctx.moveTo(21, 16);
                ctx.lineTo(27, 16);
                ctx.stroke();
                ctx.strokeStyle = primary;
                ctx.lineWidth = 2.5;
                if (!active) {
                    ctx.beginPath();
                    ctx.moveTo(16, 10);
                    ctx.lineTo(16, 22);
                    ctx.moveTo(10, 16);
                    ctx.lineTo(22, 16);
                    ctx.stroke();
                } else if (ok) {
                    ctx.beginPath();
                    ctx.moveTo(11, 17);
                    ctx.lineTo(15, 21);
                    ctx.lineTo(22, 12);
                    ctx.stroke();
                } else {
                    ctx.beginPath();
                    ctx.moveTo(11, 11);
                    ctx.lineTo(21, 21);
                    ctx.moveTo(21, 11);
                    ctx.lineTo(11, 21);
                    ctx.stroke();
                }
            }
            Component.onCompleted: requestPaint()
        }

        Design.IronVectorIcon {
            id: repairCursor

            visible: game_view.cursor_mode === "repair"
            anchors.centerIn: parent
            width: 22
            height: 22
            iconId: "repair"
        }

        Item {
            id: collectCursorContainer

            visible: game_view.cursor_mode === "collect"
            anchors.fill: parent

            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                source: StyleGuide.icon_path("collect_mode.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                opacity: !game_view.construction_preview_active ? 0.9 : (game_view.construction_preview_valid ? 1 : 0.7)
            }

            Canvas {
                id: collectCursorOverlay

                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    var active = game_view.construction_preview_active;
                    var ok = active && game_view.construction_preview_valid;
                    var primary = !active ? "#D8C17A" : (ok ? "#75D36B" : "#D36060");
                    var secondary = !active ? "#51401A" : (ok ? "#163A16" : "#4A1717");
                    ctx.strokeStyle = primary;
                    ctx.lineWidth = 2.5;
                    ctx.beginPath();
                    ctx.arc(16, 16, 10, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.strokeStyle = secondary;
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(16, 5);
                    ctx.lineTo(16, 9);
                    ctx.moveTo(16, 23);
                    ctx.lineTo(16, 27);
                    ctx.moveTo(5, 16);
                    ctx.lineTo(9, 16);
                    ctx.moveTo(23, 16);
                    ctx.lineTo(27, 16);
                    ctx.stroke();
                    ctx.strokeStyle = primary;
                    ctx.lineWidth = 2.5;
                    ctx.fillStyle = primary;
                    ctx.beginPath();
                    ctx.arc(11, 11, 2, 0, Math.PI * 2);
                    ctx.arc(21, 11, 2, 0, Math.PI * 2);
                    ctx.arc(16, 22, 2, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.strokeStyle = primary;
                    ctx.lineWidth = 2.5;
                    if (!active) {
                        ctx.beginPath();
                        ctx.moveTo(16, 10);
                        ctx.lineTo(16, 22);
                        ctx.moveTo(10, 16);
                        ctx.lineTo(22, 16);
                        ctx.stroke();
                    } else if (ok) {
                        ctx.beginPath();
                        ctx.moveTo(11, 17);
                        ctx.lineTo(15, 21);
                        ctx.lineTo(22, 12);
                        ctx.stroke();
                    } else {
                        ctx.beginPath();
                        ctx.moveTo(11, 11);
                        ctx.lineTo(21, 21);
                        ctx.moveTo(21, 11);
                        ctx.lineTo(11, 21);
                        ctx.stroke();
                    }
                }
                Component.onCompleted: requestPaint()
            }
        }
    }

    Rectangle {
        visible: game_view.is_placing_construction && typeof game !== 'undefined' && game.placement.construction_preview_segment_count > 0
        z: 999999
        radius: 6
        color: "#B8141414"
        border.color: "#5FFFFFFF"
        border.width: 1
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.topMargin: 18
        width: previewSummaryText.implicitWidth + 18
        height: previewSummaryText.implicitHeight + 12

        Text {
            id: previewSummaryText

            anchors.centerIn: parent
            color: Theme.textMain
            text: qsTr("%1/%2 walls  •  %3 wood").arg(game.placement.construction_preview_valid_segment_count).arg(game.placement.construction_preview_segment_count).arg(game.placement.construction_preview_total_cost)
            font.pixelSize: Design.Typography.label
        }
    }

    Rectangle {
        id: orderFeedbackBanner

        property string kind: ""
        property bool accepted: true
        property string message: ""
        property real anchor_x: 0
        property real anchor_y: 0

        function glyph_for(kind, accepted) {
            if (!accepted)
                return "\u2298";
            switch (kind) {
            case "attack":
                return "\u2694";
            case "move":
                return "\u279C";
            case "guard":
                return "\u26E8";
            case "patrol":
                return "\u21BB";
            case "hold":
                return "\u23F8";
            case "stop":
                return "\u25A0";
            case "build":
                return "\u2692";
            case "gather":
                return "\u26CF";
            case "deliver":
                return "\u2302";
            case "repair":
                return "\u2692";
            case "rally":
                return "\u2691";
            case "formation":
                return "\u2637";
            }
            return "\u2713";
        }

        function show(kind, accepted, message) {
            if (!message || message.length === 0)
                return;
            orderFeedbackBanner.kind = kind;
            orderFeedbackBanner.accepted = accepted;
            orderFeedbackBanner.message = message;
            if (typeof game !== 'undefined') {
                orderFeedbackBanner.anchor_x = game.global_cursor_x;
                orderFeedbackBanner.anchor_y = game.global_cursor_y;
            }
            orderFeedbackBanner.opacity = 1;
            orderFeedbackDismiss.interval = accepted ? 1400 : 2600;
            orderFeedbackDismiss.restart();
        }

        visible: opacity > 0.01
        opacity: 0
        z: 999998
        radius: 5
        color: "#C8141414"
        border.color: accepted ? Theme.successBr : Theme.dangerBr
        border.width: 1
        width: orderFeedbackRow.implicitWidth + 16
        height: orderFeedbackRow.implicitHeight + 8
        x: Math.min(Math.max(0, anchor_x + 22), game_view.width - width)
        y: Math.min(Math.max(0, anchor_y + 44), game_view.height - height)

        Behavior on opacity  {
            NumberAnimation {
                duration: 220
            }
        }

        Timer {
            id: orderFeedbackDismiss

            interval: 1400
            repeat: false
            onTriggered: orderFeedbackBanner.opacity = 0
        }

        Row {
            id: orderFeedbackRow

            anchors.centerIn: parent
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: orderFeedbackBanner.accepted ? Theme.successText : Theme.dangerBr
                text: orderFeedbackBanner.glyph_for(orderFeedbackBanner.kind, orderFeedbackBanner.accepted)
                font.pixelSize: Design.Typography.label
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: orderFeedbackBanner.accepted ? Theme.textMain : Theme.warningText
                text: orderFeedbackBanner.message
                font.pixelSize: Design.Typography.caption
            }
        }
    }

    Connections {
        function onOrder_feedback(kind, accepted, message) {
            orderFeedbackBanner.show(kind, accepted, message);
        }

        target: typeof game !== 'undefined' ? game : null
    }

    Rectangle {
        id: attackTargetHint

        readonly property var hint: (typeof game !== 'undefined' && game.activity && game.activity.attack_target_hint) ? game.activity.attack_target_hint : ({
                "state": "none",
                "name": "",
                "range": "none"
            })
        readonly property bool attackable: hint.state === "valid"
        readonly property bool in_range: hint.range === "in_range"

        function hint_text() {
            switch (hint.state) {
            case "valid":
                return hint.name && hint.name.length > 0 ? hint.name : qsTr("Enemy target");
            case "ally":
                return qsTr("Cannot attack ally");
            case "neutral":
                return qsTr("Cannot attack this target");
            case "no_attackers":
                return qsTr("Selection cannot attack");
            }
            return "";
        }

        function range_text() {
            switch (hint.range) {
            case "in_range":
                return qsTr("In range");
            case "too_close":
                return qsTr("Too close");
            case "out_of_range":
                return qsTr("Out of range");
            case "blocked":
                return qsTr("No firing line");
            }
            return "";
        }

        function range_glyph() {
            switch (hint.range) {
            case "in_range":
                return "\u25CE";
            case "too_close":
                return "\u25B3";
            case "out_of_range":
                return "\u25CB";
            case "blocked":
                return "\u2715";
            }
            return "";
        }

        visible: game_view.cursor_mode === "attack" && hint.state !== "none"
        z: 999999
        radius: 5
        color: "#C8141414"
        border.color: attackable ? Theme.dangerBr : Theme.panelBr
        border.width: 1
        width: attackTargetHintRow.implicitWidth + 14
        height: attackTargetHintRow.implicitHeight + 8
        x: typeof game !== 'undefined' ? Math.min(Math.max(0, game.global_cursor_x + 22), game_view.width - width) : 0
        y: typeof game !== 'undefined' ? Math.min(Math.max(0, game.global_cursor_y + 20), game_view.height - height) : 0

        Row {
            id: attackTargetHintRow

            anchors.centerIn: parent
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: attackTargetHint.attackable ? Theme.dangerBr : Theme.textDim
                text: attackTargetHint.attackable ? "⚔" : "⊘"
                font.pixelSize: Design.Typography.label
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: attackTargetHint.attackable ? Theme.textMain : Theme.textDim
                text: attackTargetHint.hint_text()
                font.pixelSize: Design.Typography.caption
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: attackTargetHint.range_text().length > 0
                color: attackTargetHint.in_range ? Theme.successText : Theme.warningText
                text: attackTargetHint.range_glyph() + " " + attackTargetHint.range_text()
                font.pixelSize: Design.Typography.caption
            }
        }
    }

    Timer {
        id: keyPanTimer

        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (typeof game === 'undefined')
                return;
            var step = (Qt.inputModifiers & Qt.ShiftModifier) ? 2 : 1;
            var dx = 0;
            var dz = 0;
            if (game_view.pan_axis["rts.camera_pan_up"])
                dz += step;
            if (game_view.pan_axis["rts.camera_pan_down"])
                dz -= step;
            if (game_view.pan_axis["rts.camera_pan_left"])
                dx -= step;
            if (game_view.pan_axis["rts.camera_pan_right"])
                dx += step;
            if (dx !== 0 || dz !== 0)
                game.camera.move(dx, dz);
        }
    }
}
