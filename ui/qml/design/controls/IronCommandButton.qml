import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

AbstractButton {
    id: control

    property string actionId: ""
    property string label: text
    property string hotkey: ""

    property string vectorIcon: control.actionId
    property url iconSource: control.actionId ? Design.Icons.command(control.actionId) : ""
    property string glyph: control.actionId ? Design.Icons.commandGlyph(control.actionId) : ""

    property bool active: false

    property bool mixed: false

    property bool placing: false

    property int eligibleCount: 0
    property int activeCount: 0

    property real cooldown: 0
    property string disabledReason: ""
    property string hint: ""

    property bool blocked: false
    readonly property bool interactive: enabled && !blocked

    property bool compact: width > 0 && width < minimumLabelledWidth
    readonly property real minimumLabelledWidth: Design.Metrics.iconMedium + labelMetrics.width + hotkeyLabel.width + Design.Metrics.space24

    TextMetrics {
        id: labelMetrics

        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        font.weight: Design.Typography.medium
        text: control.label
    }

    readonly property bool highlighted: active || placing
    readonly property bool showFocusRing: visualFocus || (Design.A11y.alwaysShowFocus && activeFocus)
    readonly property color stateColor: !interactive ? Design.Theme.textDisabled : placing ? Design.Theme.warning : active ? Design.Theme.accent : mixed ? Design.Theme.textSecondary : Design.Theme.borderStrong

    readonly property string tooltipText: control.interactive ? control.hint : control.disabledReason

    readonly property string coverageText: (eligibleCount > 0 && activeCount > 0 && activeCount < eligibleCount) ? qsTr("%1 of %2").arg(activeCount).arg(eligibleCount) : ""

    implicitHeight: Math.max(Design.Metrics.commandButtonSize, Design.Metrics.minTouchTarget)
    implicitWidth: compact ? Design.Metrics.commandButtonSize + Design.Metrics.space24 : Design.Metrics.commandButtonSize * 3
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    Accessible.role: Accessible.Button
    Accessible.name: control.label
    Accessible.description: control.interactive ? (control.coverageText !== "" ? control.coverageText : control.hint) : control.disabledReason
    Accessible.checkable: true
    Accessible.checked: control.active

    ToolTip.text: control.compact && control.interactive && control.hint === "" ? control.label : control.tooltipText
    ToolTip.visible: control.hovered && ToolTip.text.length > 0
    ToolTip.delay: Design.Metrics.tooltipDelay

    Connections {
        function onClicked() {
            Design.UiSound.activate();
        }

        function onHoveredChanged() {
            if (control.hovered && control.interactive)
                Design.UiSound.hover();
        }

        target: control
    }

    MouseArea {
        anchors.fill: parent
        enabled: control.blocked
        visible: enabled
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.ForbiddenCursor
        onPressed: Design.UiSound.warning()
    }

    background: Rectangle {
        radius: Design.Metrics.radiusMedium
        color: !control.interactive ? Design.Theme.surfaceDisabled : control.down ? Qt.darker(Design.Theme.panelLeather, 1.2) : (control.highlighted || control.hovered) ? Design.Theme.panelLeather : Design.Theme.panelIron
        border.width: (control.showFocusRing || control.highlighted) ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.showFocusRing ? Design.Theme.focus : control.stateColor

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: Design.Metrics.borderThin
            width: control.highlighted ? Design.Metrics.space4 : 0
            radius: Design.Metrics.radiusSmall
            color: control.stateColor
            visible: width > 0

            Behavior on width  {
                NumberAnimation {
                    duration: Design.Motion.fast
                    easing.type: Design.Motion.standardEasing
                }
            }
        }

        Behavior on color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }

    contentItem: Item {
        implicitWidth: control.compact ? Design.Metrics.iconMedium + hotkeyLabel.width + Design.Metrics.space16 : Design.Metrics.iconMedium + textColumn.implicitWidth + Design.Metrics.space24

        Item {
            id: iconSlot

            anchors.left: parent.left
            anchors.leftMargin: control.compact ? Math.max(Design.Metrics.space4, (parent.width - width - hotkeyLabel.width) / 2) : Design.Metrics.space8
            anchors.verticalCenter: parent.verticalCenter

            width: control.compact ? Math.round(Design.Metrics.iconMedium * 1.35) : Design.Metrics.iconMedium
            height: width
            opacity: control.interactive ? 1 : 0.45

            Image {
                id: art

                anchors.fill: parent
                source: control.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                visible: source.toString() !== "" && status === Image.Ready
            }

            Design.IronVectorIcon {
                id: vectorArt

                anchors.fill: parent
                visible: !art.visible && vectorArt.available
                iconId: control.vectorIcon
                tint: control.interactive ? Design.Theme.textPrimary : Design.Theme.textDisabled
                accent: control.stateColor
                monochrome: !control.interactive
            }

            Text {
                anchors.centerIn: parent
                visible: !art.visible && !vectorArt.available
                text: control.glyph
                color: control.stateColor
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.heading
            }
        }

        Column {
            id: textColumn

            visible: !control.compact
            anchors.left: iconSlot.right
            anchors.leftMargin: Design.Metrics.space8
            anchors.right: hotkeyLabel.visible ? hotkeyLabel.left : parent.right
            anchors.rightMargin: Design.Metrics.space4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Text {
                width: parent.width
                text: control.label
                color: control.interactive ? Design.Theme.textPrimary : Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: control.highlighted ? Design.Typography.bold : Design.Typography.medium
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: text !== ""
                text: control.placing ? qsTr("Pick a target") : control.coverageText
                color: Design.Theme.textSecondary
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.caption
                elide: Text.ElideRight
            }
        }

        Design.IronHotkeyLabel {
            id: hotkeyLabel

            anchors.right: parent.right
            anchors.rightMargin: Design.Metrics.space4
            anchors.verticalCenter: parent.verticalCenter
            visible: control.hotkey !== ""
            text: control.hotkey
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.max(0, Math.min(1, control.cooldown))
            visible: control.cooldown > 0
            color: Design.Theme.backgroundDeep
            opacity: 0.55

            Behavior on width  {
                NumberAnimation {
                    duration: Design.Motion.fast
                }
            }
        }
    }
}
