pragma Singleton
import QtQuick 2.15

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
            "carthageGlyph": "𐤒 Qart-Ḥadašt"
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
            "fast": 120,
            "normal": 160,
            "slow": 200
        })
    readonly property var unit_icons: ({
            "archer": "🏹",
            "swordsman": "⚔️",
            "spearman": "🛡️",
            "horse_swordsman": "🐎⚔️",
            "horse_archer": "🏹🐎",
            "horse_spearman": "🐎🛡️",
            "healer": "✚",
            "civilian": "🧑",
            "builder": "🔨",
            "catapult": "🛞",
            "ballista": "🎯",
            "elephant": "🐘",
            "defense_tower": "🏰",
            "wall": "🧱",
            "home": "🏠",
            "default": "👤"
        })
    readonly property var unit_icon_sources: ({
            "archer": ({
                    "default": root.icon_path("archer_rome.png"),
                    "roman_republic": root.icon_path("archer_rome.png"),
                    "carthage": root.icon_path("archer_cartaghe.png")
                }),
            "swordsman": ({
                    "default": root.icon_path("swordsman_rome.png"),
                    "roman_republic": root.icon_path("swordsman_rome.png"),
                    "carthage": root.icon_path("swordsman_cartaghe.png")
                }),
            "spearman": ({
                    "default": root.icon_path("spearman_rome.png"),
                    "roman_republic": root.icon_path("spearman_rome.png"),
                    "carthage": root.icon_path("spearman_cartaghe.png")
                }),
            "horse_swordsman": ({
                    "default": root.icon_path("horse_swordsman_rome.png"),
                    "roman_republic": root.icon_path("horse_swordsman_rome.png"),
                    "carthage": root.icon_path("horse_swordsman_cartaghe.png")
                }),
            "horse_archer": ({
                    "default": root.icon_path("horse_archer_rome.png"),
                    "roman_republic": root.icon_path("horse_archer_rome.png"),
                    "carthage": root.icon_path("horse_archer_cartaghe.png")
                }),
            "horse_spearman": ({
                    "default": root.icon_path("horse_spearman_rome.png"),
                    "roman_republic": root.icon_path("horse_spearman_rome.png"),
                    "carthage": root.icon_path("horse_spearman_cartaghe.png")
                }),
            "healer": ({
                    "default": root.icon_path("healer_rome.png"),
                    "roman_republic": root.icon_path("healer_rome.png"),
                    "carthage": root.icon_path("healer_cartaghe.png")
                }),
            "civilian": ({
                    "default": root.icon_path("builder_rome.png"),
                    "roman_republic": root.icon_path("builder_rome.png"),
                    "carthage": root.icon_path("builder_cartaghe.png")
                }),
            "builder": ({
                    "default": root.icon_path("builder_rome.png"),
                    "roman_republic": root.icon_path("builder_rome.png"),
                    "carthage": root.icon_path("builder_cartaghe.png")
                }),
            "catapult": ({
                    "default": root.icon_path("catapult_rome.png"),
                    "roman_republic": root.icon_path("catapult_rome.png"),
                    "carthage": root.icon_path("catapult_cartaghe.png")
                }),
            "ballista": ({
                    "default": root.icon_path("ballista_rome.png"),
                    "roman_republic": root.icon_path("ballista_rome.png"),
                    "carthage": root.icon_path("ballista_cartaghe.png")
                }),
            "elephant": ({
                    "default": root.icon_path("elephant_cartaghe.png"),
                    "roman_republic": root.icon_path("elephant_cartaghe.png"),
                    "carthage": root.icon_path("elephant_cartaghe.png")
                }),
            "defense_tower": ({
                    "default": root.icon_path("defense_tower_rome.png"),
                    "roman_republic": root.icon_path("defense_tower_rome.png"),
                    "carthage": root.icon_path("defense_tower_cartaghe.png")
                }),
            "wall": ({
                    "default": root.icon_path("wall.png"),
                    "roman_republic": root.icon_path("wall.png"),
                    "carthage": root.icon_path("wall.png")
                }),
            "wall_segment": ({
                    "default": root.icon_path("wall.png"),
                    "roman_republic": root.icon_path("wall.png"),
                    "carthage": root.icon_path("wall.png")
                }),
            "home": ({
                    "default": root.icon_path("house_rome.png"),
                    "roman_republic": root.icon_path("house_rome.png"),
                    "carthage": root.icon_path("house_cartaghe.png")
                }),
            "default": ({
                    "default": ""
                })
        })

    function icon_path(filename) {
        return Qt.resolvedUrl("../../assets/visuals/icons/" + filename);
    }
}
