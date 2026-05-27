#include <QCoreApplication>

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <thread>

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

} // namespace
