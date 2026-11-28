#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <gtest/gtest.h>
#include <numbers>
#include <thread>
#include <vector>

#define private public
#include "game/audio/audio_system.h"
#undef private

namespace {

class AudioSystemMusicDispatchTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& audio = AudioSystem::get_instance();
    audio.shutdown();
    if (!audio.initialize()) {
      GTEST_SKIP() << "Audio backend is unavailable in this environment";
    }

    std::lock_guard<std::mutex> const lock(audio.resource_mutex);
    AudioResourceConfig config;
    config.category = AudioCategory::MUSIC;
    config.volume = AudioConstants::DEFAULT_VOLUME;
    audio.resource_configs[kMusicId] = config;
    audio.active_resources.insert(kMusicId);
  }

  void TearDown() override { AudioSystem::get_instance().shutdown(); }

  static constexpr const char* kMusicId = "test.music.lock_release";
};

TEST_F(AudioSystemMusicDispatchTest,
       PlayMusicReleasesResourceMutexBeforeWaitingOnGuiThread) {
  auto& audio = AudioSystem::get_instance();
  const AudioEvent event(AudioEventType::PLAY_MUSIC,
                         kMusicId,
                         AudioConstants::DEFAULT_VOLUME,
                         true,
                         AudioConstants::DEFAULT_PRIORITY,
                         AudioCategory::MUSIC,
                         false);

  auto play_future =
      std::async(std::launch::async, [&audio, event]() { audio.process_event(event); });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto has_resource_future = std::async(
      std::launch::async, [&audio]() { return audio.has_resource(kMusicId); });

  const auto resource_wait =
      has_resource_future.wait_for(std::chrono::milliseconds(50));

  const auto drive_gui_until = [&](std::future<void>& future) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
           std::chrono::steady_clock::now() < deadline) {
      QCoreApplication::processEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  };

  drive_gui_until(play_future);
  EXPECT_EQ(resource_wait, std::future_status::ready);

  if (resource_wait != std::future_status::ready) {
    EXPECT_EQ(has_resource_future.wait_for(std::chrono::milliseconds(200)),
              std::future_status::ready);
  }
  EXPECT_TRUE(has_resource_future.get());
  EXPECT_EQ(play_future.wait_for(std::chrono::milliseconds(0)),
            std::future_status::ready);
}

TEST_F(AudioSystemMusicDispatchTest, ShutdownDuringAMusicChangeReturns) {
  auto& audio = AudioSystem::get_instance();
  std::atomic<bool> finished{false};
  std::thread watchdog([&finished]() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!finished.load()) {
      std::fprintf(stderr,
                   "AudioSystem::shutdown deadlocked on an in-flight music change\n");
      std::_Exit(1);
    }
  });

  for (int change = 0; change < 8; ++change) {
    audio.play_music(kMusicId);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  audio.shutdown();
  finished.store(true);
  watchdog.join();

  EXPECT_FALSE(audio.is_running.load());
}

TEST_F(AudioSystemMusicDispatchTest, LiveSlidersUpdatePlayingAmbienceAndMusic) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString wav_path = dir.filePath("tone.wav");
  {
    constexpr int rate = 48000;
    constexpr int frames = rate / 4;
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    const std::uint32_t payload = frames * 2 * 2;
    stream.writeRawData("RIFF", 4);
    stream << std::uint32_t(36 + payload);
    stream.writeRawData("WAVEfmt ", 8);
    stream << std::uint32_t(16) << std::uint16_t(1) << std::uint16_t(2)
           << std::uint32_t(rate) << std::uint32_t(rate * 4) << std::uint16_t(4)
           << std::uint16_t(16);
    stream.writeRawData("data", 4);
    stream << payload;
    for (int f = 0; f < frames; ++f) {
      const double phase = 2.0 * std::numbers::pi * 330.0 * f / rate;
      const auto sample = static_cast<std::int16_t>(0.4 * std::sin(phase) * 32000.0);
      stream << sample << sample;
    }
    QFile file(wav_path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(data), data.size());
  }

  auto& audio = AudioSystem::get_instance();
  const float saved_master = audio.get_master_volume();
  const float saved_ambience = audio.get_ambience_volume();
  const float saved_music = audio.get_music_volume();

  audio.set_master_volume(1.0F);
  audio.set_ambience_volume(1.0F);
  audio.set_music_volume(1.0F);

  AudioResourceConfig bed_cfg;
  bed_cfg.category = AudioCategory::AMBIENCE;
  bed_cfg.volume = 1.0F;
  ASSERT_TRUE(audio.load_sound("test.bed.live", wav_path.toStdString(), bed_cfg));

  AudioResourceConfig music_cfg;
  music_cfg.category = AudioCategory::MUSIC;
  music_cfg.volume = 0.5F;
  ASSERT_TRUE(audio.load_music("test.music.live", wav_path.toStdString(), music_cfg));

  audio.play_sound("test.bed.live", 1.0F, true, 1, AudioCategory::AMBIENCE);
  std::this_thread::sleep_for(std::chrono::milliseconds(40));

  auto measure_peak = [&audio]() {
    std::vector<float> buf(48000 * 2, 0.0F);
    audio.render_offline(buf.data(), 48000);
    float pk = 0.0F;
    for (std::size_t i = buf.size() / 2; i < buf.size(); ++i) {
      pk = std::max(pk, std::abs(buf[i]));
    }
    return pk;
  };

  const float loud_bed_peak = measure_peak();
  ASSERT_GT(loud_bed_peak, 0.05F);

  audio.set_ambience_volume(0.1F);
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  const float quiet_bed_peak = measure_peak();
  EXPECT_LT(quiet_bed_peak, loud_bed_peak * 0.25F);

  audio.stop_sound("test.bed.live");
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  (void)measure_peak();

  audio.play_music("test.music.live", 1.0F, Game::Audio::MusicTransition::Immediate);
  for (int i = 0; i < 20; ++i) {
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const float music_half_peak = measure_peak();
  ASSERT_GT(music_half_peak, 0.02F);

  audio.set_music_volume(0.2F);
  for (int i = 0; i < 20; ++i) {
    QCoreApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const float music_lower_peak = measure_peak();
  EXPECT_LT(music_lower_peak, music_half_peak * 0.35F);

  audio.stop_music();
  audio.set_master_volume(saved_master);
  audio.set_ambience_volume(saved_ambience);
  audio.set_music_volume(saved_music);
}

} // namespace
