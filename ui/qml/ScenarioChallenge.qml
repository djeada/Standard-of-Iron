pragma Singleton
import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

QtObject {
    id: root

    readonly property int maxRating: 5
    readonly property int minRating: 1

    readonly property real stepSize: 0.15

    function rating(modifier) {
        let value = Number(modifier);
        if (!value || !isFinite(value))
            return root.minRating;
        let steps = Math.ceil((value - 1.0) / root.stepSize - 1e-6);
        return Math.max(root.minRating, Math.min(root.maxRating, steps));
    }

    function has_rating(modifier) {
        let value = Number(modifier);
        return !!value && isFinite(value);
    }

    function stars(modifier) {
        let filled = root.rating(modifier);
        let text = "";
        for (let i = 0; i < root.maxRating; i++)
            text += i < filled ? "★" : "☆";
        return text;
    }

    function roman_rating(modifier) {
        return Design.Numerals.roman(root.rating(modifier)) + "/" + Design.Numerals.roman(root.maxRating);
    }
}
