import QtQuick 2.15
import QtQuick.Controls 2.15
import ".." as Design

ComboBox {
    id: control

    property string accessibleName: ""
    property string disabledReason: ""

    property var labelFor: function (data) {
        return data;
    }

    readonly property bool showFocusRing: visualFocus || popup.visible || (Design.A11y.alwaysShowFocus && activeFocus)

    function resolveLabel(data) {
        return (typeof labelFor === "function") ? labelFor(data) : data;
    }

    implicitHeight: Math.max(Design.Metrics.controlHeight, Design.Metrics.minTouchTarget)
    Accessible.name: accessibleName
    Accessible.description: enabled ? displayText : disabledReason

    contentItem: Text {
        text: control.displayText
        color: control.enabled ? Design.Theme.textPrimary : Design.Theme.textDisabled
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.label
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: Design.Metrics.space8
        rightPadding: control.indicator.width + Design.Metrics.space8
    }

    background: Rectangle {
        color: control.enabled ? Design.Theme.panelIron : Design.Theme.surfaceDisabled
        radius: Design.Metrics.radiusSmall
        border.width: control.showFocusRing ? Design.Metrics.borderFocus : Design.Metrics.borderThin
        border.color: !control.enabled ? Design.Theme.borderSubtle : control.showFocusRing ? Design.Theme.focus : control.hovered ? Design.Theme.accent : Design.Theme.borderSubtle

        Behavior on border.color  {
            ColorAnimation {
                duration: Design.Motion.fast
            }
        }
    }

    indicator: Text {
        x: control.width - width - Design.Metrics.space8
        y: control.topPadding + (control.availableHeight - height) / 2
        text: Design.Icons.disclosureOpen
        color: control.enabled ? Design.Theme.textSecondary : Design.Theme.textDisabled
        font.family: Design.Typography.family
        font.pixelSize: Design.Typography.body
    }

    delegate: ItemDelegate {
        id: entry

        required property int index
        required property var modelData

        width: control.width
        height: Math.max(Design.Metrics.controlHeight, Design.Metrics.minTouchTarget)
        highlighted: control.highlightedIndex === entry.index

        contentItem: Text {
            text: control.resolveLabel(entry.modelData)
            color: Design.Theme.textPrimary
            font.family: Design.Typography.family
            font.pixelSize: Design.Typography.label
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: entry.highlighted ? Design.Theme.panelLeather : Design.Theme.panelIron

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: control.currentIndex === entry.index ? Design.Metrics.space2 : 0
                color: Design.Theme.accent
            }
        }
    }

    popup: Popup {
        y: control.height
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, Design.Metrics.space24 * 12)
        padding: Design.Metrics.borderThin

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {
            }
        }

        background: Rectangle {
            color: Design.Theme.backgroundRaised
            radius: Design.Metrics.radiusSmall
            border.width: Design.Metrics.borderThin
            border.color: Design.Theme.borderStrong
        }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Design.Motion.fast
            }
        }
    }
}
