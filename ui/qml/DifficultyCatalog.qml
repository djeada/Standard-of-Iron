pragma Singleton
import QtQuick 2.15
import StandardOfIron.Design 1.0 as Design

QtObject {
    id: root

    readonly property string label: qsTr("Difficulty")
    readonly property string scopeNote: qsTr("Difficulty changes enemy resources, starting troops and reinforcements only. Enemy tactics never change.")
    readonly property string defaultId: "normal"

    readonly property var entries: [{
            "id": "easy",
            "name": qsTr("Easy"),
            "quip": qsTr("The legions forgot their alarm clock."),
            "icon": "difficulty_easy",
            "accent": Design.Theme.success
        }, {
            "id": "normal",
            "name": qsTr("Normal"),
            "quip": qsTr("A perfectly respectable amount of chaos."),
            "icon": "difficulty_normal",
            "accent": Design.Theme.accent
        }, {
            "id": "hard",
            "name": qsTr("Hard"),
            "quip": qsTr("The treasury has discovered your location."),
            "icon": "difficulty_hard",
            "accent": Design.Theme.focus
        }, {
            "id": "very_hard",
            "name": qsTr("Brutal"),
            "quip": qsTr("HONK. The empire has doubled its budget."),
            "icon": "difficulty_very_hard",
            "accent": Design.Theme.danger
        }]

    function entry(difficultyId) {
        let wanted = String(difficultyId || root.defaultId);
        for (let i = 0; i < root.entries.length; i++) {
            if (root.entries[i].id === wanted)
                return root.entries[i];
        }
        return root.entries[1];
    }

    function name_for(difficultyId) {
        return root.entry(difficultyId).name;
    }

    function icon_for(difficultyId) {
        return root.entry(difficultyId).icon;
    }

    function accent_for(difficultyId) {
        return root.entry(difficultyId).accent;
    }

    function quip_for(difficultyId) {
        return root.entry(difficultyId).quip;
    }

    function signed_percent(multiplier) {
        let delta = Math.round((Number(multiplier) - 1) * 100);
        if (delta === 0)
            return "±0%";
        return (delta > 0 ? "+" : "−") + Math.abs(delta) + "%";
    }

    function summary(preset) {
        if (!preset || preset.resource_multiplier === undefined)
            return "";
        return qsTr("Enemy resources %1 · starting troops %2 · reinforcements %3").arg(root.signed_percent(preset.resource_multiplier)).arg(root.signed_percent(preset.unit_multiplier)).arg(root.signed_percent(preset.wave_multiplier));
    }

    function preset_for(presets, difficultyId) {
        let wanted = String(difficultyId || root.defaultId);
        if (!presets)
            return null;
        for (let i = 0; i < presets.length; i++) {
            if (presets[i].id === wanted)
                return presets[i];
        }
        return null;
    }

    function summary_for(presets, difficultyId) {
        return root.summary(root.preset_for(presets, difficultyId));
    }
}
