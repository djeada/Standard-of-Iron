pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    readonly property string zeroGlyph: "N"
    readonly property int maxRoman: 19999

    readonly property var romanSteps: [{
            "value": 1000,
            "glyph": "M"
        }, {
            "value": 900,
            "glyph": "CM"
        }, {
            "value": 500,
            "glyph": "D"
        }, {
            "value": 400,
            "glyph": "CD"
        }, {
            "value": 100,
            "glyph": "C"
        }, {
            "value": 90,
            "glyph": "XC"
        }, {
            "value": 50,
            "glyph": "L"
        }, {
            "value": 40,
            "glyph": "XL"
        }, {
            "value": 10,
            "glyph": "X"
        }, {
            "value": 9,
            "glyph": "IX"
        }, {
            "value": 5,
            "glyph": "V"
        }, {
            "value": 4,
            "glyph": "IV"
        }, {
            "value": 1,
            "glyph": "I"
        }]

    function roman(value) {
        var number = Math.round(Number(value));
        if (!isFinite(number))
            return "";
        if (number < 0)
            return "-" + root.roman(-number);
        if (number === 0)
            return root.zeroGlyph;
        if (number > root.maxRoman)
            return String(number);
        var remainder = number;
        var text = "";
        for (var i = 0; i < root.romanSteps.length; ++i) {
            var step = root.romanSteps[i];
            while (remainder >= step.value) {
                text += step.glyph;
                remainder -= step.value;
            }
        }
        return text;
    }

    function ordinal(index) {
        return root.roman(Number(index) + 1);
    }

    function percent(value) {
        return root.roman(value) + "%";
    }

    function ratio(current, total) {
        return root.roman(current) + " / " + root.roman(total);
    }

    function clock(seconds) {
        var total = Math.max(0, Math.floor(Number(seconds) || 0));
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        return root.roman(hours) + ":" + root.roman(minutes) + ":" + root.roman(total % 60);
    }
}
