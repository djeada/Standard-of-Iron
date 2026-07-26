#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "app/core/user_settings.h"
#include "ui/preferences.h"
#include "ui/theme.h"

namespace {

namespace UserSettings = App::Core::UserSettings;

class WidgetThemeTest : public ::testing::Test {
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

TEST_F(WidgetThemeTest, CoversTheWidgetVocabularyTheToolsUse) {
  const QString sheet = Theme::widgetStyleSheet();

  for (const char* selector :
       {"QMainWindow", "QMenuBar",     "QToolBar",    "QStatusBar",   "QPushButton",
        "QToolButton", "QGroupBox",    "QComboBox",   "QSpinBox",     "QLineEdit",
        "QCheckBox",   "QRadioButton", "QListWidget", "QTreeWidget",  "QTableWidget",
        "QHeaderView", "QTabBar",      "QSlider",     "QProgressBar", "QDockWidget",
        "QSplitter",   "QScrollBar",   "QToolTip"}) {
    EXPECT_TRUE(sheet.contains(QLatin1String(selector)))
        << selector << " is not styled by the shared widget theme";
  }
}

TEST_F(WidgetThemeTest, EveryPlaceholderIsSubstituted) {
  const QString sheet = Theme::widgetStyleSheet();

  for (int slot = 1; slot <= 20; ++slot) {
    EXPECT_FALSE(sheet.contains(QStringLiteral("%%1").arg(slot)))
        << "unsubstituted placeholder %" << slot;
  }
}

TEST_F(WidgetThemeTest, FocusRingsAreDeclaredForKeyboardNavigation) {
  const QString sheet = Theme::widgetStyleSheet();
  EXPECT_TRUE(sheet.contains(QStringLiteral(":focus")))
      << "keyboard focus is invisible in the tools";
}

TEST_F(WidgetThemeTest, UiScaleGrowsTheToolChrome) {
  const QString base = Theme::widgetStyleSheet();
  EXPECT_TRUE(base.contains(QStringLiteral("font-size: 12px")));

  UiPreferences::instance()->set_ui_scale(2.0);
  const QString scaled = Theme::widgetStyleSheet();

  EXPECT_NE(base, scaled) << "the tools ignored the shared UI scale";
  EXPECT_TRUE(scaled.contains(QStringLiteral("font-size: 24px")));
}

TEST_F(WidgetThemeTest, HighContrastRepaintsTheToolSurfaces) {
  const QString base = Theme::widgetStyleSheet();

  UiPreferences::instance()->set_high_contrast(true);
  const QString contrast = Theme::widgetStyleSheet();

  EXPECT_NE(base, contrast) << "the tools ignored the shared high contrast setting";
  EXPECT_TRUE(contrast.contains(QStringLiteral("#050505")))
      << "high contrast did not reach the tool surface colour";
}

TEST_F(WidgetThemeTest, DefaultsAreRestoredWhenPreferencesReset) {
  const QString base = Theme::widgetStyleSheet();

  UiPreferences::instance()->set_ui_scale(1.75);
  UiPreferences::instance()->set_high_contrast(true);
  UiPreferences::instance()->reset_to_defaults();

  EXPECT_EQ(base, Theme::widgetStyleSheet());
}

} // namespace
