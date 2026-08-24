import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "ActivityIcons"
    when: windowShown
    width: 400
    height: 200
    visible: true

    function makeIcon(props) {
        return iconComponent.createObject(testCase, props || {});
    }

    function test_every_activity_names_itself_and_has_art() {
        var ids = ActivityIcons.ids();
        verify(ids.length >= 16, "the activity registry looks truncated");
        for (var i = 0; i < ids.length; ++i) {
            var id = ids[i];
            verify(Core.IconArt.has(ActivityIcons.iconFor(id)), id + " has no drawing");
            verify(ActivityIcons.label(id).length > 0, id + " has no label");
            verify(ActivityIcons.hint(id).length > 0, id + " has no tooltip text");
        }
    }

    function test_every_activity_named_in_the_issue_is_covered() {
        var required = ["construct", "repair", "dismantle", "chop_wood", "mine_stone", "mine_iron", "move", "attack", "patrol", "guard", "hold", "blocked", "deliver"];
        for (var i = 0; i < required.length; ++i)
            verify(ActivityIcons.ids().indexOf(required[i]) >= 0, "missing activity: " + required[i]);
    }

    function test_gathering_activities_name_their_resource() {
        compare(ActivityIcons.resourceFor("chop_wood"), "wood");
        compare(ActivityIcons.resourceFor("mine_stone"), "stone");
        compare(ActivityIcons.resourceFor("mine_iron"), "iron");
        verify(ActivityIcons.isGathering("mine_iron"));
        verify(!ActivityIcons.isGathering("construct"));
    }

    function test_the_five_order_states_are_all_described() {
        var states = ActivityIcons.stateIds();
        compare(states.length, 5);
        for (var i = 0; i < states.length; ++i) {
            verify(ActivityIcons.state(states[i]).label.length > 0);
            verify(ActivityIcons.state(states[i]).hint.length > 0);
        }
        verify(states.indexOf("active") >= 0);
        verify(states.indexOf("queued") >= 0);
        verify(states.indexOf("unavailable") >= 0);
        verify(states.indexOf("interrupted") >= 0);
        verify(states.indexOf("locked") >= 0);
    }

    function test_an_unknown_activity_degrades_to_idle_rather_than_breaking() {
        compare(ActivityIcons.label("teleporting"), ActivityIcons.label("idle"));
        compare(ActivityIcons.state("dreaming").label, ActivityIcons.state("active").label);
    }

    function test_the_summary_only_qualifies_a_state_worth_mentioning() {
        compare(ActivityIcons.summary("chop_wood", "active"), ActivityIcons.label("chop_wood"));
        verify(ActivityIcons.summary("chop_wood", "interrupted") !== ActivityIcons.label("chop_wood"));
    }

    function test_a_marker_reads_out_its_work_state_and_headcount() {
        var icon = makeIcon({
                "activity": "mine_iron",
                "state_id": "unavailable",
                "count": 5
            });
        verify(icon.Accessible.name.indexOf(ActivityIcons.label("mine_iron")) >= 0);
        verify(icon.Accessible.description.indexOf("5") >= 0, "headcount is not announced");
        verify(icon.tooltipText.length > 0);
        icon.destroy();
    }

    function test_states_are_told_apart_by_more_than_colour() {
        var active = makeIcon({
                "activity": "repair",
                "state_id": "active"
            });
        var queued = makeIcon({
                "activity": "repair",
                "state_id": "queued"
            });
        var stalled = makeIcon({
                "activity": "repair",
                "state_id": "unavailable"
            });
        verify(!active.stateMeta.decorated, "an active order should not carry a warning mark");
        verify(queued.stateMeta.decorated);
        verify(stalled.stateMeta.decorated);
        verify(active.stateTone.toString() !== stalled.stateTone.toString());
        verify(queued.stateTone.toString() !== stalled.stateTone.toString());
        active.destroy();
        queued.destroy();
        stalled.destroy();
    }

    function test_the_vector_icon_reports_when_it_has_nothing_to_draw() {
        var known = vectorComponent.createObject(testCase, {
                "iconId": "repair"
            });
        var unknown = vectorComponent.createObject(testCase, {
                "iconId": "teleport"
            });
        verify(known.available);
        verify(known.shapes.length > 0);
        verify(!unknown.available);
        verify(!unknown.visible, "an icon with no art must not leave an empty hole");
        known.destroy();
        unknown.destroy();
    }

    function test_command_buttons_prefer_the_vector_drawing() {
        var button = commandComponent.createObject(testCase, {
                "actionId": "repair",
                "label": "Repair"
            });
        compare(button.vectorIcon, "repair");
        verify(Core.IconArt.has(button.vectorIcon));
        button.destroy();
    }

    Component {
        id: iconComponent

        IronActivityIcon {
        }
    }

    Component {
        id: vectorComponent

        IronVectorIcon {
        }
    }

    Component {
        id: commandComponent

        IronCommandButton {
        }
    }
}
