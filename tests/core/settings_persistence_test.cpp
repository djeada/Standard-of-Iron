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

TEST_F(SettingsPersistenceTest, LanguageSelectionIsLoadedFromSavedPreferences) {
  App::Core::UserSettings::save_language(QStringLiteral("de"));

  LanguageManager language_manager;

  EXPECT_EQ(language_manager.current_language(), QStringLiteral("de"));
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

} // namespace
