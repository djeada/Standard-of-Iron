import QtQuick 2.15
import QtTest 1.15
import StandardOfIron.Core 1.0 as Core
import StandardOfIron.Design 1.0

TestCase {
    id: testCase

    name: "DesignTokens"

    function init() {
        Core.UiPreferences.reset_to_defaults();
    }

    function cleanupTestCase() {
        Core.UiPreferences.reset_to_defaults();
    }

    function test_module_exposes_every_documented_singleton() {
        verify(Theme !== undefined, "Theme singleton missing");
        verify(Metrics !== undefined, "Metrics singleton missing");
        verify(Typography !== undefined, "Typography singleton missing");
        verify(Motion !== undefined, "Motion singleton missing");
        verify(Icons !== undefined, "Icons singleton missing");
        verify(A11y !== undefined, "A11y singleton missing");
        verify(FactionTheme !== undefined, "FactionTheme singleton missing");
        verify(Notifications !== undefined, "Notifications singleton missing");
    }

    function test_spacing_scale_is_monotonic() {
        verify(Metrics.space2 < Metrics.space4);
        verify(Metrics.space4 < Metrics.space8);
        verify(Metrics.space8 < Metrics.space12);
        verify(Metrics.space12 < Metrics.space16);
        verify(Metrics.space16 < Metrics.space24);
        verify(Metrics.space24 < Metrics.space32);
    }

    function test_type_scale_is_monotonic() {
        verify(Typography.caption < Typography.label);
        verify(Typography.label < Typography.body);
        verify(Typography.body < Typography.bodyLarge);
        verify(Typography.bodyLarge < Typography.subheading);
        verify(Typography.subheading < Typography.heading);
        verify(Typography.heading < Typography.title);
        verify(Typography.title < Typography.hero);
    }

    function test_glyph_scale_is_monotonic() {
        verify(Typography.glyphSmall < Typography.glyph);
        verify(Typography.glyph < Typography.glyphLarge);
    }

    function test_gameplay_type_never_falls_below_the_legibility_floor() {
        Core.UiPreferences.uiScale = Core.UiPreferences.minUiScale;
        verify(Typography.minimumSize >= 12, "the floor itself is under 12px");
        verify(Typography.caption >= Typography.minimumSize, "caption fell under the floor");
        verify(Typography.label >= Typography.minimumSize, "label fell under the floor");
        verify(Typography.body >= Typography.minimumSize, "body fell under the floor");
        compare(Typography.scaled(4), Typography.minimumSize);
    }

    function test_display_sizes_follow_the_scale_without_the_floor() {
        Core.UiPreferences.uiScale = 1.0;
        compare(Typography.display(54), 54);
        Core.UiPreferences.uiScale = 2.0;
        compare(Typography.display(54), 108);
    }

    function test_ui_scale_resizes_spacing_and_type() {
        var baseSpace = Metrics.space16;
        var baseBody = Typography.body;
        var baseControl = Metrics.controlHeight;
        Core.UiPreferences.uiScale = 1.5;
        compare(A11y.uiScale, 1.5);
        verify(Metrics.space16 > baseSpace, "spacing did not follow the UI scale");
        verify(Typography.body > baseBody, "type did not follow the UI scale");
        verify(Metrics.controlHeight > baseControl, "controls did not follow the UI scale");
    }

    function test_touch_targets_never_shrink_below_the_floor() {
        Core.UiPreferences.uiScale = Core.UiPreferences.minUiScale;
        verify(Metrics.minTouchTarget >= 32, "touch target fell under the 32px floor");
    }

    function test_ui_scale_is_clamped_to_the_supported_range() {
        Core.UiPreferences.uiScale = 99.0;
        compare(A11y.uiScale, Core.UiPreferences.maxUiScale);
        Core.UiPreferences.uiScale = 0.01;
        compare(A11y.uiScale, Core.UiPreferences.minUiScale);
    }

    function test_reduced_motion_collapses_every_duration() {
        verify(Motion.normal > 0, "durations should be non-zero by default");
        Core.UiPreferences.reducedMotion = true;
        compare(Motion.fast, 0);
        compare(Motion.normal, 0);
        compare(Motion.deliberate, 0);
        compare(Motion.cinematic, 0);
        verify(!Motion.allowAmbientLoops, "ambient loops must stop under reduced motion");
    }

    function test_reduced_motion_keeps_notification_dwell_times() {
        Core.UiPreferences.reducedMotion = true;
        verify(Motion.dwellFor("critical") > 0);
        verify(Motion.dwellFor("critical") > Motion.dwellFor("info"));
    }

    function test_high_contrast_changes_the_surface_palette() {
        var base = Theme.backgroundDeep.toString();
        Core.UiPreferences.highContrast = true;
        verify(Theme.highContrast, "Theme did not follow the high contrast preference");
        verify(Theme.backgroundDeep.toString() !== base, "high contrast reused the default background");
    }

    function test_red_green_modes_repaint_status_colors() {
        var defaultSuccess = Theme.success.toString();
        var defaultDanger = Theme.danger.toString();
        Core.UiPreferences.colorVisionMode = "deuteranopia";
        verify(A11y.redGreenImpaired);
        verify(Theme.success.toString() !== defaultSuccess, "success stayed green for a red/green deficiency");
        verify(Theme.danger.toString() !== defaultDanger, "danger stayed red for a red/green deficiency");
        verify(Theme.success.toString() !== Theme.danger.toString(), "success and danger collapsed to one colour");
    }

    function test_unknown_color_vision_mode_is_rejected() {
        Core.UiPreferences.colorVisionMode = "not-a-mode";
        compare(A11y.colorVisionMode, "none");
    }

    function test_the_rts_bottom_bar_grows_with_the_viewport_data() {
        return [{
                "tag": "720p",
                "height": 720,
                "expected": 150
            }, {
                "tag": "1080p",
                "height": 1080,
                "expected": 183.6
            }, {
                "tag": "1440p",
                "height": 1440,
                "expected": 200
            }, {
                "tag": "tiny",
                "height": 480,
                "expected": 150
            }];
    }

    function test_the_rts_bottom_bar_grows_with_the_viewport(data) {
        fuzzyCompare(Metrics.bottomBarHeight(data.height, false), data.expected, 0.5, data.tag);
    }

    function test_the_rts_bottom_bar_leaves_the_battlefield_the_screen_data() {
        return [{
                "tag": "720p",
                "height": 720
            }, {
                "tag": "900p",
                "height": 900
            }, {
                "tag": "1080p",
                "height": 1080
            }, {
                "tag": "1440p",
                "height": 1440
            }, {
                "tag": "2160p",
                "height": 2160
            }];
    }

    function test_the_rts_bottom_bar_leaves_the_battlefield_the_screen(data) {
        var barHeight = Metrics.bottomBarHeight(data.height, false);
        verify(barHeight <= data.height * 0.28, "the bottom bar ate more than a quarter of the battlefield at " + data.tag + ", it was " + barHeight);
        verify(barHeight >= Metrics.orderButtonSize * 2 + Metrics.compactControlHeight, "the bottom bar is too short to seat two rows of order tiles at " + data.tag + ", it was " + barHeight);
    }

    function test_commander_mode_keeps_a_slim_bar() {
        var rts = Metrics.bottomBarHeight(1080, false);
        var commander = Metrics.bottomBarHeight(1080, true);
        verify(commander < rts, "direct commander control should not carry the RTS panels");
        verify(commander <= Metrics.commanderBottomBarMaxHeight, "the commander bar overran its ceiling");
        verify(commander >= Metrics.commanderBottomBarMinHeight, "the commander bar fell under its floor");
    }

    function test_status_color_lookup_covers_the_semantic_names() {
        compare(Theme.statusColor("success").toString(), Theme.success.toString());
        compare(Theme.statusColor("danger").toString(), Theme.danger.toString());
        compare(Theme.statusColor("warning").toString(), Theme.warning.toString());
        compare(Theme.statusColor("selected").toString(), Theme.selection.toString());
        compare(Theme.statusColor("something-new").toString(), Theme.accent.toString());
    }
}
