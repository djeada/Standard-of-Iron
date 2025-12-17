import QtQuick 2.15
pragma Singleton

QtObject {
    id: root

    readonly property var palette: ({
        "bg": "#071018",
        "bgShade": "#061214",
        "dim": Qt.rgba(0, 0, 0, 0.45),
        "panelBase": "#0E1C1E",
        "panelBr": "#0f2430",
        "cardBase": "#132526",
        "cardBaseA": "#132526AA",
        "cardBaseB": "#06141b",
        "cardBorder": "#12323a",
        "hover": "#184c7a",
        "hoverBg": "#184c7a",
        "selected": "#1f8bf5",
        "selectedBg": "#1f8bf5",
        "selectedBr": "#1b74d1",
        "thumbBr": "#2A4E56",
        "border": "#0f2b34",
        "textMain": "#eaf6ff",
        "textBright": "#dff0ff",
        "textSub": "#86a7b6",
        "textSubLite": "#79a6b7",
        "textDim": "#4f6a75",
        "textHint": "#2a5e6e",
        "accent": "#9fd9ff",
        "accentBright": "#d0e8ff",
        "addColor": "#3A9CA8",
        "removeColor": "#D04040",
        "dangerColor": "#D04040",
        "startColor": "#40D080"
    })
    readonly property var button: ({
        "primary": {
            "normalBg": palette.selectedBg,
            "hoverBg": "#2a7fe0",
            "pressBg": palette.selectedBr,
            "disabledBg": "#0a1a24",
            "normalBorder": palette.selectedBr,
            "hoverBorder": palette.selectedBr,
            "disabledBorder": palette.panelBr,
            "textColor": "white",
            "disabledTextColor": "#6f8793",
            "radius": 9,
            "height": 40,
            "minWidth": 120,
            "fontSize": 12,
            "hoverFontSize": 13
        },
        "secondary": {
            "normalBg": "transparent",
            "hoverBg": palette.cardBase,
            "pressBg": palette.hover,
            "disabledBg": "transparent",
            "normalBorder": palette.cardBorder,
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
        "default": ({
            "default": ""
        })
    })

    function iconPath(filename) {
        return "qrc:/StandardOfIron/assets/visuals/icons/" + filename;
    }

}
