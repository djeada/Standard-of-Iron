import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Item {
    id: game_view

    property bool is_paused: false
    property real game_speed: 1
    property bool set_rally_mode: false
    property string cursor_mode: "normal"
    property bool is_placing_formation: false
    property bool is_placing_construction: false
    property var pressed_keys: ({
    })

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

    function begin_pan_key(e) {
        if (!e.isAutoRepeat && !pressed_keys[e.key]) {
            pressed_keys[e.key] = true;
            renderArea.key_pan_count += 1;
            mainWindow.edge_scroll_disabled = true;
        }
    }

    function ensure_pan_timer_running() {
        if (!keyPanTimer.running)
            keyPanTimer.start();

    }

    objectName: "GameView"
    Keys.onPressed: function(event) {
        if (typeof game === 'undefined')
            return ;

        var yawStep = (event.modifiers & Qt.ShiftModifier) ? 8 : 4;
        var inputStep = (event.modifiers & Qt.ShiftModifier) ? 2 : 1;
        var shiftHeld = (event.modifiers & Qt.ShiftModifier) !== 0;
        switch (event.key) {
        case Qt.Key_Escape:
            if (typeof mainWindow !== 'undefined' && !mainWindow.menu_visible) {
                mainWindow.menu_visible = true;
                event.accepted = true;
            }
            break;
        case Qt.Key_Space:
            if (typeof mainWindow !== 'undefined') {
                mainWindow.game_paused = !mainWindow.game_paused;
                game_view.set_paused(mainWindow.game_paused);
                event.accepted = true;
            }
            break;
        case Qt.Key_S:
            if (game.has_units_selected) {
                if (game.on_stop_command)
                    game.on_stop_command();

                event.accepted = true;
            }
            break;
        case Qt.Key_A:
            if (game.has_units_selected) {
                game.cursor_mode = "attack";
                event.accepted = true;
            }
            break;
        case Qt.Key_M:
            if (game.has_units_selected) {
                game.cursor_mode = "normal";
                event.accepted = true;
            }
            break;
        case Qt.Key_Up:
            begin_pan_key(event);
            game.camera_move(0, inputStep);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Down:
            begin_pan_key(event);
            game.camera_move(0, -inputStep);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Left:
            begin_pan_key(event);
            game.camera_move(-inputStep, 0);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Right:
            begin_pan_key(event);
            game.camera_move(inputStep, 0);
            ensure_pan_timer_running();
            event.accepted = true;
            break;
        case Qt.Key_Q:
            game.camera_yaw(-yawStep);
            event.accepted = true;
            break;
        case Qt.Key_E:
            game.camera_yaw(yawStep);
            event.accepted = true;
            break;
        case Qt.Key_R:
            game.camera_orbit_direction(1, shiftHeld);
            event.accepted = true;
            break;
        case Qt.Key_F:
            game.camera_orbit_direction(-1, shiftHeld);
            event.accepted = true;
            break;
        case Qt.Key_X:
            if (game.select_all_troops)
                game.select_all_troops();

            event.accepted = true;
            break;
        case Qt.Key_P:
            if (game.has_units_selected) {
                game.cursor_mode = "patrol";
                event.accepted = true;
            }
            break;
        case Qt.Key_G:
            if (game.has_units_selected) {
                game.cursor_mode = "guard";
                event.accepted = true;
            }
            break;
        case Qt.Key_H:
            if (game.has_units_selected && game.on_hold_command) {
                game.on_hold_command();
                event.accepted = true;
            }
            break;
        }
    }
    Keys.onReleased: function(event) {
        if (typeof game === 'undefined')
            return ;

        var movementKeys = [Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right];
        if (movementKeys.indexOf(event.key) !== -1) {
            if (pressed_keys[event.key]) {
                pressed_keys[event.key] = false;
                renderArea.key_pan_count = Math.max(0, renderArea.key_pan_count - 1);
            }
            var anyHeld = false;
            for (var k in pressed_keys) {
                if (pressed_keys[k]) {
                    anyHeld = true;
                    break;
                }
            }
            if (!anyHeld) {
                if (keyPanTimer.running)
                    keyPanTimer.stop();

                if (renderArea.key_pan_count === 0 && !renderArea.mouse_pan_active)
                    mainWindow.edge_scroll_disabled = false;

            }
        }
        if (event.key === Qt.Key_Shift) {
            if (renderArea.key_pan_count === 0 && !renderArea.mouse_pan_active)
                mainWindow.edge_scroll_disabled = false;

        }
    }
    focus: true

    Timer {
        id: keyPanTimer

        interval: 16
        repeat: true
        running: false
        onTriggered: {
            if (typeof game === 'undefined')
                return ;

            var step = (Qt.inputModifiers & Qt.ShiftModifier) ? 2 : 1;
            var dx = 0;
            var dz = 0;
            if (pressed_keys[Qt.Key_Up])
                dz += step;

            if (pressed_keys[Qt.Key_Down])
                dz -= step;

            if (pressed_keys[Qt.Key_Left])
                dx -= step;

            if (pressed_keys[Qt.Key_Right])
                dx += step;

            if (dx !== 0 || dz !== 0)
                game.camera_move(dx, dz);

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

        }

        Connections {
            function onCursor_mode_changed() {
                if (typeof game !== 'undefined' && game.cursor_mode)
                    game_view.cursor_mode = game.cursor_mode;

            }

            function onPlacing_formation_changed() {
                if (typeof game !== 'undefined') {
                    game_view.is_placing_formation = game.is_placing_formation;
                    if (!game.is_placing_formation)
                        mouseArea.is_right_drag_orient = false;
                }
            }

            function onPlacing_construction_changed() {
                if (typeof game !== 'undefined')
                    game_view.is_placing_construction = game.is_placing_construction;

            }

            target: game
        }

        MouseArea {
            id: mouseArea

            property bool is_selecting: false
            property real start_x: 0
            property real start_y: 0
            property bool is_right_drag_orient: false

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            propagateComposedEvents: true
            preventStealing: true
            cursorShape: (game_view.cursor_mode === "normal") ? ((typeof game !== 'undefined' && game.civilian_delivery_available) ? Qt.PointingHandCursor : Qt.ArrowCursor) : Qt.BlankCursor
            enabled: game_view.visible
            onEntered: {
                if (typeof game !== 'undefined' && game.set_hover_at_screen)
                    game.set_hover_at_screen(0, 0);

            }
            onExited: {
                if (typeof game !== 'undefined' && game.set_hover_at_screen)
                    game.set_hover_at_screen(-1, -1);

            }
            onPositionChanged: function(mouse) {
                if (is_selecting) {
                    var endX = mouse.x;
                    var endY = mouse.y;
                    selectionBox.x = Math.min(start_x, endX);
                    selectionBox.y = Math.min(start_y, endY);
                    selectionBox.width = Math.abs(endX - start_x);
                    selectionBox.height = Math.abs(endY - start_y);
                } else {
                    if (typeof game !== 'undefined' && game.set_hover_at_screen)
                        game.set_hover_at_screen(mouse.x, mouse.y);

                    if (mouseArea.is_right_drag_orient && (mouse.buttons & Qt.RightButton)) {
                        if (typeof game !== 'undefined' && game.on_right_drag_orient)
                            game.on_right_drag_orient(mouse.x, mouse.y);

                    } else if (game_view.is_placing_formation) {
                        if (typeof game !== 'undefined' && game.on_formation_mouse_move)
                            game.on_formation_mouse_move(mouse.x, mouse.y);

                    }
                    if (game_view.is_placing_construction) {
                        if (typeof game !== 'undefined' && game.on_construction_mouse_move)
                            game.on_construction_mouse_move(mouse.x, mouse.y);

                    }
                }
            }
            onWheel: function(w) {
                var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120);
                if (typeof game !== 'undefined' && game.is_placing_formation) {
                    if (game.on_formation_scroll)
                        game.on_formation_scroll(dy);

                    w.accepted = true;
                    return ;
                }
                if (dy !== 0 && typeof game !== 'undefined' && game.camera_zoom)
                    game.camera_zoom(dy * 0.8);

                w.accepted = true;
            }
            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    if (game_view.set_rally_mode) {
                        if (typeof game !== 'undefined' && game.set_rally_at_screen)
                            game.set_rally_at_screen(mouse.x, mouse.y);

                        game_view.set_rally_mode = false;
                        return ;
                    }
                    if (game_view.cursor_mode === "attack") {
                        if (typeof game !== 'undefined' && game.on_attack_click)
                            game.on_attack_click(mouse.x, mouse.y);

                        return ;
                    }
                    if (game_view.cursor_mode === "guard") {
                        if (typeof game !== 'undefined' && game.on_guard_click)
                            game.on_guard_click(mouse.x, mouse.y);

                        return ;
                    }
                    if (game_view.cursor_mode === "patrol") {
                        if (typeof game !== 'undefined' && game.on_patrol_click)
                            game.on_patrol_click(mouse.x, mouse.y);

                        return ;
                    }
                    if (game_view.cursor_mode === "place_building") {
                        if (typeof game !== 'undefined' && game.place_building_at_screen)
                            game.place_building_at_screen(mouse.x, mouse.y);

                        return ;
                    }
                    if (typeof game !== 'undefined' && game.is_placing_formation) {
                        if (game.on_formation_confirm)
                            game.on_formation_confirm();

                        return ;
                    }
                    if (typeof game !== 'undefined' && game.is_placing_construction) {
                        if (game.on_construction_confirm)
                            game.on_construction_confirm();

                        return ;
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
                    if (typeof game !== 'undefined' && game.is_placing_formation) {
                        if (game.on_formation_cancel)
                            game.on_formation_cancel();

                        mouseArea.is_right_drag_orient = false;
                        return ;
                    }
                    if (typeof game !== 'undefined' && game.is_placing_construction) {
                        if (game.on_construction_cancel)
                            game.on_construction_cancel();

                        return ;
                    }
                    renderArea.mouse_pan_active = true;
                    mainWindow.edge_scroll_disabled = true;
                    if (game_view.set_rally_mode)
                        game_view.set_rally_mode = false;

                    mouseArea.is_right_drag_orient = true;
                    if (typeof game !== 'undefined' && game.on_right_press)
                        game.on_right_press(mouse.x, mouse.y);

                }
            }
            onDoubleClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    if (typeof game !== 'undefined' && game.on_right_double_click)
                        game.on_right_double_click(mouse.x, mouse.y);

                }
            }
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton && is_selecting) {
                    is_selecting = false;
                    selectionBox.visible = false;
                    if (selectionBox.width > 5 && selectionBox.height > 5) {
                        area_selected(selectionBox.x, selectionBox.y, selectionBox.x + selectionBox.width, selectionBox.y + selectionBox.height);
                        if (typeof game !== 'undefined' && game.on_area_selected)
                            game.on_area_selected(selectionBox.x, selectionBox.y, selectionBox.x + selectionBox.width, selectionBox.y + selectionBox.height, false);

                    } else {
                        map_clicked(mouse.x, mouse.y);
                        if (typeof game !== 'undefined' && game.on_click_select)
                            game.on_click_select(mouse.x, mouse.y, false);

                    }
                }
                if (mouse.button === Qt.RightButton) {
                    if (mouseArea.is_right_drag_orient) {
                        mouseArea.is_right_drag_orient = false;
                        if (typeof game !== 'undefined' && game.is_placing_formation) {
                            if (game.on_formation_confirm)
                                game.on_formation_confirm();

                        }
                    }
                    renderArea.mouse_pan_active = false;
                    mainWindow.edge_scroll_disabled = (renderArea.key_pan_count > 0) || renderArea.mouse_pan_active;
                }
            }
        }

        Rectangle {
            id: selectionBox

            visible: false
            border.color: "white"
            border.width: 1
            color: "transparent"
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

            SequentialAnimation on pulse_scale {
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

    }

}
