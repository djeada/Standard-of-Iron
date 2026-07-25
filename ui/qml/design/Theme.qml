pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    property string variant: "ironAndEmber"
    readonly property bool highContrast: variant === "highContrast"

    readonly property color backgroundDeep: highContrast ? "#050505" : CoreTheme.backgroundDeep
    readonly property color backgroundRaised: highContrast ? "#111111" : CoreTheme.backgroundRaised
    readonly property color panelIron: highContrast ? "#181818" : CoreTheme.panelIron
    readonly property color panelLeather: highContrast ? "#231c15" : CoreTheme.panelLeather
    readonly property color parchment: highContrast ? "#fff3cf" : CoreTheme.parchment
    readonly property color textPrimary: highContrast ? "#ffffff" : CoreTheme.textPrimary
    readonly property color textSecondary: highContrast ? "#f2d899" : CoreTheme.textSecondary
    readonly property color textDisabled: highContrast ? "#a0a0a0" : CoreTheme.textDisabled
    readonly property color borderSubtle: highContrast ? "#8c8c8c" : CoreTheme.borderSubtle
    readonly property color borderStrong: highContrast ? "#f0c674" : CoreTheme.borderStrong
    readonly property color accent: highContrast ? "#ffb347" : CoreTheme.accent
    readonly property color warning: highContrast ? "#ffd54f" : CoreTheme.warning
    readonly property color danger: highContrast ? "#ff6767" : CoreTheme.danger
    readonly property color success: highContrast ? "#7ee39a" : CoreTheme.success
    readonly property color selection: highContrast ? "#67b7ff" : CoreTheme.selection
    readonly property color focus: highContrast ? "#ffffff" : CoreTheme.accentBright
    readonly property color shadow: "#99000000"
}
