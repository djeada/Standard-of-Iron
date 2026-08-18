pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property bool mirrored: Qt.application.layoutDirection === Qt.RightToLeft
    readonly property string chevronForward: root.mirrored ? "‹" : "›"

    readonly property string move: "➜"
    readonly property string attack: "⚔"
    readonly property string patrol: "⇄"
    readonly property string stop: "■"
    readonly property string hold: "◆"
    readonly property string defense: "◈"
    readonly property string formation: "☷"
    readonly property string rally: "⚑"
    readonly property string build: "⚒"
    readonly property string capture: "◎"
    readonly property string objective: "★"
    readonly property string warning: "⚠"
    readonly property string commander: "♛"
    readonly property string search: "⌕"
    readonly property string close: "×"
    readonly property string disclosureClosed: root.mirrored ? "◂" : "▸"
    readonly property string disclosureOpen: "▾"
    readonly property string heal: "✚"
    readonly property string collect: "⛏"
    readonly property string autoGather: "⟳"
    readonly property string dismantle: "⚒"
    readonly property string deliver: "⇥"
    readonly property string aura: "◌"
    readonly property string gate: "\u25A2"
    readonly property string population: "☷"
    readonly property string defeated: "☠"
    readonly property string pause: "⏸"
    readonly property string play: "▶"
    readonly property string reset: "↺"
    readonly property string follow: "☉"
    readonly property string spectator: "◉"

    readonly property string ambient: "\u25CB"
    readonly property string map: "\u25A3"
    readonly property string briefing: "\u2630"
    readonly property string locked: "\u2716"
    readonly property string terrainPlains: "\u2630"
    readonly property string terrainForest: "\u2663"
    readonly property string terrainRiver: "\u2248"
    readonly property string terrainDesert: "\u2591"
    readonly property string terrainHills: "\u25B2"

    readonly property var commandArt: ({
            "attack": "attack_mode.png",
            "guard": "defend_mode.png",
            "hold": "hold_mode.png",
            "patrol": "patrol_mode.png",
            "formation": "formation_mode.png",
            "build": "build_mode.png",
            "heal": "heal_mode.png",
            "collect": "collect_mode.png",
            "rally": "rally_mode.png",
            "deliver": "deliver_mode.png",
            "aura": "aura_mode.png"
        })

    readonly property var commandGlyphs: ({
            "attack": root.attack,
            "guard": root.defense,
            "hold": root.hold,
            "patrol": root.patrol,
            "formation": root.formation,
            "build": root.build,
            "heal": root.heal,
            "collect": root.collect,
            "auto_gather": root.autoGather,
            "dismantle": root.dismantle,
            "rally": root.rally,
            "deliver": root.deliver,
            "aura": root.aura,
            "gate": root.gate,
            "stop": root.stop
        })

    readonly property var resourceArt: ({
            "gold": "gold.png",
            "food": "food.png",
            "wood": "wood.png",
            "stone": "stone.png",
            "iron": "iron.png"
        })

    readonly property var statusArt: ({
            "population": "troop_count.png",
            "human": "human_player.png",
            "ai": "ai_player.png",
            "defeated": "defeated.png"
        })

    readonly property var nationArtSuffix: ({
            "roman_republic": "rome",
            "carthage": "cartaghe",
            "iron_sepulcher": "rome"
        })

    readonly property string defaultNationSuffix: "rome"

    readonly property var unitArtBase: ({
            "archer": "archer",
            "swordsman": "swordsman",
            "spearman": "spearman",
            "horse_archer": "horse_archer",
            "horse_swordsman": "horse_swordsman",
            "horse_spearman": "horse_spearman",
            "healer": "healer",
            "builder": "builder",
            "civilian": "builder",
            "catapult": "catapult",
            "ballista": "ballista",
            "defense_tower": "defense_tower",
            "home": "house",
            "house": "house",
            "wall": "wall",
            "wall_segment": "wall",
            "wall_gate": "wall"
        })

    readonly property var unitArtShared: ({
            "elephant": "elephant_cartaghe.png",
            "marketplace": "marketplace.png"
        })

    readonly property var unitArtNeutral: ({
            "wall": "wall.png",
            "wall_segment": "wall.png",
            "wall_gate": "wall.png"
        })

    readonly property var unitGlyphs: ({
            "archer": "\u279C",
            "swordsman": "\u2694",
            "spearman": "\u25B2",
            "horse_swordsman": "\u265E",
            "horse_archer": "\u265E",
            "horse_spearman": "\u265E",
            "healer": "\u271A",
            "civilian": "\u265F",
            "builder": "\u2692",
            "catapult": "\u25CE",
            "ballista": "\u25C8",
            "elephant": "\u25C6",
            "defense_tower": "\u265C",
            "wall": "\u25AC",
            "wall_segment": "\u25AC",
            "wall_gate": "\u25A2",
            "home": "\u2302",
            "house": "\u2302",
            "marketplace": "\u25C7",
            "temple": "\u25B3",
            "farm": "\u2618",
            "commander": "\u265B"
        })

    readonly property var resourceGlyphs: ({
            "gold": "\u25CF",
            "food": "\u2740",
            "wood": "\u2551",
            "stone": "\u25B0",
            "iron": "\u25C6"
        })

    readonly property string defaultUnitGlyph: "\u25CF"

    function artPath(filename) {
        return filename ? Qt.resolvedUrl("../../assets/visuals/icons/" + filename) : "";
    }

    function command(actionId) {
        return root.artPath(root.commandArt[actionId] || "");
    }

    function commandGlyph(actionId) {
        return root.commandGlyphs[actionId] || root.objective;
    }

    function resource(kind) {
        return root.artPath(root.resourceArt[kind] || "");
    }

    function resourceGlyph(kind) {
        return root.resourceGlyphs[kind] || root.objective;
    }

    function status(kind) {
        return root.artPath(root.statusArt[kind] || "");
    }

    function unit(unitType, nationId) {
        if (!unitType)
            return "";
        var shared = root.unitArtShared[unitType];
        if (shared)
            return root.artPath(shared);
        var base = root.unitArtBase[unitType];
        if (!base)
            return "";
        var suffix = root.nationArtSuffix[nationId];
        if (!suffix) {
            var neutral = root.unitArtNeutral[unitType];
            if (neutral)
                return root.artPath(neutral);
            suffix = root.defaultNationSuffix;
        }
        return root.artPath(base + "_" + suffix + ".png");
    }

    function terrain(kind) {
        var name = (kind || "").toString().toLowerCase();
        if (name.indexOf("forest") >= 0 || name.indexOf("wood") >= 0)
            return root.terrainForest;
        if (name.indexOf("river") >= 0 || name.indexOf("coast") >= 0 || name.indexOf("water") >= 0)
            return root.terrainRiver;
        if (name.indexOf("desert") >= 0 || name.indexOf("sand") >= 0)
            return root.terrainDesert;
        if (name.indexOf("hill") >= 0 || name.indexOf("mountain") >= 0 || name.indexOf("alp") >= 0)
            return root.terrainHills;
        if (name.indexOf("plain") >= 0 || name.indexOf("field") >= 0)
            return root.terrainPlains;
        switch (kind) {
        case "forest":
            return root.terrainForest;
        case "river":
            return root.terrainRiver;
        case "desert":
            return root.terrainDesert;
        case "hills":
        case "mountain":
            return root.terrainHills;
        case "plains":
            return root.terrainPlains;
        default:
            return root.map;
        }
    }

    function unitGlyph(unitType) {
        return root.unitGlyphs[unitType] || root.defaultUnitGlyph;
    }

    function humanise(key) {
        if (!key)
            return "";
        var text = key.toString().replace(/_/g, " ").trim();
        return text.charAt(0).toUpperCase() + text.slice(1);
    }

    function typeKeyFromName(displayName) {
        if (!displayName)
            return "";
        return displayName.toString().trim().toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
    }

    function allArtFilenames() {
        var names = [];
        var key;
        for (key in root.commandArt)
            names.push(root.commandArt[key]);
        for (key in root.resourceArt)
            names.push(root.resourceArt[key]);
        for (key in root.statusArt)
            names.push(root.statusArt[key]);
        for (key in root.unitArtShared)
            names.push(root.unitArtShared[key]);
        for (key in root.unitArtNeutral)
            names.push(root.unitArtNeutral[key]);
        var suffixes = [];
        for (var nation in root.nationArtSuffix) {
            var suffix = root.nationArtSuffix[nation];
            if (suffixes.indexOf(suffix) < 0)
                suffixes.push(suffix);
        }
        for (key in root.unitArtBase) {
            for (var i = 0; i < suffixes.length; ++i)
                names.push(root.unitArtBase[key] + "_" + suffixes[i] + ".png");
        }
        var unique = [];
        for (var n = 0; n < names.length; ++n) {
            if (unique.indexOf(names[n]) < 0)
                unique.push(names[n]);
        }
        return unique;
    }
}
