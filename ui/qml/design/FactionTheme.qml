pragma Singleton
import QtQuick 2.15
import StandardOfIron.Core 1.0 as Core

QtObject {
    id: root

    property string activeFaction: ""

    readonly property var neutral: ({
            "id": "",
            "name": qsTr("Standard"),
            "accent": Theme.accent,
            "accentDeep": Theme.borderStrong,
            "glyph": "◈",
            "emblem": "",
            "motto": qsTr("Iron and Ember")
        })

    readonly property var factions: ({
            "roman_republic": ({
                    "id": "roman_republic",
                    "name": qsTr("Roman Republic"),
                    "accent": "#c9a227",
                    "accentDeep": "#8c6a1f",
                    "glyph": "SPQR",
                    "emblem": root.emblemFor("roman_republic"),
                    "motto": qsTr("Senatus Populusque Romanus")
                }),
            "carthage": ({
                    "id": "carthage",
                    "name": qsTr("Carthage"),
                    "accent": "#9b59b6",
                    "accentDeep": "#6c3f80",
                    "glyph": "\u263E",
                    "emblem": root.emblemFor("carthage"),
                    "motto": qsTr("Qart-Ḥadašt")
                }),
            "iron_sepulcher": ({
                    "id": "iron_sepulcher",
                    "name": qsTr("Iron Sepulcher"),
                    "accent": "#7f8fa6",
                    "accentDeep": "#4a5566",
                    "glyph": "\u25C8",
                    "emblem": root.emblemFor("iron_sepulcher"),
                    "motto": qsTr("The watch does not sleep")
                })
        })

    readonly property var active: root.describe(root.activeFaction)
    readonly property color accent: active.accent
    readonly property color accentDeep: active.accentDeep

    function emblemFor(factionId) {
        var emblems = Core.Theme.nationEmblems;
        return emblems && emblems[factionId] ? emblems[factionId] : "";
    }

    function describe(factionId) {
        if (!factionId)
            return root.neutral;
        var entry = root.factions[factionId];
        return entry ? entry : root.neutral;
    }

    function nameFor(factionId) {
        return root.describe(factionId).name;
    }

    function accentFor(factionId) {
        return root.describe(factionId).accent;
    }

    function glyphFor(factionId) {
        return root.describe(factionId).glyph;
    }
}
