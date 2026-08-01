#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "app/core/user_settings.h"
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
