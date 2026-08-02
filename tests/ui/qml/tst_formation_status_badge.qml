import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "FormationStatusBadge"
    when: windowShown
    width: 400
    height: 300
    visible: true

    function makeBadge(status) {
        return badgeComponent.createObject(testCase, {
                "status": status
            });
    }

    function test_no_formation_hides_the_badge() {
        var badge = makeBadge({
                "active": false
            });
        verify(!badge.has_formation);
        verify(!badge.visible);
        badge.destroy();
    }

    function test_a_formed_line_reads_as_formed_and_healthy() {
        var badge = makeBadge({
                "active": true,
                "phase": "formed",
                "cohesion": 0.95,
                "member_count": 12
            });
        verify(badge.has_formation);
        compare(badge.phase_label, qsTr("Formed"));
        compare(badge.phase_tone, Theme.success);
        compare(badge.member_count, 12);
        badge.destroy();
    }

    function test_a_disrupted_line_reads_as_danger() {
        var badge = makeBadge({
                "active": true,
                "phase": "disrupted",
                "cohesion": 0.2,
                "member_count": 12
            });
        compare(badge.phase_label, qsTr("Disrupted"));
        compare(badge.phase_tone, Theme.danger);
        badge.destroy();
    }

    function test_an_unknown_phase_falls_back_to_forming() {
        var badge = makeBadge({
                "active": true,
                "phase": "",
                "cohesion": 0.5
            });
        compare(badge.phase_label, qsTr("Forming up"));
        compare(badge.phase_tone, Theme.warning);
        badge.destroy();
    }

    function test_phase_colours_stay_distinct() {
        var formed = makeBadge({
                "active": true,
                "phase": "formed"
            });
        var forming = makeBadge({
                "active": true,
                "phase": "forming"
            });
        var disrupted = makeBadge({
                "active": true,
                "phase": "disrupted"
            });
        verify(formed.phase_tone !== forming.phase_tone);
        verify(forming.phase_tone !== disrupted.phase_tone);
        verify(formed.phase_tone !== disrupted.phase_tone);
        verify(formed.phase_label !== forming.phase_label);
        verify(forming.phase_label !== disrupted.phase_label);
        formed.destroy();
        forming.destroy();
        disrupted.destroy();
    }

    Component {
        id: badgeComponent

        FormationStatusBadge {
        }
    }
}
