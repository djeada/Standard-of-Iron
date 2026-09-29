#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

#include "app/core/simulation_lifecycle.h"

namespace {

using App::Core::FrameBarrier;
using App::Core::SimulationLifecycle;
using namespace std::chrono_literals;

template <typename Predicate>
auto wait_for(Predicate predicate, std::chrono::milliseconds budget = 2000ms) -> bool {
  const auto deadline = std::chrono::steady_clock::now() + budget;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(2ms);
  }
  return predicate();
}

TEST(SimulationLifecycleTest, TicksRunOnTheirOwnThreadUnderTheFrameLock) {
  SimulationLifecycle lifecycle;
  std::atomic<int> ticks{0};
  std::atomic<bool> on_other_thread{true};
  std::atomic<bool> dt_in_range{true};
  std::atomic<int> lock_held_by_tick{0};
  const auto main_thread = std::this_thread::get_id();

  lifecycle.start([&](float dt) {
    if (std::this_thread::get_id() == main_thread) {
      on_other_thread = false;
    }
    if (dt <= 0.0F || dt > 0.1F + 1e-4F) {
      dt_in_range = false;
    }
    if (ticks.load() == 0) {
      std::thread probe([&] {
        std::unique_lock<std::recursive_mutex> lock(lifecycle.frame_mutex(),
                                                    std::try_to_lock);
        lock_held_by_tick = lock.owns_lock() ? -1 : 1;
      });
      probe.join();
    }
    ticks.fetch_add(1);
  });
  EXPECT_TRUE(lifecycle.running());
  ASSERT_TRUE(wait_for([&] { return ticks.load() >= 3; }));

  lifecycle.stop();
  EXPECT_FALSE(lifecycle.running());
  EXPECT_TRUE(on_other_thread.load());
  EXPECT_TRUE(dt_in_range.load()) << "dt is clamped to 0.1 s and positive";
  EXPECT_EQ(lock_held_by_tick.load(), 1)
      << "another thread cannot take the frame mutex while a tick is running";
  EXPECT_GT(lifecycle.take_tick_us(), 0U);
  EXPECT_EQ(lifecycle.take_tick_us(), 0U) << "the tick time is consumed on read";

  const int stopped_at = ticks.load();
  std::this_thread::sleep_for(60ms);
  EXPECT_EQ(ticks.load(), stopped_at) << "no tick runs after stop returns";
}

TEST(SimulationLifecycleTest, AFrozenWorldSkipsTicksUntilTheFreezeIsReleased) {
  SimulationLifecycle lifecycle;
  std::atomic<int> ticks{0};
  lifecycle.start([&](float) { ticks.fetch_add(1); });
  ASSERT_TRUE(wait_for([&] { return ticks.load() >= 2; }));

  ASSERT_EQ(lifecycle.barrier().try_freeze(500ms, 1ms),
            FrameBarrier::FreezeResult::Acquired);
  const int frozen_at = ticks.load();
  std::this_thread::sleep_for(80ms);
  EXPECT_EQ(ticks.load(), frozen_at);

  lifecycle.barrier().release_freeze();
  EXPECT_TRUE(wait_for([&] { return ticks.load() > frozen_at; }));
  lifecycle.stop();
}

TEST(SimulationLifecycleTest, StartingTwiceKeepsOneThreadAndStoppingTwiceIsHarmless) {
  SimulationLifecycle lifecycle;
  std::atomic<int> first{0};
  std::atomic<int> second{0};
  lifecycle.start([&](float) { first.fetch_add(1); });
  lifecycle.start([&](float) { second.fetch_add(1); });
  ASSERT_TRUE(wait_for([&] { return first.load() >= 2; }));
  lifecycle.stop();
  lifecycle.stop();
  EXPECT_EQ(second.load(), 0) << "a second start does not replace the running loop";
}

TEST(SimulationLifecycleTest, ContendedLockAcquisitionsAreCounted) {
  SimulationLifecycle lifecycle;
  {
    auto lock = lifecycle.lock_frame();
    EXPECT_TRUE(lock.owns_lock());
  }
  EXPECT_EQ(lifecycle.stats().uncontended.load(), 1U);
  EXPECT_EQ(lifecycle.stats().contended.load(), 0U);

  std::recursive_mutex& mutex = lifecycle.frame_mutex();
  std::atomic<bool> released{false};
  mutex.lock();
  std::thread waiter([&] {
    auto lock = lifecycle.lock_frame();
    released = true;
  });
  std::this_thread::sleep_for(30ms);
  mutex.unlock();
  waiter.join();
  EXPECT_TRUE(released.load());
  EXPECT_EQ(lifecycle.stats().contended.load(), 1U);
  EXPECT_GT(lifecycle.stats().longest_wait_us.load(), 0U);
}

} // namespace
