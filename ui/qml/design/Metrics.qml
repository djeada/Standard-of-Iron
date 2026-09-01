pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property real uiScale: A11y.uiScale

    readonly property int space2: A11y.scaled(2)
    readonly property int space4: A11y.scaled(4)
    readonly property int space8: A11y.scaled(8)
    readonly property int space12: A11y.scaled(12)
    readonly property int space16: A11y.scaled(16)
    readonly property int space24: A11y.scaled(24)
    readonly property int space32: A11y.scaled(32)

    readonly property int radiusSmall: 3
    readonly property int radiusMedium: 5
    readonly property int radiusLarge: 8

    readonly property int borderThin: 1
    readonly property int borderFocus: 2

    readonly property int controlHeight: A11y.scaled(36)
    readonly property int toolControlHeight: A11y.scaled(30)
    readonly property int compactControlHeight: A11y.scaled(26)
    readonly property int commandButtonSize: A11y.scaled(48)
    readonly property int orderButtonSize: A11y.scaled(40)
    readonly property int iconSmall: A11y.scaled(16)
    readonly property int iconMedium: A11y.scaled(24)

    readonly property int panelPadding: A11y.scaled(12)
    readonly property int panelPaddingCompact: A11y.scaled(8)
    readonly property int scrollBarThickness: A11y.scaled(8)

    readonly property int minTouchTarget: Math.max(A11y.scaled(32), 32)

    readonly property int tooltipWidth: A11y.scaled(320)
    readonly property int tooltipDelay: 500

    readonly property int notificationWidth: A11y.scaled(340)
    readonly property int hudZoneMargin: A11y.scaled(12)

    readonly property int rtsBottomBarMinHeight: A11y.scaled(184)
    readonly property int rtsBottomBarMaxHeight: A11y.scaled(224)
    readonly property real rtsBottomBarShare: 0.21

    readonly property int commanderBottomBarMinHeight: A11y.scaled(96)
    readonly property int commanderBottomBarMaxHeight: A11y.scaled(120)
    readonly property real commanderBottomBarShare: 0.12

    readonly property real bottomBarViewportCeiling: 0.28

    function bottomBarHeight(viewportHeight, commanderMode) {
        var height = Number(viewportHeight) || 0;
        var floor = commanderMode ? root.commanderBottomBarMinHeight : root.rtsBottomBarMinHeight;
        var ceiling = commanderMode ? root.commanderBottomBarMaxHeight : root.rtsBottomBarMaxHeight;
        var share = commanderMode ? root.commanderBottomBarShare : root.rtsBottomBarShare;
        var preferred = Math.max(floor, Math.min(ceiling, height * share));
        return Math.min(preferred, Math.max(floor, height * root.bottomBarViewportCeiling));
    }
}
