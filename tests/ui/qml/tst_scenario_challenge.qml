import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0 as Design
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "ScenarioChallenge"

    function test_rating_data() {
        return [{
                "tag": "unrated",
                "modifier": 0,
                "rating": 1
            }, {
                "tag": "baseline",
                "modifier": 1.0,
                "rating": 1
            }, {
                "tag": "one_step",
                "modifier": 1.15,
                "rating": 1
            }, {
                "tag": "two_steps",
                "modifier": 1.3,
                "rating": 2
            }, {
                "tag": "mid",
                "modifier": 1.7,
                "rating": 5
            }, {
                "tag": "clamped",
                "modifier": 4.0,
                "rating": 5
            }];
    }

    function test_rating(data) {
        compare(ScenarioChallenge.rating(data.modifier), data.rating);
    }

    function test_the_list_and_the_detail_panel_agree() {
        var modifiers = [1.0, 1.15, 1.3, 1.45, 1.7, 2.2];
        for (var i = 0; i < modifiers.length; i++) {
            var rating = ScenarioChallenge.rating(modifiers[i]);
            var stars = ScenarioChallenge.stars(modifiers[i]);
            var filled = stars.split("★").length - 1;
            compare(filled, rating, "star row disagrees at " + modifiers[i]);
            compare(stars.length, ScenarioChallenge.maxRating);
            compare(ScenarioChallenge.roman_rating(modifiers[i]), Design.Numerals.roman(rating) + "/" + Design.Numerals.roman(ScenarioChallenge.maxRating), "detail rating disagrees at " + modifiers[i]);
        }
    }

    function test_an_unrated_mission_shows_no_row() {
        verify(!ScenarioChallenge.has_rating(0));
        verify(!ScenarioChallenge.has_rating(undefined));
        verify(ScenarioChallenge.has_rating(1.3));
    }

    function test_the_chosen_preset_is_a_different_scale() {
        compare(ScenarioChallenge.rating(1.5), 4);
        compare(DifficultyCatalog.name_for("hard"), qsTr("Hard"));
        verify(ScenarioChallenge.maxRating !== DifficultyCatalog.entries.length);
    }
}
