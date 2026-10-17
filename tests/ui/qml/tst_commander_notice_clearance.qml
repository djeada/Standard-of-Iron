import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0
import StandardOfIron.Core 1.0
import StandardOfIron.Design 1.0 as Design

TestCase {
    id: testCase

    name: "CommanderNoticeClearance"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    function commanderStatus() {
        return {
            "health_ratio": 0.6,
            "stamina_ratio": 0.4,
            "focus_target_name": "Hasdrubal Barca",
            "focus_target_hp": 40,
            "focus_target_max_hp": 220,
            "focus_target_hp_ratio": 0.18,
            "can_switch_weapon": true,
            "aura_available": true
        };
    }

    function init() {
        Design.Notifications.clear();
    }

    function cleanup() {
        Design.Notifications.clear();
    }

    function make_overlay(w, h) {
        var host = hostComponent.createObject(testCase, {
                "width": w,
                "height": h
            });
        var component = Qt.createComponent(Qt.resolvedUrl("../../../ui/qml/RpgFpvOverlay.qml"));
        compare(component.status, Component.Ready, "RpgFpvOverlay did not compile: " + component.errorString());
        var overlay = component.createObject(host, {
                "status": commanderStatus()
            });
        verify(overlay !== null, "overlay was not created");
        wait(1);
        return overlay;
    }

    function test_reserved_bottom_covers_the_commander_hud_band_data() {
        return [{
                "tag": "720p",
                "w": 1280,
                "h": 720
            }, {
                "tag": "1080p",
                "w": 1920,
                "h": 1080
            }, {
                "tag": "small",
                "w": 1024,
                "h": 600
            }];
    }

    function test_reserved_bottom_covers_the_commander_hud_band(data) {
        var overlay = make_overlay(data.w, data.h);
        verify(overlay.reservedBottom !== undefined, "the overlay must publish how much of the bottom edge it occupies");
        var names = ["rpgAbilityCooldowns", "rpgVitalsPlate", "rpgTargetPlate", "rpgWeaponStanceChip", "rpgAbilityTile_commander.rally"];
        for (var i = 0; i < names.length; ++i) {
            var item = findChild(overlay, names[i]);
            verify(item !== null, names[i] + " is missing");
            if (!item.visible)
                continue;
            var top = overlay.mapFromItem(item, 0, 0).y;
            verify(overlay.height - top <= overlay.reservedBottom, names[i] + " rises " + (overlay.height - top) + " px above the bottom edge but only " + overlay.reservedBottom + " px are reserved");
        }
        overlay.parent.destroy();
    }

    function test_notifications_stop_above_a_height_limit() {
        for (var i = 0; i < 3; ++i)
            Design.Notifications.info("Objective updated " + i);
        var host = notificationHost.createObject(testCase);
        wait(1);
        compare(host.fittingCount, 3, "without a limit every queued row shows");
        var oneRow = host.height / 3;
        host.maxHeight = oneRow * 1.5;
        wait(1);
        compare(host.fittingCount, 1, "only the rows that fit above the commander HUD may show");
        tryVerify(function () {
                return host.height <= host.maxHeight;
            }, 1000, "the notification column must end above the limit (" + host.height + " > " + host.maxHeight + ")");
        host.maxHeight = -1;
        wait(1);
        compare(host.fittingCount, 3, "held rows come back once there is room again");
        host.destroy();
    }

    function test_notifications_wait_instead_of_expiring_unseen() {
        Design.Notifications.info("Wave incoming");
        Design.Notifications.info("Objective complete");
        var host = notificationHost.createObject(testCase, {
                "maxHeight": 1
            });
        wait(1);
        compare(host.fittingCount, 0);
        tryCompare(host, "height", 0, 1000, "nothing may draw when no row fits");
        var row = findChild(host, "notificationRow");
        verify(row !== null, "the notification row is missing");
        verify(!row.dwelling, "a held row must not count down its dwell while hidden");
        host.maxHeight = -1;
        wait(1);
        verify(row.dwelling, "a row starts its dwell once it is shown");
        host.destroy();
    }

    Component {
        id: hostComponent

        Item {
        }
    }

    Component {
        id: notificationHost

        Design.NotificationHost {
        }
    }
}
