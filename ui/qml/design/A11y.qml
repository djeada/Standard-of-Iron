pragma Singleton
import QtQuick 2.15
import StandardOfIron.Core 1.0 as Core

QtObject {
    id: root

    readonly property real uiScale: Core.UiPreferences.uiScale
    readonly property bool reducedMotion: Core.UiPreferences.reducedMotion
    readonly property bool highContrast: Core.UiPreferences.highContrast

    readonly property string colorVisionMode: Core.UiPreferences.colorVisionMode

    readonly property bool alwaysShowFocus: Core.UiPreferences.alwaysShowFocus

    readonly property bool teamPatterns: Core.UiPreferences.effectiveTeamPatterns
    readonly property bool edgeScrollEnabled: Core.UiPreferences.edgeScrollEnabled
    readonly property real edgeScrollSensitivity: Core.UiPreferences.edgeScrollSensitivity
    readonly property real cameraMotionScale: Core.UiPreferences.cameraMotionScale
    readonly property bool damageNumbers: Core.UiPreferences.damageNumbers
    readonly property real screenEffectIntensity: Core.UiPreferences.screenEffectIntensity

    readonly property bool colorVisionAdjusted: colorVisionMode !== "none"

    readonly property bool redGreenImpaired: colorVisionMode === "protanopia" || colorVisionMode === "deuteranopia"
    readonly property bool blueYellowImpaired: colorVisionMode === "tritanopia"

    function scaled(px) {
        return Math.round(px * root.uiScale);
    }

    function scaledFont(px) {
        return Math.max(1, Math.round(px * root.uiScale));
    }
}
