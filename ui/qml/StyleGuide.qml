import QtQuick 2.15
pragma Singleton

QtObject {
    id: root

    readonly property var palette: ({
        "bg": "#17110C",
        "bgShade": "#120D09",
        "dim": Qt.rgba(0, 0, 0, 0.45),
        "panelBase": "#231B14",
        "panelBr": "#8C6A3E",
        "cardBase": "#2F251D",
        "cardBaseA": "#2F251DBB",
        "cardBaseB": "#1D1610",
        "cardBorder": "#A7814A",
        "hover": "#6B4D2A",
        "hoverBg": "#6B4D2A",
        "selected": "#A32925",
        "selectedBg": "#A32925",
        "selectedBr": "#C0403B",
        "thumbBr": "#B08A50",
        "border": "#7A5C36",
        "textMain": "#F4E7C8",
        "textBright": "#EEDDB3",
        "textSub": "#C7A66D",
        "textSubLite": "#D4B57C",
        "textDim": "#8D7146",
        "textHint": "#6B5231",
        "accent": "#D4A15A",
        "accentBright": "#E8C98B",
        "addColor": "#3A9CA8",
        "removeColor": "#D04040",
        "dangerColor": "#D04040",
        "startColor": "#40D080"
    })
    readonly property var historical: ({
        "parchmentLight": "#3B2F24",
        "parchmentDark": "#1D1610",
        "bronze": "#C29555",
        "bronzeDeep": "#8C6A3E",
        "wax": "#A32925",
        "waxHover": "#C0403B",
        "waxDark": "#7A1F1D",
        "bannerNeutral": "#5C4A33",
        "bannerAttack": "#7A1F1D",
        "romanGlyph": "SPQR",
        "carthageGlyph": "𐤒 QRT"
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
            "textColor": palette.textMain,
            "disabledTextColor": palette.textDim,
            "radius": 9,
            "height": 40,
            "minWidth": 120,
            "fontSize": 12,
            "hoverFontSize": 13
        },
        "secondary": {
            "normalBg": historical.parchmentDark,
            "hoverBg": historical.parchmentLight,
            "pressBg": palette.hover,
            "disabledBg": palette.cardBaseB,
            "normalBorder": historical.bronzeDeep,
            "hoverBorder": palette.thumbBr,
            "disabledBorder": palette.panelBr,
            "textColor": palette.textBright,
            "disabledTextColor": palette.textDim,
            "radius": 8,
            "height": 38,
            "minWidth": 100,
            "fontSize": 11,
            "hoverFontSize": 12
        },
        "small": {
            "normalBg": palette.addColor,
            "hoverBg": Qt.lighter(palette.addColor, 1.2),
            "pressBg": Qt.darker(palette.addColor, 1.2),
            "disabledBg": palette.cardBase,
            "normalBorder": Qt.lighter(palette.addColor, 1.1),
            "hoverBorder": Qt.lighter(palette.addColor, 1.3),
            "disabledBorder": palette.thumbBr,
            "textColor": "white",
            "disabledTextColor": palette.textDim,
            "radius": 6,
            "height": 32,
            "minWidth": 80,
            "fontSize": 11,
            "hoverFontSize": 11
        },
        "danger": {
            "normalBg": "transparent",
            "hoverBg": palette.dangerColor,
            "pressBg": Qt.darker(palette.dangerColor, 1.2),
            "disabledBg": "transparent",
            "normalBorder": palette.dangerColor,
            "hoverBorder": palette.dangerColor,
            "disabledBorder": palette.thumbBr,
            "textColor": palette.dangerColor,
            "hoverTextColor": "white",
            "disabledTextColor": palette.textDim,
            "radius": 4,
            "height": 32,
            "minWidth": 32,
            "fontSize": 14,
            "hoverFontSize": 14
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
    readonly property var listItem: ({
        "height": 48,
        "radius": 6,
        "spacing": 10,
        "bg": palette.cardBaseB,
        "border": palette.thumbBr,
        "borderWidth": 1
    })
    readonly property var animation: ({
        "fast": 120,
        "normal": 160,
        "slow": 200
    })
    readonly property var unitIcons: ({
        "archer": "🏹",
        "swordsman": "⚔️",
        "spearman": "🛡️",
        "horse_swordsman": "🐎⚔️",
        "horse_archer": "🏹🐎",
        "horse_spearman": "🐎🛡️",
        "healer": "✚",
        "builder": "🔨",
        "catapult": "🛞",
        "ballista": "🎯",
        "elephant": "🐘",
        "defense_tower": "🏰",
        "wall": "🧱",
        "home": "🏠",
        "default": "👤"
    })
    readonly property var unitIconSources: ({
        "archer": ({
            "default": root.iconPath("archer_rome.png"),
            "roman_republic": root.iconPath("archer_rome.png"),
            "carthage": root.iconPath("archer_cartaghe.png")
        }),
        "swordsman": ({
            "default": root.iconPath("swordsman_rome.png"),
            "roman_republic": root.iconPath("swordsman_rome.png"),
            "carthage": root.iconPath("swordsman_cartaghe.png")
        }),
        "spearman": ({
            "default": root.iconPath("spearman_rome.png"),
            "roman_republic": root.iconPath("spearman_rome.png"),
            "carthage": root.iconPath("spearman_cartaghe.png")
        }),
        "horse_swordsman": ({
            "default": root.iconPath("horse_swordsman_rome.png"),
            "roman_republic": root.iconPath("horse_swordsman_rome.png"),
            "carthage": root.iconPath("horse_swordsman_cartaghe.png")
        }),
        "horse_archer": ({
            "default": root.iconPath("horse_archer_rome.png"),
            "roman_republic": root.iconPath("horse_archer_rome.png"),
            "carthage": root.iconPath("horse_archer_cartaghe.png")
        }),
        "horse_spearman": ({
            "default": root.iconPath("horse_spearman_rome.png"),
            "roman_republic": root.iconPath("horse_spearman_rome.png"),
            "carthage": root.iconPath("horse_spearman_cartaghe.png")
        }),
        "healer": ({
            "default": root.iconPath("healer_rome.png"),
            "roman_republic": root.iconPath("healer_rome.png"),
            "carthage": root.iconPath("healer_cartaghe.png")
        }),
        "builder": ({
            "default": root.iconPath("builder_rome.png"),
            "roman_republic": root.iconPath("builder_rome.png"),
            "carthage": root.iconPath("builder_cartaghe.png")
        }),
        "catapult": ({
            "default": root.iconPath("catapult_rome.png"),
            "roman_republic": root.iconPath("catapult_rome.png"),
            "carthage": root.iconPath("catapult_cartaghe.png")
        }),
        "ballista": ({
            "default": root.iconPath("ballista_rome.png"),
            "roman_republic": root.iconPath("ballista_rome.png"),
            "carthage": root.iconPath("ballista_cartaghe.png")
        }),
        "elephant": ({
            "default": root.iconPath("elephant_cartaghe.png"),
            "roman_republic": root.iconPath("elephant_cartaghe.png"),
            "carthage": root.iconPath("elephant_cartaghe.png")
        }),
        "defense_tower": ({
            "default": root.iconPath("defense_tower_rome.png"),
            "roman_republic": root.iconPath("defense_tower_rome.png"),
            "carthage": root.iconPath("defense_tower_cartaghe.png")
        }),
        "wall": ({
            "default": root.iconPath("wall_rome.png"),
            "roman_republic": root.iconPath("wall_rome.png"),
            "carthage": root.iconPath("wall_cartaghe.png")
        }),
        "home": ({
            "default": root.iconPath("house_rome.png"),
            "roman_republic": root.iconPath("house_rome.png"),
            "carthage": root.iconPath("house_cartaghe.png")
        }),
        "default": ({
            "default": ""
        })
    })

    function iconPath(filename) {
        return Qt.resolvedUrl("../../assets/visuals/icons/" + filename);
    }

}
