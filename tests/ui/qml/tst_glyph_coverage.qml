import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import StandardOfIron.TestSupport 1.0

TestCase {
    id: testCase

    name: "GlyphCoverage"

    readonly property var glyphNames: ["move", "attack", "patrol", "stop", "hold", "defense", "formation", "rally", "build", "capture", "objective", "warning", "commander", "search", "close", "disclosureClosed", "disclosureOpen", "heal", "collect", "deliver", "aura", "population", "defeated", "pause", "play", "reset", "follow", "spectator", "ambient", "map", "briefing", "locked", "terrainPlains", "terrainForest", "terrainRiver", "terrainDesert", "terrainHills"]

    function missingCharacters(text) {
        return GlyphProbe.missing(Typography.family, text);
    }

    function missingDisplayCharacters(text) {
        return GlyphProbe.missing(Typography.displayFamily, text);
    }

    function missingTitleCharacters(text) {
        return GlyphProbe.missingWithoutFallback(Typography.titleFamily, text);
    }

    function test_the_title_face_loads_from_the_bundle() {
        compare(Typography.brandFont.status, FontLoader.Ready, "bundled title face did not load from qrc");
        verify(Typography.titleFamily.length > 0, "titleFamily is empty");
        verify(Typography.titleFamily !== Typography.displayFamily, "titleFamily fell back to displayFamily, so the bundled face is not loading");
    }

    function test_the_title_face_covers_the_words_titles_use() {
        var missing = missingTitleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.\u2026,:;!?-'\"()&%+/\u00d7");
        compare(missing.join(", "), "", "characters missing from the title face");
    }

    function test_the_title_face_covers_the_capitals_the_localisations_need() {
        var missing = missingTitleCharacters("\u00c1\u00c4\u00c7\u00c9\u00cd\u00d1\u00d3\u00d6\u00da\u00dc\u011e\u0130\u015e");
        compare(missing.join(", "), "", "an uppercased title would fall out of the brand face mid-word");
    }

    function test_every_interface_glyph_exists_in_the_ui_font() {
        var broken = [];
        for (var i = 0; i < glyphNames.length; ++i) {
            var name = glyphNames[i];
            var glyph = Icons[name];
            verify(glyph !== undefined && glyph.length > 0, name + " is empty");
            var missing = missingCharacters(glyph);
            if (missing.length > 0)
                broken.push(name + ": " + missing.join(", "));
        }
        compare(broken.join(" | "), "", "glyphs missing from the UI font");
    }

    function test_every_terrain_mark_exists_in_the_ui_font() {
        var kinds = ["plains", "forest", "river", "desert", "hills", "mountain", "unknown"];
        var broken = [];
        for (var i = 0; i < kinds.length; ++i) {
            var missing = missingCharacters(Icons.terrain(kinds[i]));
            if (missing.length > 0)
                broken.push(kinds[i] + ": " + missing.join(", "));
        }
        compare(broken.join(" | "), "", "terrain marks missing from the UI font");
    }

    function test_every_unit_glyph_exists_in_the_ui_font() {
        var broken = [];
        for (var typeKey in Icons.unitGlyphs) {
            var missing = missingCharacters(Icons.unitGlyphs[typeKey]);
            if (missing.length > 0)
                broken.push(typeKey + ": " + missing.join(", "));
        }
        var defaultMissing = missingCharacters(Icons.defaultUnitGlyph);
        if (defaultMissing.length > 0)
            broken.push("default: " + defaultMissing.join(", "));
        compare(broken.join(" | "), "", "unit glyphs missing from the UI font");
    }

    function test_every_faction_glyph_exists_in_the_display_font() {
        var factions = ["roman_republic", "carthage", "iron_sepulcher", ""];
        var broken = [];
        for (var i = 0; i < factions.length; ++i) {
            var missing = missingDisplayCharacters(FactionTheme.glyphFor(factions[i]));
            if (missing.length > 0)
                broken.push((factions[i] || "neutral") + ": " + missing.join(", "));
        }
        compare(broken.join(" | "), "", "faction glyphs missing from the font");
    }

    function test_every_objective_marker_exists_in_the_ui_font() {
        var states = ["active", "complete", "failed", "optional"];
        var broken = [];
        for (var i = 0; i < states.length; ++i) {
            var row = markerProbe.createObject(testCase, {
                    "objectiveState": states[i]
                });
            var missing = missingCharacters(row.marker);
            if (missing.length > 0)
                broken.push(states[i] + ": " + missing.join(", "));
            row.destroy();
        }
        compare(broken.join(" | "), "", "objective markers missing from the font");
    }

    Component {
        id: markerProbe

        IronObjectiveRow {
        }
    }
}
