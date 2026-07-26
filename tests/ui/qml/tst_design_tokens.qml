import QtQuick 2.15
import QtTest 1.15
import StandardOfIron 1.0 as Core
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
        verify(Typography.body < Typography.heading);
        verify(Typography.heading < Typography.title);
        verify(Typography.title < Typography.hero);
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

    function test_status_color_lookup_covers_the_semantic_names() {
        compare(Theme.statusColor("success").toString(), Theme.success.toString());
        compare(Theme.statusColor("danger").toString(), Theme.danger.toString());
        compare(Theme.statusColor("warning").toString(), Theme.warning.toString());
        compare(Theme.statusColor("selected").toString(), Theme.selection.toString());
        compare(Theme.statusColor("something-new").toString(), Theme.accent.toString());
    }
}
