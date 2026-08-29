#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <limits>

#include "app/core/user_settings.h"
#include "game/accessibility/motion_settings.h"
#include "game/accessibility/team_identity.h"
#include "game/render_bridge/camera_speeds.h"
#include "ui/hints.h"
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
  EXPECT_EQ(prefs->display_window_mode(),
            QString::fromLatin1(UserSettings::kDefaultDisplayWindowMode));
  EXPECT_TRUE(prefs->display_vsync());
  EXPECT_FALSE(prefs->show_fps());
  EXPECT_DOUBLE_EQ(prefs->camera_pan_speed(), UserSettings::kDefaultCameraSpeedScale);
  EXPECT_DOUBLE_EQ(prefs->camera_zoom_speed(), UserSettings::kDefaultCameraSpeedScale);
  EXPECT_DOUBLE_EQ(prefs->camera_rotation_speed(),
                   UserSettings::kDefaultCameraSpeedScale);
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
    settings.setValue(QString::fromLatin1(UserSettings::kDisplayWindowModeKey),
                      QStringLiteral("cinema"));
    settings.sync();
  }

  EXPECT_DOUBLE_EQ(UserSettings::load_ui_scale(), UserSettings::kDefaultUiScale);
  EXPECT_EQ(UserSettings::load_ui_color_vision_mode(), QStringLiteral("none"));
  EXPECT_EQ(UserSettings::load_display_window_mode(),
            QString::fromLatin1(UserSettings::kDefaultDisplayWindowMode));
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

TEST_F(UiPreferencesTest, ResettingPreferencesOffersEverySuppressedHintAgain) {
  auto* prefs = UiPreferences::instance();
  auto* hints = UiHints::instance();

  hints->suppress(QStringLiteral("formation_readout"));
  ASSERT_FALSE(hints->is_enabled(QStringLiteral("formation_readout")));

  prefs->reset_to_defaults();

  EXPECT_TRUE(hints->is_enabled(QStringLiteral("formation_readout")))
      << "a settings reset must offer the prompts again";
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

TEST_F(UiPreferencesTest, DisplayOptionsRoundTripThroughSettings) {
  auto* prefs = UiPreferences::instance();

  prefs->set_display_window_mode(QStringLiteral("Windowed"));
  EXPECT_EQ(prefs->display_window_mode(), QStringLiteral("windowed"));
  EXPECT_EQ(UserSettings::load_display_window_mode(), QStringLiteral("windowed"));

  prefs->set_display_window_mode(QStringLiteral("holographic"));
  EXPECT_EQ(prefs->display_window_mode(), QStringLiteral("windowed"));
  EXPECT_EQ(UserSettings::load_display_window_mode(), QStringLiteral("windowed"));

  prefs->set_display_vsync(false);
  EXPECT_FALSE(UserSettings::load_display_vsync());

  prefs->set_show_fps(true);
  EXPECT_TRUE(UserSettings::load_ui_show_fps());
}

TEST_F(UiPreferencesTest, CameraSpeedScalesRoundTripThroughSettings) {
  auto* prefs = UiPreferences::instance();

  prefs->set_camera_pan_speed(1.75);
  prefs->set_camera_zoom_speed(0.5);
  prefs->set_camera_rotation_speed(2.0);

  EXPECT_DOUBLE_EQ(UserSettings::load_camera_pan_speed_scale(), 1.75);
  EXPECT_DOUBLE_EQ(UserSettings::load_camera_zoom_speed_scale(), 0.5);
  EXPECT_DOUBLE_EQ(UserSettings::load_camera_rotation_speed_scale(), 2.0);
}

TEST_F(UiPreferencesTest, CameraSpeedScalesAreClampedAndReachTheCameraService) {
  namespace CameraSpeeds = Game::Systems::CameraSpeeds;
  auto* prefs = UiPreferences::instance();

  prefs->set_camera_pan_speed(50.0);
  EXPECT_DOUBLE_EQ(prefs->camera_pan_speed(), UserSettings::kMaxCameraSpeedScale);
  EXPECT_FLOAT_EQ(CameraSpeeds::pan_scale(),
                  static_cast<float>(UserSettings::kMaxCameraSpeedScale));

  prefs->set_camera_zoom_speed(-3.0);
  EXPECT_DOUBLE_EQ(prefs->camera_zoom_speed(), UserSettings::kMinCameraSpeedScale);

  prefs->set_camera_rotation_speed(1.5);
  EXPECT_FLOAT_EQ(CameraSpeeds::rotation_scale(), 1.5F);

  prefs->reset_to_defaults();
  EXPECT_FLOAT_EQ(CameraSpeeds::pan_scale(),
                  static_cast<float>(UserSettings::kDefaultCameraSpeedScale));
  EXPECT_FLOAT_EQ(CameraSpeeds::zoom_scale(),
                  static_cast<float>(UserSettings::kDefaultCameraSpeedScale));
  EXPECT_FLOAT_EQ(CameraSpeeds::rotation_scale(),
                  static_cast<float>(UserSettings::kDefaultCameraSpeedScale));
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

TEST_F(UiPreferencesTest, DamageNumbersDefaultToShowingEveryHit) {
  auto* prefs = UiPreferences::instance();
  EXPECT_EQ(prefs->damage_number_mode(), QStringLiteral("all"));
  EXPECT_TRUE(prefs->damage_numbers());
}

TEST_F(UiPreferencesTest, DamageNumbersOfferOffImportantAndAll) {
  const QStringList modes = UiPreferences::damage_number_modes();
  EXPECT_EQ(modes,
            (QStringList{QStringLiteral("off"),
                         QStringLiteral("important"),
                         QStringLiteral("all")}));
}

TEST_F(UiPreferencesTest, TheImportantOnlyModeStillCountsAsDamageNumbersOn) {
  auto* prefs = UiPreferences::instance();
  prefs->set_damage_number_mode(QStringLiteral("important"));

  EXPECT_EQ(prefs->damage_number_mode(), QStringLiteral("important"));
  EXPECT_TRUE(prefs->damage_numbers());
}

TEST_F(UiPreferencesTest, TurningDamageNumbersOffSelectsTheOffMode) {
  auto* prefs = UiPreferences::instance();
  prefs->set_damage_numbers(false);

  EXPECT_EQ(prefs->damage_number_mode(), QStringLiteral("off"));
  EXPECT_FALSE(prefs->damage_numbers());
}

TEST_F(UiPreferencesTest, AnUnknownDamageNumberModeIsIgnored) {
  auto* prefs = UiPreferences::instance();
  prefs->set_damage_number_mode(QStringLiteral("important"));
  prefs->set_damage_number_mode(QStringLiteral("occasionally"));

  EXPECT_EQ(prefs->damage_number_mode(), QStringLiteral("important"));
}

TEST_F(UiPreferencesTest, TheDamageNumberModeSurvivesAReload) {
  UiPreferences::instance()->set_damage_number_mode(QStringLiteral("important"));
  EXPECT_EQ(UserSettings::load_ui_damage_number_mode(), QStringLiteral("important"));

  UiPreferences::instance()->set_damage_number_mode(QStringLiteral("off"));
  EXPECT_EQ(UserSettings::load_ui_damage_number_mode(), QStringLiteral("off"));
  EXPECT_FALSE(UserSettings::load_ui_damage_numbers());
}
