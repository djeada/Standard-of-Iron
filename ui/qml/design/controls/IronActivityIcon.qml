import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Item {
    id: root

    property string activity: Design.ActivityIcons.defaultActivity
    property string state_id: Design.ActivityIcons.defaultState

    property int count: 1
    property bool showLabel: false
    property bool interactive: true
    property real iconScale: 1.0

    readonly property var meta: Design.ActivityIcons.activity(root.activity)
    readonly property var stateMeta: Design.ActivityIcons.state(root.state_id)
    readonly property string summary: Design.ActivityIcons.summary(root.activity, root.state_id)
    readonly property string tooltipText: Design.ActivityIcons.description(root.activity, root.state_id, root.count)
    readonly property string resource: Design.ActivityIcons.resourceFor(root.activity)

    readonly property color stateTone: {
        switch (root.stateMeta.tone) {
        case "danger":
            return Design.Theme.danger;
        case "warning":
            return Design.Theme.warning;
        case "secondary":
            return Design.Theme.textSecondary;
        }
        return Design.Theme.accent;
    }

    readonly property real glyphSize: Math.round(Design.Metrics.iconMedium * root.iconScale)

    implicitWidth: layout.implicitWidth
    implicitHeight: Math.max(root.glyphSize + Design.Metrics.space8, layout.implicitHeight)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.summary
    Accessible.description: root.tooltipText

    ToolTip.text: root.tooltipText
    ToolTip.visible: root.interactive && hoverArea.containsMouse
    ToolTip.delay: Design.Metrics.tooltipDelay

    Row {
        id: layout

        x: 0
        y: 0
        spacing: Design.Metrics.space8

        Item {
            id: medallion

            width: root.glyphSize + Design.Metrics.space8
            height: width

            Rectangle {
                anchors.fill: parent
                radius: Design.Metrics.radiusSmall
                color: Design.Theme.backgroundDeep
                border.width: root.stateMeta.decorated ? Design.Metrics.borderFocus : Design.Metrics.borderThin
                border.color: root.stateTone
                opacity: root.state_id === "unavailable" ? 0.85 : 1
            }

            Design.IronVectorIcon {
                id: art

                anchors.centerIn: parent
                width: root.glyphSize
                height: root.glyphSize
                iconId: root.meta.icon
                accent: root.stateTone
                opacity: root.state_id === "unavailable" ? 0.55 : root.state_id === "queued" ? 0.8 : 1
            }

            Text {
                anchors.centerIn: parent
                visible: !art.available
                text: "●"
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Math.round(root.glyphSize * 0.6)
            }

            Loader {
                id: stateMark

                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: -Design.Metrics.space2
                active: root.stateMeta.decorated
                visible: active
                sourceComponent: stateMarkComponent
            }

            Rectangle {
                id: countBadge

                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: -Design.Metrics.space4
                visible: root.count > 1
                width: Math.max(Design.Metrics.space16, countText.implicitWidth + Design.Metrics.space8)
                height: Design.Metrics.space16
                radius: height / 2
                color: Design.Theme.panelLeather
                border.width: Design.Metrics.borderThin
                border.color: Design.Theme.accent

                Text {
                    id: countText

                    anchors.centerIn: parent
                    text: "×" + root.count
                    color: Design.Theme.accent
                    font.family: Design.Typography.family
                    font.pixelSize: Design.Typography.caption
                    font.weight: Design.Typography.bold
                }
            }
        }

        Column {
            visible: root.showLabel
            spacing: 0

            Text {
                text: Design.ActivityIcons.label(root.activity)
                color: Design.Theme.textPrimary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: Design.Typography.medium
                elide: Text.ElideRight
            }

            Text {
                visible: root.state_id !== Design.ActivityIcons.defaultState
                text: root.stateMeta.label
                color: root.stateTone
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: hoverArea

        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: root.interactive
    }

    Component {
        id: stateMarkComponent

        Rectangle {
            width: Design.Metrics.space12
            height: Design.Metrics.space12
            radius: width / 2
            color: Design.Theme.backgroundDeep
            border.width: Design.Metrics.borderThin
            border.color: root.stateTone

            Canvas {
                id: stateMarkCanvas

                anchors.fill: parent
                anchors.margins: 2.5
                antialiasing: true

                Connections {
                    function onState_idChanged() {
                        stateMarkCanvas.requestPaint();
                    }

                    target: root
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = root.stateTone;
                    ctx.fillStyle = root.stateTone;
                    ctx.lineWidth = Math.max(1, width * 0.22);
                    ctx.lineCap = "round";
                    if (root.state_id === "queued") {
                        ctx.beginPath();
                        ctx.moveTo(width * 0.2, 0);
                        ctx.lineTo(width, height * 0.5);
                        ctx.lineTo(width * 0.2, height);
                        ctx.closePath();
                        ctx.fill();
                    } else if (root.state_id === "interrupted") {
                        ctx.fillRect(width * 0.12, 0, width * 0.26, height);
                        ctx.fillRect(width * 0.62, 0, width * 0.26, height);
                    } else {
                        ctx.beginPath();
                        ctx.moveTo(0, 0);
                        ctx.lineTo(width, height);
                        ctx.moveTo(width, 0);
                        ctx.lineTo(0, height);
                        ctx.stroke();
                    }
                }
            }
        }
    }
}
