import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
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

    function init() {
        Core.UiHints.restore_all();
        Core.UiHints.dismiss_all();
    }

    function test_no_formation_hides_the_badge() {
        var badge = makeBadge({
                "active": false
            });
        verify(!badge.has_formation);
        verify(!badge.visible);
        badge.destroy();
    }

    function test_the_badge_waits_for_a_deployment_before_it_appears() {
        var badge = makeBadge({
                "active": true,
                "phase": "formed",
                "cohesion": 0.9,
                "member_count": 8
            });
        badge.gate = true;
        verify(!badge.visible, "an active formation alone must not raise the readout");
        Core.UiHints.show(badge.hintId);
        verify(badge.visible, "deploying a multi-unit order must raise the readout");
        Core.UiHints.on_selection_changed();
        verify(!badge.visible, "changing the selection must close the readout");
        badge.destroy();
    }

    function test_closing_the_badge_hides_it_without_turning_it_off() {
        var badge = makeBadge({
                "active": true,
                "phase": "formed"
            });
        badge.gate = true;
        Core.UiHints.show(badge.hintId);
        verify(badge.visible);
        badge.dismiss();
        verify(!badge.visible);
        verify(Core.UiHints.enabled[badge.hintId] === true);
        badge.destroy();
    }

    function test_never_show_again_keeps_the_badge_down_for_good() {
        var badge = makeBadge({
                "active": true,
                "phase": "formed"
            });
        badge.gate = true;
        badge.stop_showing();
        Core.UiHints.show(badge.hintId);
        verify(!badge.visible, "a suppressed readout must ignore later deployments");
        verify(Core.UiHints.enabled[badge.hintId] === false);
        Core.UiHints.restore_all();
        badge.destroy();
    }

    function test_every_phase_the_simulation_can_report_has_a_label_data() {
        return [{
                "tag": "reforming",
                "phase": "reforming",
                "label": qsTr("Forming up")
            }, {
                "tag": "formed",
                "phase": "formed",
                "label": qsTr("Formed")
            }, {
                "tag": "arrived",
                "phase": "arrived",
                "label": qsTr("In position")
            }, {
                "tag": "opening",
                "phase": "opening",
                "label": qsTr("Opening ranks")
            }, {
                "tag": "traversing",
                "phase": "traversing",
                "label": qsTr("Filing through")
            }, {
                "tag": "disrupted",
                "phase": "disrupted",
                "label": qsTr("Disrupted")
            }];
    }

    function test_every_phase_the_simulation_can_report_has_a_label(data) {
        var badge = makeBadge({
                "active": true,
                "phase": data.phase,
                "cohesion": 0.8,
                "member_count": 6
            });
        compare(badge.phase_label, data.label, data.tag + " fell through to the default label");
        verify(badge.phase_hint.length > 0, data.tag + " has no hint");
        badge.destroy();
    }

    function test_a_settled_line_reads_as_healthy_data() {
        return [{
                "tag": "formed",
                "phase": "formed"
            }, {
                "tag": "arrived",
                "phase": "arrived"
            }];
    }

    function test_a_settled_line_reads_as_healthy(data) {
        var badge = makeBadge({
                "active": true,
                "phase": data.phase,
                "cohesion": 0.9,
                "member_count": 6
            });
        compare(badge.phase_tone, Theme.success, data.tag + " was not painted as settled");
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
