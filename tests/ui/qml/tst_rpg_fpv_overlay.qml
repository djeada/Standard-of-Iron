import QtQuick 2.15
import QtTest 1.15

TestCase {
    id: testCase

    name: "RpgFpvOverlay"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    function loudStatus() {
        return {
            "health": 12,
            "max_health": 150,
            "health_ratio": 0.08,
            "stamina_ratio": 0.12,
            "posture_ratio": 0.92,
            "combo_step": 4,
            "finisher_ready": true,
            "punish_active": true,
            "guard_active": true,
            "guard_broken": true,
            "perfect_guard_active": true,
            "dodge_active": true,
            "is_attacking": true,
            "attack_direction": 2,
            "aim_candidate_in_range": true,
            "focus_marker_valid": true,
            "focus_marker_locked": true,
            "locked_target_name": "Hasdrubal Barca",
            "shield_bash_ready": false,
            "shield_bash_cooldown": 3.0,
            "shield_bash_cooldown_remaining": 2.4,
            "vanguard_rush_ready": false,
            "vanguard_rush_cooldown": 4.5,
            "vanguard_rush_cooldown_remaining": 3.9,
            "second_wind_ready": false,
            "second_wind_cooldown": 8.0,
            "second_wind_cooldown_remaining": 7.1
        };
    }

    property var overlayComponent: null

    function overlay_component() {
        if (overlayComponent === null) {
            overlayComponent = Qt.createComponent(Qt.resolvedUrl("../../../ui/qml/RpgFpvOverlay.qml"));
            compare(overlayComponent.status, Component.Ready, "RpgFpvOverlay did not compile: " + overlayComponent.errorString());
        }
        return overlayComponent;
    }

    function makeOverlay(w, h, status) {
        var host = hostComponent.createObject(testCase, {
                "width": w,
                "height": h
            });
        verify(host !== null, "overlay host was not created");
        var overlay = overlay_component().createObject(host, {
                "status": status
            });
        verify(overlay !== null, "overlay was not created");
        host.overlay = overlay;
        wait(1);
        return host;
    }

    function collectVisibleItems(item, out) {
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];
            if (child === null || child.visible === false) {
                continue;
            }
            if (child.width !== undefined && child.height !== undefined) {
                out.push(child);
            }
            collectVisibleItems(child, out);
        }
    }

    function rectIn(root, item) {
        var topLeft = root.mapFromItem(item, 0, 0);
        return {
            "left": topLeft.x,
            "top": topLeft.y,
            "right": topLeft.x + item.width,
            "bottom": topLeft.y + item.height
        };
    }

    function overlaps(a, b) {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }

    function checkNoOverflow(w, h) {
        var host = makeOverlay(w, h, loudStatus());
        var overlay = host.overlay;
        var items = [];
        collectVisibleItems(overlay, items);
        verify(items.length > 0, "overlay produced no visible items");
        var tolerance = 1.0;
        for (var i = 0; i < items.length; ++i) {
            var item = items[i];
            if (item.width >= overlay.width || item.height >= overlay.height) {
                continue;
            }
            var r = rectIn(overlay, item);
            verify(r.left >= -tolerance, "item " + i + " overflows the left edge at " + w + "x" + h + " (" + r.left + ")");
            verify(r.top >= -tolerance, "item " + i + " overflows the top edge at " + w + "x" + h + " (" + r.top + ")");
            verify(r.right <= overlay.width + tolerance, "item " + i + " overflows the right edge at " + w + "x" + h + " (" + r.right + " > " + overlay.width + ")");
            verify(r.bottom <= overlay.height + tolerance, "item " + i + " overflows the bottom edge at " + w + "x" + h + " (" + r.bottom + " > " + overlay.height + ")");
        }
        host.destroy();
    }

    function test_overlay_never_overflows_the_viewport_data() {
        return [{
                "tag": "1920x1080",
                "w": 1920,
                "h": 1080
            }, {
                "tag": "1280x720",
                "w": 1280,
                "h": 720
            }, {
                "tag": "1024x768",
                "w": 1024,
                "h": 768
            }, {
                "tag": "800x600",
                "w": 800,
                "h": 600
            }, {
                "tag": "640x480",
                "w": 640,
                "h": 480
            }];
    }

    function test_overlay_never_overflows_the_viewport(data) {
        checkNoOverflow(data.w, data.h);
    }

    function test_bottom_bands_never_collide_data() {
        return test_overlay_never_overflows_the_viewport_data();
    }

    function test_bottom_bands_never_collide(data) {
        var host = makeOverlay(data.w, data.h, loudStatus());
        var overlay = host.overlay;
        var bars = findChild(overlay, "rpgHudBarsRow");
        var abilities = findChild(overlay, "rpgAbilityCooldowns");
        var posture = findChild(overlay, "rpgPostureBar");
        var combo = findChild(overlay, "rpgComboIndicator");
        verify(bars !== null, "hud bars row is missing");
        verify(abilities !== null, "ability cooldown row is missing");
        verify(posture !== null, "posture bar is missing");
        verify(combo !== null, "combo indicator is missing");
        var barsRect = rectIn(overlay, bars);
        var abilitiesRect = rectIn(overlay, abilities);
        verify(!overlaps(barsRect, abilitiesRect), "vitals bars collide with the ability row at " + data.tag);
        verify(!overlaps(rectIn(overlay, posture), abilitiesRect), "posture bar collides with the ability row at " + data.tag);
        verify(!overlaps(rectIn(overlay, combo), abilitiesRect), "combo indicator collides with the ability row at " + data.tag);
        host.destroy();
    }

    function visibleDirectChildCount(overlay) {
        var count = 0;
        for (var i = 0; i < overlay.children.length; ++i) {
            var child = overlay.children[i];
            if (child === null || child.visible !== true) {
                continue;
            }
            if (child.opacity !== undefined && child.opacity <= 0.0) {
                continue;
            }
            count += 1;
        }
        return count;
    }

    function quietStatus() {
        return {
            "health": 150,
            "max_health": 150,
            "health_ratio": 1.0,
            "stamina_ratio": 1.0,
            "posture_ratio": 0.0,
            "combo_step": 0,
            "finisher_ready": false,
            "punish_active": false,
            "guard_active": false,
            "guard_broken": false,
            "perfect_guard_active": false,
            "dodge_active": false,
            "is_attacking": false,
            "aim_candidate_in_range": false,
            "focus_marker_valid": false,
            "focus_marker_locked": false,
            "locked_target_name": "",
            "shield_bash_ready": true,
            "vanguard_rush_ready": true,
            "second_wind_ready": true
        };
    }

    function test_each_state_flag_raises_at_most_one_new_element_data() {
        return [{
                "tag": "finisher_ready",
                "flag": "finisher_ready"
            }, {
                "tag": "punish_active",
                "flag": "punish_active"
            }, {
                "tag": "guard_active",
                "flag": "guard_active"
            }, {
                "tag": "guard_broken",
                "flag": "guard_broken"
            }, {
                "tag": "perfect_guard_active",
                "flag": "perfect_guard_active"
            }, {
                "tag": "is_attacking",
                "flag": "is_attacking"
            }, {
                "tag": "focus_marker_locked",
                "flag": "focus_marker_locked"
            }];
    }

    function test_an_attack_fires_a_single_centre_impulse() {
        var host = makeOverlay(1280, 720, quietStatus());
        var overlay = host.overlay;
        var baseline = visibleDirectChildCount(overlay);
        var swinging = quietStatus();
        swinging["is_attacking"] = true;
        overlay.status = swinging;
        wait(1);
        var lit = visibleDirectChildCount(overlay);
        verify(lit - baseline <= 2, "an attack lights " + (lit - baseline) + " overlay elements; expected the combat frame plus one impulse");
        host.destroy();
    }

    function test_each_state_flag_raises_at_most_one_new_element(data) {
        var host = makeOverlay(1280, 720, quietStatus());
        var overlay = host.overlay;
        var baseline = visibleDirectChildCount(overlay);
        var raised = quietStatus();
        raised[data.flag] = true;
        overlay.status = raised;
        wait(600);
        var lit = visibleDirectChildCount(overlay);
        verify(lit - baseline <= 1, data.flag + " lights " + (lit - baseline) + " new overlay elements; " + "one state may only raise one signal");
        host.destroy();
    }

    Component {
        id: hostComponent

        Item {
            property var overlay: null
        }
    }
}
