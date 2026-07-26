import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "Notifications"

    function init() {
        Notifications.clear();
    }

    function cleanup() {
        Notifications.clear();
    }

    function test_higher_priority_preempts_what_is_already_queued() {
        Notifications.info("Spearmen ready");
        Notifications.ambient("Supply route restored");
        Notifications.critical("Commander lost");
        compare(Notifications.count, 3);
        compare(Notifications.current.message, "Commander lost");
        compare(Notifications.queue[1].message, "Spearmen ready");
        compare(Notifications.queue[2].message, "Supply route restored");
    }

    function test_equal_priority_keeps_arrival_order() {
        Notifications.urgent("First");
        Notifications.urgent("Second");
        Notifications.urgent("Third");
        compare(Notifications.queue[0].message, "First");
        compare(Notifications.queue[1].message, "Second");
        compare(Notifications.queue[2].message, "Third");
    }

    function test_repeated_channel_collapses_into_one_row() {
        Notifications.urgent("Barracks under attack", {
                "channel": "under-attack"
            });
        Notifications.urgent("Barracks under attack", {
                "channel": "under-attack"
            });
        Notifications.urgent("Barracks under attack", {
                "channel": "under-attack"
            });
        compare(Notifications.count, 1);
        compare(Notifications.current.repeats, 3);
    }

    function test_a_repeat_can_escalate_the_channel_priority() {
        Notifications.info("Commander wounded", {
                "channel": "commander"
            });
        Notifications.critical("Commander dying", {
                "channel": "commander"
            });
        compare(Notifications.count, 1);
        compare(Notifications.current.priority, "critical");
        compare(Notifications.current.message, "Commander dying");
    }

    function test_a_repeat_never_downgrades_the_channel_priority() {
        Notifications.critical("Commander dying", {
                "channel": "commander"
            });
        Notifications.info("Commander stable", {
                "channel": "commander"
            });
        compare(Notifications.current.priority, "critical");
    }

    function test_unknown_priority_falls_back_to_info() {
        Notifications.push("whenever", "Reinforcements sighted");
        compare(Notifications.current.priority, "info");
    }

    function test_dismiss_removes_only_the_named_entry() {
        var first = Notifications.info("First");
        Notifications.info("Second");
        verify(Notifications.dismiss(first));
        compare(Notifications.count, 1);
        compare(Notifications.current.message, "Second");
        verify(!Notifications.dismiss(first), "dismissing twice should report no-op");
    }

    function test_host_renders_the_top_of_the_queue() {
        Notifications.info("Spearmen ready");
        Notifications.critical("Commander lost");
        var host = hostComponent.createObject(testCase);
        verify(host !== null, "NotificationHost failed to instantiate");
        compare(host.entries.length, 2);
        compare(host.entries[0].message, "Commander lost");
        host.destroy();
    }

    function test_host_shows_at_most_the_configured_row_count() {
        for (var i = 0; i < 6; ++i)
            Notifications.info("Message " + i);
        var host = hostComponent.createObject(testCase, {
                "visibleCount": 2
            });
        compare(host.entries.length, 2);
        host.destroy();
    }

    Component {
        id: hostComponent

        NotificationHost {
        }
    }
}
