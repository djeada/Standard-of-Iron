import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

Slider {
    id: control

    property string accessibleName: ""

    property bool blocked: false
    readonly property bool interactive: enabled && !blocked

    readonly property bool showFocusRing: visualFocus || (Design.A11y.alwaysShowFocus && activeFocus)

    implicitHeight: Math.max(Design.Metrics.controlHeight, Design.Metrics.minTouchTarget)
    implicitWidth: Design.Metrics.space24 * 6
    hoverEnabled: true

    Accessible.name: accessibleName
    Accessible.description: Math.round(control.position * 100) + "%"

    Connections {
        function onPressedChanged() {
            if (control.pressed)
                Design.UiSound.toggle();
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
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: Design.Metrics.space4 + Design.Metrics.space2
        radius: height / 2
        color: Design.Theme.backgroundDeep
        border.width: Design.Metrics.borderThin
        border.color: Design.Theme.borderSubtle

        Rectangle {
            id: sliderFill

            readonly property color tone: control.interactive ? Design.Theme.accent : Design.Theme.textDisabled

            width: control.position * parent.width
            height: parent.height
            radius: parent.radius

            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Design.Theme.sheenTop(sliderFill.tone)
                }

                GradientStop {
                    position: 1
                    color: Design.Theme.sheenBottom(sliderFill.tone)
                }
            }
        }
    }

    handle: Rectangle {
        id: grip

        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: Design.Metrics.iconSmall
        height: width
        radius: Design.Metrics.radiusSmall
        color: !control.interactive ? Design.Theme.surfaceDisabled : control.pressed ? Design.Theme.focus : control.hovered ? Qt.lighter(Design.Theme.accent, 1.12) : Design.Theme.accent
        border.width: control.showFocusRing ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: control.showFocusRing ? Design.Theme.focus : Design.Theme.backgroundDeep
        scale: control.pressed ? 1.12 : 1

        gradient: Gradient {
            GradientStop {
                position: 0
                color: Design.Theme.sheenTop(grip.color)
            }

            GradientStop {
                position: 1
                color: Design.Theme.sheenBottom(grip.color)
            }
        }

        Design.IronShadow {
            cornerRadius: grip.radius
            spread: Design.Metrics.space4
            lift: 1
        }

        Behavior on scale  {
            NumberAnimation {
                duration: Design.Motion.fast
                easing.type: Design.Motion.standardEasing
            }
        }

        Behavior on color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }
}
