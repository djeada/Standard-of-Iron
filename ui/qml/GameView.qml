import QtQuick 2.15
import QtQuick.Controls 2.15
import StandardOfIron 1.0

Item {
    id: gameView

    property bool isPaused: false
    property real gameSpeed: 1
    property bool setRallyMode: false
    property string cursorMode: "normal"
    property var pressedKeys: ({
    })

    signal mapClicked(real x, real y)
    signal unitSelected(int unitId)
    signal areaSelected(real x1, real y1, real x2, real y2)

    function setPaused(paused) {
        isPaused = paused;
        if (typeof game !== 'undefined' && game.setPaused)
            game.setPaused(paused);

    }

    function setGameSpeed(speed) {
        gameSpeed = speed;
        if (typeof game !== 'undefined' && game.setGameSpeed)
            game.setGameSpeed(speed);

    }

    function issueCommand(command) {
        console.log("Command issued:", command);
    }

    function beginPanKey(e) {
        if (!e.isAutoRepeat && !pressedKeys[e.key]) {
            pressedKeys[e.key] = true;
            renderArea.keyPanCount += 1;
            mainWindow.edgeScrollDisabled = true;
        }
    }

    function ensurePanTimerRunning() {
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
            if (typeof mainWindow !== 'undefined' && !mainWindow.menuVisible) {
                mainWindow.menuVisible = true;
                event.accepted = true;
            }
            break;
        case Qt.Key_Space:
            if (typeof mainWindow !== 'undefined') {
                mainWindow.gamePaused = !mainWindow.gamePaused;
                gameView.setPaused(mainWindow.gamePaused);
                event.accepted = true;
            }
            break;
        case Qt.Key_S:
            if (game.hasUnitsSelected && !shiftHeld) {
                if (game.onStopCommand)
                    game.onStopCommand();

                event.accepted = true;
            } else {
                beginPanKey(event);
                game.cameraMove(0, -inputStep);
                ensurePanTimerRunning();
                event.accepted = true;
            }
            break;
        case Qt.Key_A:
            if (game.hasUnitsSelected && !shiftHeld) {
                game.cursorMode = "attack";
                event.accepted = true;
            } else {
                beginPanKey(event);
                game.cameraMove(-inputStep, 0);
                ensurePanTimerRunning();
                event.accepted = true;
            }
            break;
        case Qt.Key_D:
            beginPanKey(event);
            game.cameraMove(inputStep, 0);
            ensurePanTimerRunning();
            event.accepted = true;
            break;
        case Qt.Key_M:
            if (game.hasUnitsSelected) {
                game.cursorMode = "normal";
                event.accepted = true;
            }
            break;
        case Qt.Key_Up:
            beginPanKey(event);
            game.cameraMove(0, inputStep);
            ensurePanTimerRunning();
            event.accepted = true;
            break;
        case Qt.Key_Down:
            beginPanKey(event);
            game.cameraMove(0, -inputStep);
            ensurePanTimerRunning();
            event.accepted = true;
            break;
        case Qt.Key_Left:
            beginPanKey(event);
            game.cameraMove(-inputStep, 0);
            ensurePanTimerRunning();
            event.accepted = true;
            break;
        case Qt.Key_Right:
            beginPanKey(event);
            game.cameraMove(inputStep, 0);
            ensurePanTimerRunning();
            event.accepted = true;
            break;
        case Qt.Key_Q:
            game.cameraYaw(-yawStep);
            event.accepted = true;
            break;
        case Qt.Key_E:
            game.cameraYaw(yawStep);
            event.accepted = true;
            break;
        case Qt.Key_R:
            game.cameraOrbitDirection(1, shiftHeld);
            event.accepted = true;
            break;
        case Qt.Key_F:
            game.cameraOrbitDirection(-1, shiftHeld);
            event.accepted = true;
            break;
        case Qt.Key_X:
            if (game.selectAllTroops)
                game.selectAllTroops();

            event.accepted = true;
            break;
        case Qt.Key_P:
            if (game.hasUnitsSelected) {
                game.cursorMode = "patrol";
                event.accepted = true;
            }
            break;
        case Qt.Key_H:
            if (game.hasUnitsSelected && game.onHoldCommand) {
                game.onHoldCommand();
                event.accepted = true;
            }
            break;
        }
    }
    Keys.onReleased: function(event) {
        if (typeof game === 'undefined')
            return ;

        var movementKeys = [Qt.Key_A, Qt.Key_S, Qt.Key_D, Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right];
        if (movementKeys.indexOf(event.key) !== -1) {
            if (pressedKeys[event.key]) {
                pressedKeys[event.key] = false;
                renderArea.keyPanCount = Math.max(0, renderArea.keyPanCount - 1);
            }
            var anyHeld = false;
            for (var k in pressedKeys) {
                if (pressedKeys[k]) {
                    anyHeld = true;
                    break;
                }
            }
            if (!anyHeld) {
                if (keyPanTimer.running)
                    keyPanTimer.stop();

                if (renderArea.keyPanCount === 0 && !renderArea.mousePanActive)
                    mainWindow.edgeScrollDisabled = false;

            }
        }
        if (event.key === Qt.Key_Shift) {
            if (renderArea.keyPanCount === 0 && !renderArea.mousePanActive)
                mainWindow.edgeScrollDisabled = false;

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
            if (pressedKeys[Qt.Key_Up])
                dz += step;

            if (pressedKeys[Qt.Key_S] || pressedKeys[Qt.Key_Down])
                dz -= step;

            if (pressedKeys[Qt.Key_A] || pressedKeys[Qt.Key_Left])
                dx -= step;

            if (pressedKeys[Qt.Key_D] || pressedKeys[Qt.Key_Right])
                dx += step;

            if (dx !== 0 || dz !== 0)
                game.cameraMove(dx, dz);

        }
    }

    GLView {
        id: renderArea

        property int keyPanCount: 0
        property bool mousePanActive: false

        anchors.fill: parent
        engine: game
        focus: false
        Component.onCompleted: {
            if (typeof game !== 'undefined' && game.cursorMode)
                gameView.cursorMode = game.cursorMode;

        }

        Connections {
            function onCursorModeChanged() {
                if (typeof game !== 'undefined' && game.cursorMode)
                    gameView.cursorMode = game.cursorMode;

            }

            target: game
        }

        MouseArea {
            id: mouseArea

            property bool isSelecting: false
            property real startX: 0
            property real startY: 0

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            propagateComposedEvents: true
            preventStealing: true
            cursorShape: (gameView.cursorMode === "normal") ? Qt.ArrowCursor : Qt.BlankCursor
            enabled: gameView.visible
            onEntered: {
                if (typeof game !== 'undefined' && game.setHoverAtScreen)
                    game.setHoverAtScreen(0, 0);

            }
            onExited: {
                if (typeof game !== 'undefined' && game.setHoverAtScreen)
                    game.setHoverAtScreen(-1, -1);

            }
            onPositionChanged: function(mouse) {
                if (isSelecting) {
                    var endX = mouse.x;
                    var endY = mouse.y;
                    selectionBox.x = Math.min(startX, endX);
                    selectionBox.y = Math.min(startY, endY);
                    selectionBox.width = Math.abs(endX - startX);
                    selectionBox.height = Math.abs(endY - startY);
                } else {
                    if (typeof game !== 'undefined' && game.setHoverAtScreen)
                        game.setHoverAtScreen(mouse.x, mouse.y);

                }
            }
            onWheel: function(w) {
                var dy = (w.angleDelta ? w.angleDelta.y / 120 : w.delta / 120);
                if (dy !== 0 && typeof game !== 'undefined' && game.cameraZoom)
                    game.cameraZoom(dy * 0.8);

                w.accepted = true;
            }
            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    if (gameView.setRallyMode) {
                        if (typeof game !== 'undefined' && game.setRallyAtScreen)
                            game.setRallyAtScreen(mouse.x, mouse.y);

                        gameView.setRallyMode = false;
                        return ;
                    }
                    if (gameView.cursorMode === "attack") {
                        if (typeof game !== 'undefined' && game.onAttackClick)
                            game.onAttackClick(mouse.x, mouse.y);

                        return ;
                    }
                    if (gameView.cursorMode === "guard")
                        return ;

                    if (gameView.cursorMode === "patrol") {
                        if (typeof game !== 'undefined' && game.onPatrolClick)
                            game.onPatrolClick(mouse.x, mouse.y);

                        return ;
                    }
                    isSelecting = true;
                    startX = mouse.x;
                    startY = mouse.y;
                    selectionBox.x = startX;
                    selectionBox.y = startY;
                    selectionBox.width = 0;
                    selectionBox.height = 0;
                    selectionBox.visible = true;
                } else if (mouse.button === Qt.RightButton) {
                    renderArea.mousePanActive = true;
                    mainWindow.edgeScrollDisabled = true;
                    if (gameView.setRallyMode)
                        gameView.setRallyMode = false;

                    if (typeof game !== 'undefined' && game.onRightClick)
                        game.onRightClick(mouse.x, mouse.y);

                }
            }
            onReleased: function(mouse) {
                if (mouse.button === Qt.LeftButton && isSelecting) {
                    isSelecting = false;
                    selectionBox.visible = false;
                    if (selectionBox.width > 5 && selectionBox.height > 5) {
                        areaSelected(selectionBox.x, selectionBox.y, selectionBox.x + selectionBox.width, selectionBox.y + selectionBox.height);
                        if (typeof game !== 'undefined' && game.onAreaSelected)
                            game.onAreaSelected(selectionBox.x, selectionBox.y, selectionBox.x + selectionBox.width, selectionBox.y + selectionBox.height, false);

                    } else {
                        mapClicked(mouse.x, mouse.y);
                        if (typeof game !== 'undefined' && game.onClickSelect)
                            game.onClickSelect(mouse.x, mouse.y, false);

                    }
                }
                if (mouse.button === Qt.RightButton) {
                    renderArea.mousePanActive = false;
                    mainWindow.edgeScrollDisabled = (renderArea.keyPanCount > 0) || renderArea.mousePanActive;
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

        visible: gameView.cursorMode !== "normal"
        width: 32
        height: 32
        z: 999999
        x: (typeof game !== 'undefined' && game.globalCursorX) ? game.globalCursorX - 16 : 0
        y: (typeof game !== 'undefined' && game.globalCursorY) ? game.globalCursorY - 16 : 0

        Item {
            id: attackCursorContainer

            property real pulseScale: 1

            visible: gameView.cursorMode === "attack"
            anchors.fill: parent

            Canvas {
                id: attackCursor

                anchors.fill: parent
                scale: attackCursorContainer.pulseScale
                transformOrigin: Item.Center
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = "#ff4444";
                    ctx.lineWidth = 3;
                    ctx.beginPath();
                    ctx.moveTo(16, 4);
                    ctx.lineTo(16, 28);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(4, 16);
                    ctx.lineTo(28, 16);
                    ctx.stroke();
                    ctx.fillStyle = "#ff2222";
                    ctx.beginPath();
                    ctx.arc(16, 16, 4, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.strokeStyle = "rgba(255, 68, 68, 0.5)";
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.arc(16, 16, 7, 0, Math.PI * 2);
                    ctx.stroke();
                    ctx.strokeStyle = "#e74c3c";
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

            SequentialAnimation on pulseScale {
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

            visible: gameView.cursorMode === "guard"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.fillStyle = "#3498db";
                ctx.strokeStyle = "#2980b9";
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
                ctx.strokeStyle = "#ecf0f1";
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

            visible: gameView.cursorMode === "patrol"
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = "#27ae60";
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.arc(16, 16, 10, 0, Math.PI * 2);
                ctx.stroke();
                ctx.fillStyle = "#27ae60";
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
