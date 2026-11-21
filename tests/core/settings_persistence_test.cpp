#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#define private public
#include "game/audio/audio_system.h"
#undef private

#include "app/core/game_engine.h"
#include "app/core/language_manager.h"
#include "app/core/user_settings.h"
#include "app/models/graphics_settings_proxy.h"
#include "render/graphics_settings.h"
#include "render/software_backend.h"
#include "scene/camera.h"

namespace {

class SettingsPersistenceTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    App::Core::UserSettings::clear();
    Render::GraphicsSettings::instance().set_quality(
        Render::k_default_graphics_quality);
    AudioSystem::get_instance().load_persisted_volumes();
  }

  void TearDown() override {
    App::Core::UserSettings::clear();
    Render::GraphicsSettings::instance().set_quality(
        Render::k_default_graphics_quality);
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

  Render::GraphicsSettings::instance().set_quality(Render::k_default_graphics_quality);
  App::Core::UserSettings::apply_saved_graphics_quality();
  EXPECT_EQ(Render::GraphicsSettings::instance().quality(),
            Render::GraphicsQuality::Medium);
}

TEST_F(SettingsPersistenceTest, FreshProfileDefaultsToHighGraphics) {
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Low);

  App::Core::UserSettings::apply_saved_graphics_quality();

  EXPECT_EQ(Render::GraphicsSettings::instance().quality(),
            Render::GraphicsQuality::High);
  EXPECT_EQ(Render::k_default_graphics_quality, Render::GraphicsQuality::High);
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

  language_manager.set_language(QStringLiteral("tr"));
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("tr"));

  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QString::fromUtf8("Savaş Fili"));

  language_manager.set_language(QStringLiteral("pl"));
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("pl"));

  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QString::fromUtf8("Słoń bojowy"));

  language_manager.set_language(QStringLiteral("ru"));
  ASSERT_EQ(language_manager.current_language(), QStringLiteral("ru"));

  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QString::fromUtf8("Боевой слон"));

  language_manager.set_language(QStringLiteral("en"));
  EXPECT_EQ(QCoreApplication::translate("Units", "War Elephant"),
            QStringLiteral("War Elephant"));
}

TEST_F(SettingsPersistenceTest, TurkishIsOfferedAndLaidOutLeftToRight) {
  LanguageManager language_manager;

  EXPECT_TRUE(language_manager.available_languages().contains(QStringLiteral("tr")))
      << "the Turkish catalogue ships but the language cannot be chosen";
  EXPECT_EQ(LanguageManager::language_display_name(QStringLiteral("tr")),
            QString::fromUtf8("Türkçe (Turkish)"));

  language_manager.set_language(QStringLiteral("tr"));
  EXPECT_EQ(QGuiApplication::layoutDirection(), Qt::LeftToRight)
      << "Turkish is not a right-to-left script";

  language_manager.set_language(QStringLiteral("en"));
}

TEST_F(SettingsPersistenceTest, PolishAndRussianAreOfferedAndLaidOutLeftToRight) {
  LanguageManager language_manager;

  for (const auto* code : {"pl", "ru"}) {
    EXPECT_TRUE(
        language_manager.available_languages().contains(QString::fromLatin1(code)))
        << code << " ships a catalogue but cannot be chosen";
  }
  EXPECT_EQ(LanguageManager::language_display_name(QStringLiteral("pl")),
            QString::fromUtf8("Polski (Polish)"));
  EXPECT_EQ(LanguageManager::language_display_name(QStringLiteral("ru")),
            QString::fromUtf8("Русский (Russian)"));

  language_manager.set_language(QStringLiteral("pl"));
  EXPECT_EQ(QGuiApplication::layoutDirection(), Qt::LeftToRight);
  language_manager.set_language(QStringLiteral("ru"));
  EXPECT_EQ(QGuiApplication::layoutDirection(), Qt::LeftToRight);

  language_manager.set_language(QStringLiteral("en"));
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

  const auto defaults = Game::Audio::Settings::first_run_volumes();
  EXPECT_FLOAT_EQ(loaded.master, 0.42F);
  EXPECT_FLOAT_EQ(loaded.music, defaults.music);
  EXPECT_FLOAT_EQ(loaded.sound, defaults.sound);
  EXPECT_FLOAT_EQ(loaded.voice, defaults.voice);
  EXPECT_FLOAT_EQ(loaded.ambience, defaults.ambience);
}

TEST_F(SettingsPersistenceTest,
       SoftwareBackendReportsReadyAndProducesBattlefieldFrame) {
  Render::GL::SoftwareBackend backend;
  EXPECT_TRUE(backend.initialize());
  EXPECT_EQ(backend.resources(), nullptr);
  EXPECT_TRUE(backend.is_ready());
  EXPECT_EQ(backend.shader_quality(), Render::ShaderQuality::None);

  backend.set_viewport(64, 64);
  backend.set_clear_color(0.05F, 0.05F, 0.05F, 1.0F);
  backend.begin_frame();

  Render::GL::Camera camera;
  camera.set_perspective(45.0F, 1.0F, 0.1F, 100.0F);
  camera.look_at(QVector3D(0.0F, 6.0F, 10.0F),
                 QVector3D(0.0F, 0.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));

  Render::GL::DrawQueue queue;
  Render::GL::MeshCmd cmd;
  cmd.model.scale(2.0F, 2.0F, 2.0F);
  cmd.color = QVector3D(0.85F, 0.25F, 0.20F);
  cmd.alpha = 1.0F;
  queue.submit(cmd);
  backend.execute(queue, camera);

  const QImage* frame = backend.presented_frame();
  ASSERT_NE(frame, nullptr);
  ASSERT_FALSE(frame->isNull());
  EXPECT_EQ(frame->width(), 64);
  EXPECT_EQ(frame->height(), 64);

  const QRgb corner = frame->pixel(0, 0);
  bool found_battlefield_pixel = false;
  for (int y = 0; y < frame->height() && !found_battlefield_pixel; ++y) {
    for (int x = 0; x < frame->width(); ++x) {
      if (frame->pixel(x, y) != corner) {
        found_battlefield_pixel = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_battlefield_pixel);
}

TEST_F(SettingsPersistenceTest,
       RendererFailureStopsLoadingAndReportsOnceOnTheGuiThread) {
  GameEngine engine;
  engine.set_release_self_test_mode(true);
  engine.launch_match({.map_path = QStringLiteral("should-never-load")});
  ASSERT_TRUE(engine.is_loading());

  QEventLoop loop;
  QStringList errors;
  QObject::connect(
      &engine,
      &GameEngine::renderer_failed,
      &loop,
      [&](const QString& error) {
        EXPECT_EQ(QThread::currentThread(), engine.thread());
        EXPECT_FALSE(engine.is_loading());
        errors.push_back(error);
      },
      Qt::DirectConnection);

  const QString first_error = QStringLiteral("OpenGL 3.3 Core required");
  std::thread render_thread([&] {
    engine.report_renderer_failure(first_error);
    engine.report_renderer_failure(QStringLiteral("Framebuffer unavailable"));
  });
  render_thread.join();

  EXPECT_TRUE(engine.renderer_has_failed());
  EXPECT_FALSE(engine.renderer_initialized());
  EXPECT_TRUE(errors.isEmpty());
  engine.clear_error();
  engine.ensure_initialized();

  QTimer::singleShot(std::chrono::milliseconds(100), &loop, &QEventLoop::quit);
  loop.exec();
  EXPECT_EQ(errors, QStringList{first_error});
  EXPECT_TRUE(engine.last_error().isEmpty());
  EXPECT_FALSE(engine.is_loading());
}

} // namespace
