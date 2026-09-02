import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
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

    property var details: []

    property string statusText: ""

    property bool blocked: false

    property bool spotlit: false
    readonly property bool interactive: enabled && !blocked

    property string shortLabel: ""
    property bool iconOnly: false

    property bool tile: false

    readonly property bool compact: control.iconOnly || (width > 0 && width < minimumShortWidth)

    readonly property bool showsShortLabel: !control.iconOnly && control.shortLabel !== "" && width < minimumLabelledWidth
    readonly property string displayLabel: control.showsShortLabel ? control.shortLabel : control.label

    readonly property real labelChromeWidth: Design.Metrics.iconMedium + Design.Metrics.space8 + Design.Metrics.space8 + Design.Metrics.space4 + hotkeyWidth
    readonly property real minimumLabelledWidth: labelChromeWidth + labelMetrics.width
    readonly property real minimumShortWidth: control.shortLabel !== "" ? labelChromeWidth + shortLabelMetrics.width : minimumLabelledWidth
    readonly property real hotkeyWidth: control.hotkey !== "" ? hotkeyMetrics.width + Design.Metrics.space8 + Design.Metrics.space4 : 0

    TextMetrics {
        id: labelMetrics

        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        font.weight: Design.Typography.medium
        text: control.label
    }

    TextMetrics {
        id: shortLabelMetrics

        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        font.weight: Design.Typography.medium
        text: control.shortLabel
    }

    TextMetrics {
        id: hotkeyMetrics

        font.family: "monospace"
        font.pixelSize: Design.Typography.caption
        text: control.hotkey
    }

    readonly property bool highlighted: active || placing
    readonly property bool showFocusRing: visualFocus || (Design.A11y.alwaysShowFocus && activeFocus)
    readonly property color stateColor: !interactive ? Design.Theme.textDisabled : placing ? Design.Theme.warning : active ? Design.Theme.accent : mixed ? Design.Theme.textSecondary : Design.Theme.borderStrong

    readonly property string tooltipText: control.interactive ? control.hint : control.disabledReason

    readonly property alias tooltip: commandTooltip

    readonly property bool attachedToWindow: control.Window.window !== null

    readonly property bool hasTooltipBody: control.hint !== "" || control.statusText !== "" || (!control.interactive && control.disabledReason !== "") || (control.details && control.details.length > 0)

    readonly property string coverageText: (eligibleCount > 0 && activeCount > 0 && activeCount < eligibleCount) ? qsTr("%1 of %2").arg(activeCount).arg(eligibleCount) : ""

    implicitHeight: control.tile ? Math.max(Design.Metrics.orderButtonSize, Design.Metrics.minTouchTarget) : Math.max(Design.Metrics.commandButtonSize, Design.Metrics.minTouchTarget)
    implicitWidth: control.tile ? control.implicitHeight : compact ? Design.Metrics.commandButtonSize + Design.Metrics.space24 : Design.Metrics.commandButtonSize * 3
    hoverEnabled: true

    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.Button
    Accessible.name: control.label
    Accessible.description: control.interactive ? (control.coverageText !== "" ? control.coverageText : control.hint) : control.disabledReason
    Accessible.checkable: true
    Accessible.checked: control.active

    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()

    Design.IronSpotlight {
        active: control.spotlit
        cornerRadius: Design.Metrics.radiusMedium
    }

    Design.IronCommandTooltip {
        id: commandTooltip

        parent: control
        visible: control.attachedToWindow && (control.hovered || control.showFocusRing) && control.hasTooltipBody
        title: control.label
        hotkey: control.hotkey
        summary: control.hint

        details: visible ? control.details : []
        status: control.interactive ? control.statusText : ""
        warning: control.interactive ? "" : control.disabledReason
    }

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
            width: control.highlighted ? (control.tile ? Design.Metrics.space2 : Design.Metrics.space4) : 0
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
        implicitWidth: control.tile ? control.implicitHeight : control.compact ? Design.Metrics.iconMedium + control.hotkeyWidth + Design.Metrics.space16 : Design.Metrics.iconMedium + textColumn.implicitWidth + Design.Metrics.space24

        Item {
            id: iconSlot

            anchors.left: parent.left
            anchors.leftMargin: control.tile ? Math.max(0, (parent.width - iconSlot.width) / 2) : control.compact ? Math.max(Design.Metrics.space4, (parent.width - width - hotkeyLabel.width) / 2) : Design.Metrics.space8
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: control.tile && tileHotkey.visible ? -Design.Metrics.space2 : 0

            width: control.tile ? Math.round(Design.Metrics.iconMedium * 1.1) : control.compact ? Math.round(Design.Metrics.iconMedium * 1.35) : Design.Metrics.iconMedium
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
                text: control.displayLabel
                color: control.interactive ? Design.Theme.textPrimary : Design.Theme.textDisabled
                font.family: Design.Typography.family
                font.pixelSize: Design.Typography.label
                font.weight: control.highlighted ? Design.Typography.bold : Design.Typography.medium
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: text !== ""
                text: control.statusText !== "" ? control.statusText : (control.placing ? qsTr("Pick a target") : control.coverageText)
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
            visible: !control.tile && control.hotkey !== ""
            text: control.hotkey
        }

        Text {
            id: tileHotkey

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: Design.Metrics.space2
            anchors.bottomMargin: 1
            visible: control.tile && control.hotkey !== ""
            text: control.hotkey
            color: control.interactive ? Design.Theme.textSecondary : Design.Theme.textDisabled
            font.family: "monospace"
            font.pixelSize: Math.max(9, Math.round(Design.Typography.caption * 0.72))
            opacity: 0.85
        }

        Rectangle {
            id: tileStateDot

            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Design.Metrics.space4
            width: Design.Metrics.space4
            height: width
            radius: width / 2
            visible: control.tile && (control.highlighted || control.mixed)
            color: control.stateColor
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
