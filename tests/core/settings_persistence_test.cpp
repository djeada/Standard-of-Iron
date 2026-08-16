#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#define private public
#include "game/audio/audio_system.h"
#undef private

#include "app/core/language_manager.h"
#include "app/core/user_settings.h"
#include "app/models/graphics_settings_proxy.h"
#include "render/graphics_settings.h"

namespace {

class SettingsPersistenceTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    App::Core::UserSettings::clear();
    Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Ultra);
    AudioSystem::get_instance().load_persisted_volumes();
  }

  void TearDown() override {
    App::Core::UserSettings::clear();
    Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Ultra);
    AudioSystem::get_instance().load_persisted_volumes();
  }

  QTemporaryDir temp_dir_;
};

TEST_F(SettingsPersistenceTest, GraphicsQualitySelectionIsSavedAndRestored) {
  App::Models::GraphicsSettingsProxy proxy;

  proxy.set_quality_level(1);

  const auto saved_level = App::Core::UserSettings::load_graphics_quality_level();
  ASSERT_TRUE(saved_level.has_value());
  EXPECT_EQ(*saved_level, 1);

  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Ultra);
  App::Core::UserSettings::apply_saved_graphics_quality();
  EXPECT_EQ(Render::GraphicsSettings::instance().quality(),
            Render::GraphicsQuality::Medium);
}

TEST_F(SettingsPersistenceTest, FreshProfileDefaultsToUltraGraphics) {
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::High);

  App::Core::UserSettings::apply_saved_graphics_quality();

  EXPECT_EQ(Render::GraphicsSettings::instance().quality(),
            Render::GraphicsQuality::Ultra);
}

TEST_F(SettingsPersistenceTest, LanguageSelectionIsLoadedFromSavedPreferences) {
  App::Core::UserSettings::save_language(QStringLiteral("de"));

  LanguageManager language_manager;

  EXPECT_EQ(language_manager.current_language(), QStringLiteral("de"));
}

TEST_F(SettingsPersistenceTest, CompiledCatalogueIsEmbeddedAndTranslates) {
  LanguageManager language_manager;
  language_manager.set_language(QStringLiteral("de"));
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("de"));

  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QStringLiteral("Kriegselefant"));

  language_manager.set_language(QStringLiteral("pt_br"));
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("pt_br"));

  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QString::fromUtf8("Elefante de guerra"));

  language_manager.set_language(QStringLiteral("en"));
  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QStringLiteral("War Elephant"));
}

TEST_F(SettingsPersistenceTest, LanguageChangesArePersisted) {
  LanguageManager language_manager;

  language_manager.set_language(QStringLiteral("pt_br"));

  const auto saved_language = App::Core::UserSettings::load_language();
  ASSERT_TRUE(saved_language.has_value());
  EXPECT_EQ(*saved_language, QStringLiteral("pt_br"));
}

TEST_F(SettingsPersistenceTest, AudioVolumesAreSavedAndRestored) {
  auto& audio = AudioSystem::get_instance();

  audio.set_master_volume(0.35F);
  audio.set_sound_volume(0.45F);
  audio.set_music_volume(0.55F);
  audio.set_voice_volume(0.65F);
  audio.set_ambience_volume(0.75F);

  const auto saved = App::Core::UserSettings::load_audio_volumes();
  EXPECT_FLOAT_EQ(saved.master, 0.35F);
  EXPECT_FLOAT_EQ(saved.sound, 0.45F);
  EXPECT_FLOAT_EQ(saved.music, 0.55F);
  EXPECT_FLOAT_EQ(saved.voice, 0.65F);
  EXPECT_FLOAT_EQ(saved.ambience, 0.75F);

  audio.master_volume = AudioConstants::DEFAULT_VOLUME;
  audio.sound_volume = AudioConstants::DEFAULT_VOLUME;
  audio.music_volume = AudioConstants::DEFAULT_VOLUME;
  audio.voice_volume = AudioConstants::DEFAULT_VOLUME;
  audio.ambience_volume = AudioConstants::DEFAULT_VOLUME;

  audio.load_persisted_volumes();

  EXPECT_FLOAT_EQ(audio.get_master_volume(), 0.35F);
  EXPECT_FLOAT_EQ(audio.get_sound_volume(), 0.45F);
  EXPECT_FLOAT_EQ(audio.get_music_volume(), 0.55F);
  EXPECT_FLOAT_EQ(audio.get_voice_volume(), 0.65F);
  EXPECT_FLOAT_EQ(audio.get_ambience_volume(), 0.75F);
}

TEST_F(SettingsPersistenceTest, FreshInstallUsesConservativeAudioDefaults) {
  App::Core::UserSettings::clear();

  const auto loaded = App::Core::UserSettings::load_audio_volumes();
  const auto defaults = App::Core::UserSettings::default_audio_volumes();

  EXPECT_FLOAT_EQ(loaded.master, defaults.master);
  EXPECT_FLOAT_EQ(loaded.sound, defaults.sound);
  EXPECT_FLOAT_EQ(loaded.music, defaults.music);
  EXPECT_FLOAT_EQ(loaded.voice, defaults.voice);
  EXPECT_FLOAT_EQ(loaded.ambience, defaults.ambience);

  EXPECT_GT(defaults.master, AudioConstants::MIN_VOLUME);
  EXPECT_LT(defaults.master, AudioConstants::MAX_VOLUME);
  EXPECT_LT(defaults.music, AudioConstants::MAX_VOLUME);
}

TEST_F(SettingsPersistenceTest, FreshInstallDefaultsReachTheAudioSystem) {
  App::Core::UserSettings::clear();

  auto& audio = AudioSystem::get_instance();
  audio.master_volume = AudioConstants::MAX_VOLUME;
  audio.music_volume = AudioConstants::MAX_VOLUME;
  audio.load_persisted_volumes();

  const auto defaults = App::Core::UserSettings::default_audio_volumes();
  EXPECT_FLOAT_EQ(audio.get_master_volume(), defaults.master);
  EXPECT_FLOAT_EQ(audio.get_music_volume(), defaults.music);
}

TEST_F(SettingsPersistenceTest, FreshInstallDefaultsArePersistedOnFirstLoad) {
  App::Core::UserSettings::clear();

  const auto defaults = App::Core::UserSettings::load_audio_volumes();

  auto settings = App::Core::UserSettings::open();
  ASSERT_TRUE(settings.contains(
      QString::fromLatin1(App::Core::UserSettings::kMasterVolumeKey)));
  EXPECT_FLOAT_EQ(
      settings.value(QString::fromLatin1(App::Core::UserSettings::kMasterVolumeKey))
          .toFloat(),
      defaults.master);
  EXPECT_FLOAT_EQ(
      settings.value(QString::fromLatin1(App::Core::UserSettings::kMusicVolumeKey))
          .toFloat(),
      defaults.music);
}

TEST_F(SettingsPersistenceTest, SavedFullVolumeSurvivesTheNewDefaults) {
  App::Core::UserSettings::clear();
  App::Core::UserSettings::save_master_volume(AudioConstants::MAX_VOLUME);
  App::Core::UserSettings::save_music_volume(AudioConstants::MAX_VOLUME);
  App::Core::UserSettings::save_sound_volume(0.25F);
  App::Core::UserSettings::save_voice_volume(0.25F);
  App::Core::UserSettings::save_ambience_volume(0.25F);

  const auto loaded = App::Core::UserSettings::load_audio_volumes();

  EXPECT_FLOAT_EQ(loaded.master, AudioConstants::MAX_VOLUME);
  EXPECT_FLOAT_EQ(loaded.music, AudioConstants::MAX_VOLUME);
  EXPECT_FLOAT_EQ(loaded.sound, 0.25F);
  EXPECT_FLOAT_EQ(loaded.voice, 0.25F);
  EXPECT_FLOAT_EQ(loaded.ambience, 0.25F);
}

TEST_F(SettingsPersistenceTest, PartiallySavedVolumesAreNotTreatedAsAFirstRun) {
  App::Core::UserSettings::clear();
  App::Core::UserSettings::save_master_volume(0.42F);

  const auto loaded = App::Core::UserSettings::load_audio_volumes();

  EXPECT_FLOAT_EQ(loaded.master, 0.42F);
  EXPECT_FLOAT_EQ(loaded.music, AudioConstants::DEFAULT_VOLUME);
  EXPECT_FLOAT_EQ(loaded.sound, AudioConstants::DEFAULT_VOLUME);
  EXPECT_FLOAT_EQ(loaded.voice, AudioConstants::DEFAULT_VOLUME);
  EXPECT_FLOAT_EQ(loaded.ambience, AudioConstants::DEFAULT_VOLUME);
}

} // namespace
