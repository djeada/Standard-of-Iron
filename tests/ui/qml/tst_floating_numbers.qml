import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Design 1.0 as Design
import "../../../ui/qml"

TestCase {
    id: testCase

    name: "FloatingNumbers"
    when: windowShown
    width: 800
    height: 600
    visible: true

    property var pendingBatches: []

    QtObject {
        id: stubCamera

        function project_world(wx, wy, wz) {
            return {
                "valid": true,
                "x": 400 + wx,
                "y": 300 - wy
            };
        }
    }

    QtObject {
        id: stubSource

        function pop_feedback_ticks() {
            if (testCase.pendingBatches.length === 0)
                return [];
            return testCase.pendingBatches.shift();
        }
    }

    WorldProjector {
        id: projector

        width: testCase.width
        height: testCase.height
        camera: stubCamera
    }

    FloatingNumbers {
        id: numbers

        width: testCase.width
        height: testCase.height
        source: stubSource
        projector: projector
    }

    function init() {
        testCase.pendingBatches = [];
    }

    function tick(overrides) {
        var base = {
            "kind": "damage",
            "style": "tick",
            "amount": 12,
            "severity": 0.2,
            "resource": -1,
            "pairedResource": -1,
            "pairedAmount": 0,
            "lane": 0,
            "hits": 1,
            "killingBlow": false,
            "incoming": false,
            "outgoing": true,
            "focused": false,
            "x": 0,
            "y": 0,
            "z": 0
        };
        for (var key in overrides)
            base[key] = overrides[key];
        return base;
    }

    function test_the_projector_only_reports_ready_with_a_camera() {
        verify(projector.ready);
        var lonely = Qt.createQmlObject("import \"../../../ui/qml\"; WorldProjector {}", testCase);
        verify(!lonely.ready);
        compare(lonely.project(1, 2, 3), null, "no camera means no projection");
        lonely.destroy();
    }

    function test_incoming_damage_reads_as_a_loss_and_outgoing_as_a_plain_number() {
        compare(numbers.body_for({
                    "kind": "damage",
                    "amount": 24,
                    "incoming": true
                }), "-24");
        compare(numbers.body_for({
                    "kind": "damage",
                    "amount": 24,
                    "incoming": false
                }), "24");
    }

    function test_economy_amounts_keep_their_sign() {
        compare(numbers.body_for({
                    "kind": "resource",
                    "amount": -40
                }), "-40");
        compare(numbers.body_for({
                    "kind": "resource",
                    "amount": 8
                }), "+8");
        compare(numbers.body_for({
                    "kind": "reserve",
                    "amount": -18
                }), "-18");
    }

    function test_every_resource_index_maps_to_a_glyph() {
        for (var i = 0; i < 5; ++i)
            verify(numbers.resource_glyph(i).length > 0, "resource " + i + " has no glyph");
        compare(numbers.resource_glyph(-1), "", "population is not a resource");
        compare(numbers.resource_glyph(99), "");
    }

    function test_spending_and_earning_are_told_apart_by_colour() {
        var spend = numbers.accent_for({
                "kind": "resource",
                "amount": -40
            });
        var earn = numbers.accent_for({
                "kind": "resource",
                "amount": 8
            });
        verify(!Qt.colorEqual(spend, earn), "a spend must not look like a gain");
    }

    function test_each_kind_is_gated_by_its_own_setting() {
        numbers.combatEnabled = false;
        numbers.economyEnabled = true;
        verify(!numbers.accepts(tick({}), false), "combat is off");
        verify(numbers.accepts(tick({
                        "kind": "resource"
                    }), false), "economy is still on");
        numbers.combatEnabled = true;
        numbers.economyEnabled = false;
        verify(numbers.accepts(tick({}), false));
        verify(!numbers.accepts(tick({
                        "kind": "resource"
                    }), false));
        verify(!numbers.accepts(tick({
                        "kind": "reserve"
                    }), false));
        numbers.combatEnabled = true;
        numbers.economyEnabled = true;
    }

    function test_a_batch_of_every_kind_spawns_one_item_each() {
        testCase.pendingBatches = [[tick({}), tick({
                        "kind": "resource",
                        "amount": 8,
                        "resource": 2
                    }), tick({
                        "kind": "resource",
                        "amount": -40,
                        "resource": 0,
                        "pairedResource": 2,
                        "pairedAmount": 10
                    }), tick({
                        "kind": "reserve",
                        "amount": -18
                    })]];
        tryVerify(function () {
                return numbers.activeTicks === 4;
            }, 3000, "every kind reaches the layer");
        compare(numbers.activeBursts, 0);
    }

    function test_a_burst_goes_to_its_own_layer() {
        testCase.pendingBatches = [[tick({
                        "style": "burst",
                        "amount": 55,
                        "severity": 0.8,
                        "killingBlow": true
                    })]];
        tryVerify(function () {
                return numbers.activeBursts === 1;
            }, 3000, "the commander burst is not drawn as an RTS pill");
    }

    function test_the_tick_layer_refuses_more_than_its_cap() {
        var flood = [];
        for (var i = 0; i < numbers.maxTicks + 12; ++i)
            flood.push(tick({
                        "amount": 5 + i
                    }));
        testCase.pendingBatches = [flood];
        tryVerify(function () {
                return numbers.activeTicks > 0;
            }, 3000);
        verify(numbers.activeTicks <= numbers.maxTicks, "the cap held at " + numbers.activeTicks);
    }
}
