#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>

#include <gtest/gtest.h>

#include "app/core/language_manager.h"
#include "app/core/user_settings.h"

namespace {

QStringList g_warnings;

void capture_warnings(QtMsgType type,
                      const QMessageLogContext&,
                      const QString& message) {
  if (type == QtWarningMsg) {
    g_warnings << message;
  }
}

class LanguageOverrideTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    App::Core::UserSettings::clear();
    g_warnings.clear();
  }

  void TearDown() override { App::Core::UserSettings::clear(); }

  QTemporaryDir temp_dir_;
};

TEST_F(LanguageOverrideTest, OverrideIsAppliedWithoutChangingTheSavedLanguage) {
  App::Core::UserSettings::save_language(QStringLiteral("en"));
  LanguageManager language_manager;
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("en"));

  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("de")));

  EXPECT_EQ(language_manager.current_language(), QStringLiteral("de"));
  EXPECT_TRUE(language_manager.is_language_temporary());
  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QStringLiteral("Kriegselefant"));
  EXPECT_EQ(App::Core::UserSettings::load_language(), QStringLiteral("en"));
}

TEST_F(LanguageOverrideTest, OverrideOnAFreshProfileWritesNoLanguage) {
  LanguageManager language_manager;

  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("de")));

  EXPECT_FALSE(App::Core::UserSettings::load_language().has_value());
}

TEST_F(LanguageOverrideTest, SavingOtherSettingsDoesNotWriteTheOverrideBack) {
  App::Core::UserSettings::save_language(QStringLiteral("en"));
  LanguageManager language_manager;
  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("de")));

  App::Core::UserSettings::save_ui_scale(1.25);
  App::Core::UserSettings::save_display_vsync(false);
  App::Core::UserSettings::save_ui_high_contrast(true);

  EXPECT_EQ(App::Core::UserSettings::load_language(), QStringLiteral("en"));
}

TEST_F(LanguageOverrideTest, RegionalCodesAreMatchedCaseInsensitively) {
  LanguageManager language_manager;

  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("pt_BR")));
  EXPECT_EQ(language_manager.current_language(), QStringLiteral("pt_br"));

  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral(" DE ")));
  EXPECT_EQ(language_manager.current_language(), QStringLiteral("de"));

  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("pt-br")));
  EXPECT_EQ(language_manager.current_language(), QStringLiteral("pt_br"));
}

TEST_F(LanguageOverrideTest, UnsupportedLanguageWarnsAndKeepsTheSavedLanguage) {
  App::Core::UserSettings::save_language(QStringLiteral("pl"));
  LanguageManager language_manager;
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("pl"));

  const QtMessageHandler previous = qInstallMessageHandler(capture_warnings);
  const bool applied =
      language_manager.apply_temporary_language(QStringLiteral("xx_YY"));
  qInstallMessageHandler(previous);

  EXPECT_FALSE(applied);
  EXPECT_EQ(language_manager.current_language(), QStringLiteral("pl"));
  EXPECT_FALSE(language_manager.is_language_temporary());
  EXPECT_EQ(App::Core::UserSettings::load_language(), QStringLiteral("pl"));
  ASSERT_EQ(g_warnings.size(), 1);
  EXPECT_TRUE(g_warnings.first().contains(QStringLiteral("xx_YY")));
  for (const auto& code : language_manager.available_languages()) {
    EXPECT_TRUE(g_warnings.first().contains(code))
        << "the warning should list the supported language " << code.toStdString();
  }
}

TEST_F(LanguageOverrideTest, ChoosingALanguageInSettingsStillPersists) {
  App::Core::UserSettings::save_language(QStringLiteral("en"));
  LanguageManager language_manager;
  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("de")));

  language_manager.set_language(QStringLiteral("es"));

  EXPECT_EQ(language_manager.current_language(), QStringLiteral("es"));
  EXPECT_FALSE(language_manager.is_language_temporary());
  EXPECT_EQ(App::Core::UserSettings::load_language(), QStringLiteral("es"));
}

TEST_F(LanguageOverrideTest, ConfirmingTheOverriddenLanguageInSettingsPersistsIt) {
  App::Core::UserSettings::save_language(QStringLiteral("en"));
  LanguageManager language_manager;
  ASSERT_TRUE(language_manager.apply_temporary_language(QStringLiteral("de")));

  language_manager.set_language(QStringLiteral("de"));

  EXPECT_FALSE(language_manager.is_language_temporary());
  EXPECT_EQ(App::Core::UserSettings::load_language(), QStringLiteral("de"));
}

} // namespace
