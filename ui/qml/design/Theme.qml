pragma Singleton
import QtQuick 2.15
import StandardOfIron.Core 1.0 as Core

QtObject {
    id: root

    property string variant: A11y.highContrast ? "highContrast" : "ironAndEmber"

    readonly property bool highContrast: variant === "highContrast"

    readonly property color backgroundDeep: highContrast ? "#050505" : Core.Theme.backgroundDeep
    readonly property color backgroundRaised: highContrast ? "#111111" : Core.Theme.backgroundRaised
    readonly property color panelIron: highContrast ? "#181818" : Core.Theme.panelIron
    readonly property color panelLeather: highContrast ? "#231c15" : Core.Theme.panelLeather
    readonly property color parchment: highContrast ? "#fff3cf" : Core.Theme.parchment
    readonly property color textPrimary: highContrast ? "#ffffff" : Core.Theme.textPrimary
    readonly property color textSecondary: highContrast ? "#f2d899" : Core.Theme.textSecondary
    readonly property color textDisabled: highContrast ? "#a0a0a0" : Core.Theme.textDisabled
    readonly property color borderSubtle: highContrast ? "#8c8c8c" : Core.Theme.borderSubtle
    readonly property color borderStrong: highContrast ? "#f0c674" : Core.Theme.borderStrong
    readonly property color accent: highContrast ? "#ffb347" : Core.Theme.accent
    readonly property color focus: highContrast ? "#ffffff" : Core.Theme.accentBright
    readonly property color shadow: "#99000000"

    readonly property color success: highContrast ? "#7ee39a" : (A11y.redGreenImpaired ? "#56b4e9" : Core.Theme.success)
    readonly property color danger: highContrast ? "#ff6767" : (A11y.redGreenImpaired ? "#ff8c42" : Core.Theme.danger)
    readonly property color warning: highContrast ? "#ffd54f" : (A11y.blueYellowImpaired ? "#ff9ec4" : Core.Theme.warning)
    readonly property color selection: highContrast ? "#67b7ff" : (A11y.blueYellowImpaired ? "#d98cff" : Core.Theme.selection)

    readonly property color surfaceDisabled: highContrast ? "#1a1a1a" : Core.Theme.disabledBg
    readonly property color scrim: Core.Theme.dim

    function statusColor(status) {
        switch (status) {
        case "success":
        case "complete":
            return root.success;
        case "danger":
        case "failed":
            return root.danger;
        case "warning":
            return root.warning;
        case "selected":
            return root.selection;
        case "disabled":
            return root.textDisabled;
        default:
            return root.accent;
        }
    }
}
