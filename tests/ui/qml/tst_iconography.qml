import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "Iconography"

    readonly property var shippedNations: ["roman_republic", "carthage", "iron_sepulcher"]

    readonly property var hudCommands: ["attack", "guard", "hold", "patrol", "build", "heal", "collect", "rally", "deliver", "aura"]

    function test_every_hud_command_has_art_and_a_glyph() {
        for (var i = 0; i < hudCommands.length; ++i) {
            var id = hudCommands[i];
            verify(Icons.command(id).toString() !== "", id + " has no artwork");
            verify(Icons.commandGlyph(id).length > 0, id + " has no fallback glyph");
        }
    }

    function test_unknown_command_degrades_instead_of_breaking() {
        compare(Icons.command("teleport").toString(), "");
        compare(Icons.commandGlyph("teleport"), Icons.objective);
    }

    function test_every_resource_and_status_readout_has_art() {
        var resources = ["gold", "wood", "stone", "iron"];
        for (var i = 0; i < resources.length; ++i)
            verify(Icons.resource(resources[i]).toString() !== "", resources[i] + " has no icon");
        var statuses = ["population", "human", "ai", "defeated"];
        for (var s = 0; s < statuses.length; ++s)
            verify(Icons.status(statuses[s]).toString() !== "", statuses[s] + " has no icon");
    }

    function test_every_unit_type_resolves_for_every_shipped_nation() {
        for (var typeKey in Icons.unitArtBase) {
            for (var n = 0; n < shippedNations.length; ++n) {
                var source = Icons.unit(typeKey, shippedNations[n]);
                verify(source.toString() !== "", typeKey + " has no icon for " + shippedNations[n]);
            }
        }
    }

    function test_a_nation_agnostic_type_uses_neutral_art_without_a_faction() {
        verify(Icons.unit("wall", "").toString() !== "");
        verify(Icons.unit("wall", "").toString() !== Icons.unit("wall", "carthage").toString());
        verify(Icons.unit("wall", "roman_republic").toString() !== Icons.unit("wall", "carthage").toString());
    }

    function test_shared_art_ignores_the_nation() {
        for (var typeKey in Icons.unitArtShared) {
            var roman = Icons.unit(typeKey, "roman_republic").toString();
            var punic = Icons.unit(typeKey, "carthage").toString();
            compare(roman, punic, typeKey + " should share one icon across nations");
        }
    }

    function test_a_nation_without_its_own_art_borrows_the_default_family() {
        compare(Icons.unit("swordsman", "iron_sepulcher").toString(), Icons.unit("swordsman", "roman_republic").toString());
    }

    function test_unknown_nation_falls_back_rather_than_returning_nothing() {
        verify(Icons.unit("archer", "gauls").toString() !== "");
        compare(Icons.unit("archer", "gauls").toString(), Icons.unit("archer", "roman_republic").toString());
    }

    function test_unknown_unit_type_returns_nothing_so_the_glyph_takes_over() {
        compare(Icons.unit("chariot", "roman_republic").toString(), "");
        compare(Icons.unitGlyph("chariot"), Icons.defaultUnitGlyph);
    }

    function test_every_unit_type_with_art_also_has_a_glyph() {
        var typeKey;
        for (typeKey in Icons.unitArtBase)
            verify(Icons.unitGlyph(typeKey) !== Icons.defaultUnitGlyph, typeKey + " has art but no glyph");
        for (typeKey in Icons.unitArtShared)
            verify(Icons.unitGlyph(typeKey) !== Icons.defaultUnitGlyph, typeKey + " has art but no glyph");
    }

    function test_type_keys_are_derived_consistently_from_display_names() {
        compare(Icons.typeKeyFromName("Horse Archer"), "horse_archer");
        compare(Icons.typeKeyFromName("  Defense Tower  "), "defense_tower");
        compare(Icons.typeKeyFromName(""), "");
    }

    function test_registry_reports_every_filename_it_can_produce() {
        var names = Icons.allArtFilenames();
        verify(names.length > 0);
        verify(names.indexOf("attack_mode.png") >= 0);
        verify(names.indexOf("gold.png") >= 0);
        verify(names.indexOf("marketplace.png") >= 0);
        verify(names.indexOf("archer_rome.png") >= 0);
        verify(names.indexOf("archer_cartaghe.png") >= 0);
        for (var i = 0; i < names.length; ++i)
            compare(names.indexOf(names[i]), i, "duplicate entry: " + names[i]);
    }
}
