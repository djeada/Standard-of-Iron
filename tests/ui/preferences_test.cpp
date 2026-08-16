#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "app/core/user_settings.h"
#include "game/accessibility/motion_settings.h"
#include "game/accessibility/team_identity.h"
#include "ui/preferences.h"

namespace {

namespace UserSettings = App::Core::UserSettings;

class UiPreferencesTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    UserSettings::clear();
    UiPreferences::instance()->reset_to_defaults();
  }

  void TearDown() override {
    UiPreferences::instance()->reset_to_defaults();
    UserSettings::clear();
  }

  QTemporaryDir temp_dir_;
};

TEST_F(UiPreferencesTest, DefaultsMatchTheShippedPresentation) {
  auto* prefs = UiPreferences::instance();

  EXPECT_DOUBLE_EQ(prefs->ui_scale(), UserSettings::kDefaultUiScale);
  EXPECT_FALSE(prefs->reduced_motion());
  EXPECT_FALSE(prefs->high_contrast());
  EXPECT_EQ(prefs->color_vision_mode(), QStringLiteral("none"));
  EXPECT_FALSE(prefs->always_show_focus());
  EXPECT_FALSE(prefs->team_patterns());
  EXPECT_TRUE(prefs->edge_scroll_enabled());
  EXPECT_DOUBLE_EQ(prefs->edge_scroll_sensitivity(),
                   UserSettings::kDefaultEdgeScrollSensitivity);
  EXPECT_DOUBLE_EQ(prefs->camera_motion_scale(),
                   UserSettings::kDefaultCameraMotionScale);
  EXPECT_TRUE(prefs->damage_numbers());
  EXPECT_DOUBLE_EQ(prefs->screen_effect_intensity(),
                   UserSettings::kDefaultScreenEffectIntensity);
}

TEST_F(UiPreferencesTest, UiScaleIsPersistedAndClampedToTheSupportedRange) {
  auto* prefs = UiPreferences::instance();

  prefs->set_ui_scale(1.4);
  EXPECT_DOUBLE_EQ(UserSettings::load_ui_scale(), 1.4);

  prefs->set_ui_scale(9000.0);
  EXPECT_DOUBLE_EQ(prefs->ui_scale(), UserSettings::kMaxUiScale);
  EXPECT_DOUBLE_EQ(UserSettings::load_ui_scale(), UserSettings::kMaxUiScale);

  prefs->set_ui_scale(0.0);
  EXPECT_DOUBLE_EQ(prefs->ui_scale(), UserSettings::kMinUiScale);
}

TEST_F(UiPreferencesTest, NonFiniteUiScaleIsIgnored) {
  auto* prefs = UiPreferences::instance();
  prefs->set_ui_scale(1.25);

  prefs->set_ui_scale(std::numeric_limits<qreal>::quiet_NaN());

  EXPECT_DOUBLE_EQ(prefs->ui_scale(), 1.25);
}

TEST_F(UiPreferencesTest, AccessibilityTogglesRoundTripThroughSettings) {
  auto* prefs = UiPreferences::instance();

  prefs->set_reduced_motion(true);
  prefs->set_high_contrast(true);
  prefs->set_always_show_focus(true);

  EXPECT_TRUE(UserSettings::load_ui_reduced_motion());
  EXPECT_TRUE(UserSettings::load_ui_high_contrast());
  EXPECT_TRUE(UserSettings::load_ui_always_show_focus());
}

TEST_F(UiPreferencesTest, OnlySupportedColorVisionModesAreAccepted) {
  auto* prefs = UiPreferences::instance();

  prefs->set_color_vision_mode(QStringLiteral("Deuteranopia"));
  EXPECT_EQ(prefs->color_vision_mode(), QStringLiteral("deuteranopia"));

  prefs->set_color_vision_mode(QStringLiteral("tetrachromacy"));
  EXPECT_EQ(prefs->color_vision_mode(), QStringLiteral("deuteranopia"));
  EXPECT_EQ(UserSettings::load_ui_color_vision_mode(), QStringLiteral("deuteranopia"));
}

TEST_F(UiPreferencesTest, EveryAdvertisedColorVisionModeIsAccepted) {
  auto* prefs = UiPreferences::instance();

  for (const QString& mode : UiPreferences::color_vision_modes()) {
    prefs->set_color_vision_mode(mode);
    EXPECT_EQ(prefs->color_vision_mode(), mode);
  }
}

TEST_F(UiPreferencesTest, CorruptedStoredValuesFallBackToDefaults) {
  {
    auto settings = UserSettings::open();
    settings.setValue(QString::fromLatin1(UserSettings::kUiScaleKey),
                      QStringLiteral("enormous"));
    settings.setValue(QString::fromLatin1(UserSettings::kUiColorVisionKey),
                      QStringLiteral("not-a-mode"));
    settings.sync();
  }

  EXPECT_DOUBLE_EQ(UserSettings::load_ui_scale(), UserSettings::kDefaultUiScale);
  EXPECT_EQ(UserSettings::load_ui_color_vision_mode(), QStringLiteral("none"));
}

TEST_F(UiPreferencesTest, GameplayAccessibilityOptionsRoundTripThroughSettings) {
  auto* prefs = UiPreferences::instance();

  prefs->set_edge_scroll_enabled(false);
  prefs->set_edge_scroll_sensitivity(0.5);
  prefs->set_camera_motion_scale(0.25);
  prefs->set_damage_numbers(false);
  prefs->set_screen_effect_intensity(0.0);
  prefs->set_team_patterns(true);

  EXPECT_FALSE(UserSettings::load_ui_edge_scroll_enabled());
  EXPECT_DOUBLE_EQ(UserSettings::load_ui_edge_scroll_sensitivity(), 0.5);
  EXPECT_DOUBLE_EQ(UserSettings::load_ui_camera_motion_scale(), 0.25);
  EXPECT_FALSE(UserSettings::load_ui_damage_numbers());
  EXPECT_DOUBLE_EQ(UserSettings::load_ui_screen_effect_intensity(), 0.0);
  EXPECT_TRUE(UserSettings::load_ui_team_patterns());
}

TEST_F(UiPreferencesTest, TheCameraLegendIsShownOnceAndThenRemembered) {
  auto* prefs = UiPreferences::instance();

  EXPECT_FALSE(prefs->camera_legend_seen())
      << "a fresh profile should be offered the camera legend";
  EXPECT_FALSE(UserSettings::load_ui_camera_legend_seen());

  prefs->set_camera_legend_seen(true);

  EXPECT_TRUE(prefs->camera_legend_seen());
  EXPECT_TRUE(UserSettings::load_ui_camera_legend_seen());

  prefs->reset_to_defaults();
  EXPECT_FALSE(prefs->camera_legend_seen());
}

TEST_F(UiPreferencesTest, SlidersAreClampedToTheirSupportedRange) {
  auto* prefs = UiPreferences::instance();

  prefs->set_edge_scroll_sensitivity(50.0);
  EXPECT_DOUBLE_EQ(prefs->edge_scroll_sensitivity(),
                   UserSettings::kMaxEdgeScrollSensitivity);

  prefs->set_camera_motion_scale(-3.0);
  EXPECT_DOUBLE_EQ(prefs->camera_motion_scale(), 0.0);

  prefs->set_screen_effect_intensity(9.0);
  EXPECT_DOUBLE_EQ(prefs->screen_effect_intensity(), 1.0);
}

TEST_F(UiPreferencesTest, ChoosingAColorVisionModeAlsoTurnsOnTheRingPatterns) {
  auto* prefs = UiPreferences::instance();
  ASSERT_FALSE(prefs->effective_team_patterns());

  prefs->set_color_vision_mode(QStringLiteral("protanopia"));

  EXPECT_TRUE(prefs->effective_team_patterns());
  EXPECT_FALSE(prefs->team_patterns());

  prefs->set_color_vision_mode(QStringLiteral("none"));
  EXPECT_FALSE(prefs->effective_team_patterns());
}

TEST_F(UiPreferencesTest, PreferencesReachTheLayersThatCannotReadSettings) {
  namespace Accessibility = Game::Accessibility;
  auto* prefs = UiPreferences::instance();

  prefs->set_color_vision_mode(QStringLiteral("tritanopia"));
  EXPECT_EQ(Accessibility::TeamIdentity::palette_variant(),
            Accessibility::PaletteVariant::Tritanopia);
  EXPECT_TRUE(Accessibility::TeamIdentity::patterns_enabled());

  prefs->set_camera_motion_scale(0.4);
  EXPECT_FLOAT_EQ(Accessibility::MotionSettings::camera_motion_scale(), 0.4F);

  prefs->reset_to_defaults();
  EXPECT_EQ(Accessibility::TeamIdentity::palette_variant(),
            Accessibility::PaletteVariant::Standard);
  EXPECT_FALSE(Accessibility::TeamIdentity::patterns_enabled());
  EXPECT_FLOAT_EQ(Accessibility::MotionSettings::camera_motion_scale(), 1.0F);
}

TEST_F(UiPreferencesTest, CorruptedGameplayOptionsFallBackToDefaults) {
  {
    auto settings = UserSettings::open();
    settings.setValue(QString::fromLatin1(UserSettings::kUiEdgeScrollSensitivityKey),
                      QStringLiteral("fast"));
    settings.setValue(QString::fromLatin1(UserSettings::kUiCameraMotionKey), -12.0);
    settings.sync();
  }

  EXPECT_DOUBLE_EQ(UserSettings::load_ui_edge_scroll_sensitivity(),
                   UserSettings::kDefaultEdgeScrollSensitivity);
  EXPECT_DOUBLE_EQ(UserSettings::load_ui_camera_motion_scale(),
                   UserSettings::kDefaultCameraMotionScale);
}

TEST_F(UiPreferencesTest, ChangeSignalsFireOnlyOnRealChanges) {
  auto* prefs = UiPreferences::instance();

  QObject context;
  int scale_changes = 0;
  QObject::connect(prefs,
                   &UiPreferences::ui_scale_changed,
                   &context,
                   [&scale_changes]() { ++scale_changes; });

  prefs->set_ui_scale(1.5);
  prefs->set_ui_scale(1.5);

  EXPECT_EQ(scale_changes, 1);
}

} // namespace
