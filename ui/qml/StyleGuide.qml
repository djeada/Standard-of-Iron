pragma Singleton
import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

QtObject {
    id: root

    readonly property var palette: ({
            "bg": Design.Theme.backgroundRaised,
            "bgShade": Design.Theme.backgroundDeep,
            "dim": Design.Theme.scrim,
            "panelBase": Design.Theme.panelLeather,
            "panelBr": Design.Theme.borderSubtle,
            "cardBase": Design.Theme.panelIron,
            "cardBaseA": Design.Theme.panelIron,
            "cardBaseB": Design.Theme.backgroundDeep,
            "cardBorder": Design.Theme.borderStrong,
            "hover": Design.Theme.panelLeather,
            "hoverBg": Design.Theme.panelLeather,
            "selected": Design.Theme.selection,
            "selectedBg": Design.Theme.selection,
            "selectedBr": Design.Theme.selection,
            "thumbBr": Design.Theme.borderStrong,
            "border": Design.Theme.borderSubtle,
            "textMain": Design.Theme.textPrimary,
            "textBright": Design.Theme.textSecondary,
            "textSub": Design.Theme.textSecondary,
            "textSubLite": Design.Theme.textSecondary,
            "textDim": Design.Theme.textDisabled,
            "textHint": Design.Theme.textDisabled,
            "accent": Design.Theme.accent,
            "accentBright": Design.Theme.focus,
            "addColor": Design.Theme.success,
            "removeColor": Design.Theme.danger,
            "dangerColor": Design.Theme.danger,
            "startColor": Design.Theme.success
        })
    readonly property var historical: ({
            "parchmentLight": Design.Theme.panelLeather,
            "parchmentDark": Design.Theme.panelIron,
            "bronze": Design.Theme.borderStrong,
            "bronzeDeep": Design.Theme.borderSubtle,
            "wax": Design.Theme.selection,
            "waxHover": Design.Theme.danger,
            "waxDark": Design.Theme.backgroundDeep,
            "bannerNeutral": Design.Theme.panelLeather,
            "bannerAttack": Design.Theme.danger,
            "romanGlyph": Design.FactionTheme.glyphFor("roman_republic"),
            "carthageGlyph": Design.FactionTheme.glyphFor("carthage")
        })
    readonly property var button: ({
            "primary": {
                "normalBg": historical.wax,
                "hoverBg": historical.waxHover,
                "pressBg": historical.waxDark,
                "disabledBg": palette.cardBaseB,
                "normalBorder": historical.bronze,
                "hoverBorder": historical.bronze,
                "disabledBorder": palette.panelBr,
                "text_color": palette.textMain,
                "disabledTextColor": palette.textDim,
                "radius": 9,
                "height": 40,
                "minWidth": 120,
                "fontSize": 12,
                "hoverFontSize": 13,
                "letterSpacing": 1.5
            },
            "secondary": {
                "normalBg": historical.parchmentDark,
                "hoverBg": historical.parchmentLight,
                "pressBg": palette.hover,
                "disabledBg": palette.cardBaseB,
                "normalBorder": historical.bronzeDeep,
                "hoverBorder": palette.thumbBr,
                "disabledBorder": palette.panelBr,
                "text_color": palette.textBright,
                "disabledTextColor": palette.textDim,
                "radius": 8,
                "height": 38,
                "minWidth": 100,
                "fontSize": 11,
                "hoverFontSize": 12,
                "letterSpacing": 1.0
            },
            "small": {
                "normalBg": "#3E5C24",
                "hoverBg": "#4C7030",
                "pressBg": "#2D4318",
                "disabledBg": palette.cardBase,
                "normalBorder": "#6E8A40",
                "hoverBorder": "#8AAA52",
                "disabledBorder": palette.thumbBr,
                "text_color": "#CBE0A8",
                "disabledTextColor": palette.textDim,
                "radius": 6,
                "height": 32,
                "minWidth": 80,
                "fontSize": 11,
                "hoverFontSize": 11,
                "letterSpacing": 0.5
            },
            "danger": {
                "normalBg": "transparent",
                "hoverBg": palette.dangerColor,
                "pressBg": Qt.darker(palette.dangerColor, 1.2),
                "disabledBg": "transparent",
                "normalBorder": palette.dangerColor,
                "hoverBorder": palette.dangerColor,
                "disabledBorder": palette.thumbBr,
                "text_color": palette.dangerColor,
                "hoverTextColor": "white",
                "disabledTextColor": palette.textDim,
                "radius": 4,
                "height": 32,
                "minWidth": 32,
                "fontSize": 14,
                "hoverFontSize": 14,
                "letterSpacing": 0.0
            }
        })
    readonly property var card: ({
            "radius": 8,
            "borderWidth": 1,
            "bg": palette.cardBase,
            "border": palette.cardBorder,
            "hoverBg": palette.hover,
            "selectedBg": palette.selected,
            "selectedBorder": palette.selectedBr
        })
    readonly property var list_item: ({
            "height": 48,
            "radius": 6,
            "spacing": 10,
            "bg": palette.cardBaseB,
            "border": palette.thumbBr,
            "borderWidth": 1
        })
    readonly property var animation: ({
            "fast": Design.Motion.fast,
            "normal": Design.Motion.normal,
            "slow": Design.Motion.deliberate
        })

    function icon_path(filename) {
        return Design.Icons.artPath(filename);
    }
}
