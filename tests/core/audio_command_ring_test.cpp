#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "game/audio/audio_commands.h"

namespace {

using Game::Audio::AudioCommand;
using Ring = Game::Audio::CommandRing<64>;

auto make(AudioCommand::Type type, int channel) -> AudioCommand {
  AudioCommand command;
  command.type = type;
  command.channel = static_cast<std::int16_t>(channel);
  return command;
}

TEST(AudioCommandRing, DeliversInSubmissionOrder) {
  Ring ring;
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(ring.push(make(AudioCommand::Type::Pause, i)), i);
  }

  std::vector<int> seen;
  ring.drain([&seen](const AudioCommand& command) { seen.push_back(command.channel); });

  ASSERT_EQ(seen.size(), 10U);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(seen[static_cast<std::size_t>(i)], i);
  }
  EXPECT_EQ(ring.pending(), 0U);
  EXPECT_EQ(ring.processed(), 10U);
}

TEST(AudioCommandRing, RejectsWhenFullInsteadOfOverwriting) {
  Ring ring;
  for (int i = 0; i < 64; ++i) {
    ASSERT_NE(ring.push(make(AudioCommand::Type::Pause, i)), Ring::REJECTED);
  }
  EXPECT_EQ(ring.push(make(AudioCommand::Type::Pause, 64)), Ring::REJECTED);
  EXPECT_EQ(ring.dropped(), 1U);

  int count = 0;
  ring.drain([&count](const AudioCommand&) { ++count; });
  EXPECT_EQ(count, 64);

  EXPECT_NE(ring.push(make(AudioCommand::Type::Pause, 0)), Ring::REJECTED);
}

TEST(AudioCommandRing, SurvivesConcurrentProducers) {
  Ring ring;
  std::atomic<int> accepted{0};
  std::atomic<bool> stop{false};
  std::atomic<int> consumed{0};

  std::thread consumer([&] {
    while (!stop.load() || ring.pending() > 0) {
      ring.drain([&consumed](const AudioCommand&) { consumed.fetch_add(1); });
      std::this_thread::yield();
    }
  });

  std::vector<std::thread> producers;
  producers.reserve(4);
  for (int p = 0; p < 4; ++p) {
    producers.emplace_back([&] {
      for (int i = 0; i < 500; ++i) {
        if (ring.push(make(AudioCommand::Type::Resume, i)) != Ring::REJECTED) {
          accepted.fetch_add(1);
        }
        std::this_thread::yield();
      }
    });
  }
  for (std::thread& producer : producers) {
    producer.join();
  }
  stop.store(true);
  consumer.join();
  ring.drain([&consumed](const AudioCommand&) { consumed.fetch_add(1); });

  EXPECT_EQ(consumed.load(), accepted.load());
  EXPECT_EQ(ring.pending(), 0U);
}

} // namespace
