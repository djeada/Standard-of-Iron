import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "FactionTheme"

    readonly property var shippedFactions: ["roman_republic", "carthage", "iron_sepulcher"]

    function cleanup() {
        FactionTheme.activeFaction = "";
    }

    function test_every_shipped_faction_has_an_identity() {
        for (var i = 0; i < shippedFactions.length; ++i) {
            var faction = FactionTheme.describe(shippedFactions[i]);
            compare(faction.id, shippedFactions[i]);
            verify(faction.name.length > 0, shippedFactions[i] + " has no name");
            verify(faction.glyph.length > 0, shippedFactions[i] + " has no heraldic glyph");
            verify(faction.accent !== undefined, shippedFactions[i] + " has no accent");
        }
    }

    function test_faction_accents_stay_distinguishable() {
        for (var i = 0; i < shippedFactions.length; ++i) {
            for (var j = i + 1; j < shippedFactions.length; ++j) {
                verify(!Qt.colorEqual(FactionTheme.accentFor(shippedFactions[i]), FactionTheme.accentFor(shippedFactions[j])), shippedFactions[i] + " and " + shippedFactions[j] + " share an accent");
            }
        }
    }

    function test_unknown_faction_falls_back_to_the_neutral_skin() {
        var unknown = FactionTheme.describe("gauls");
        compare(unknown.id, "");
        compare(unknown.name, FactionTheme.neutral.name);
    }

    function test_active_faction_drives_the_shared_accent() {
        FactionTheme.activeFaction = "iron_sepulcher";
        compare(FactionTheme.active.id, "iron_sepulcher");
        compare(FactionTheme.accent.toString(), FactionTheme.accentFor("iron_sepulcher").toString());
    }

    function test_factions_with_emblem_art_expose_it() {
        verify(FactionTheme.describe("roman_republic").emblem.length > 0);
        verify(FactionTheme.describe("carthage").emblem.length > 0);
    }
}
