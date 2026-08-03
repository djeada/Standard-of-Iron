import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "Numerals"

    function test_roman_data() {
        return [{
                "tag": "one",
                "value": 1,
                "text": "I"
            }, {
                "tag": "four",
                "value": 4,
                "text": "IV"
            }, {
                "tag": "nine",
                "value": 9,
                "text": "IX"
            }, {
                "tag": "fourteen",
                "value": 14,
                "text": "XIV"
            }, {
                "tag": "forty",
                "value": 40,
                "text": "XL"
            }, {
                "tag": "eighty_eight",
                "value": 88,
                "text": "LXXXVIII"
            }, {
                "tag": "hundred",
                "value": 100,
                "text": "C"
            }, {
                "tag": "four_hundred_twenty",
                "value": 420,
                "text": "CDXX"
            }, {
                "tag": "nine_hundred",
                "value": 900,
                "text": "CM"
            }, {
                "tag": "three_thousand_nine_hundred_ninety_nine",
                "value": 3999,
                "text": "MMMCMXCIX"
            }, {
                "tag": "five_thousand",
                "value": 5000,
                "text": "MMMMM"
            }];
    }

    function test_roman(data) {
        compare(Numerals.roman(data.value), data.text);
    }

    function test_zero_uses_the_roman_nulla() {
        compare(Numerals.roman(0), "N");
        compare(Numerals.percent(0), "N%");
    }

    function test_fractions_round_to_the_nearest_whole() {
        compare(Numerals.roman(3.4), "III");
        compare(Numerals.roman(3.6), "IV");
        compare(Numerals.percent(79.5), "LXXX%");
    }

    function test_values_beyond_the_ceiling_stay_arabic() {
        compare(Numerals.roman(20000), "20000");
    }

    function test_negative_values_keep_their_sign() {
        compare(Numerals.roman(-12), "-XII");
    }

    function test_ordinal_counts_from_one() {
        compare(Numerals.ordinal(0), "I");
        compare(Numerals.ordinal(3), "IV");
    }

    function test_ratio_pairs_both_sides() {
        compare(Numerals.ratio(3, 12), "III / XII");
        compare(Numerals.ratio(0, 5), "N / V");
    }

    function test_clock_splits_hours_minutes_seconds() {
        compare(Numerals.clock(0), "N:N:N");
        compare(Numerals.clock(3661), "I:I:I");
        compare(Numerals.clock(754), "N:XII:XXXIV");
    }

    function test_non_numeric_input_yields_empty_text() {
        compare(Numerals.roman("not a number"), "");
    }
}
