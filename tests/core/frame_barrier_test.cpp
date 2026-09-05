#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "app/core/frame_barrier.h"

namespace {

using App::Core::FrameBarrier;
using namespace std::chrono_literals;

constexpr auto k_poll = std::chrono::microseconds(200);

} // namespace

TEST(FrameBarrierTest, FreezeIsAcquiredWhenNobodyIsInAFrame) {
  FrameBarrier barrier;
  EXPECT_EQ(barrier.try_freeze(50ms, k_poll), FrameBarrier::FreezeResult::Acquired);
  EXPECT_TRUE(barrier.frozen());
  EXPECT_EQ(barrier.refusals(), 0);
  barrier.release_freeze();
  EXPECT_FALSE(barrier.frozen());
}

TEST(FrameBarrierTest, FrozenBarrierRefusesNewRenderAndSimulationFrames) {
  FrameBarrier barrier;
  ASSERT_EQ(barrier.try_freeze(50ms, k_poll), FrameBarrier::FreezeResult::Acquired);

  EXPECT_FALSE(barrier.try_begin_render());
  EXPECT_FALSE(barrier.render_active());
  EXPECT_FALSE(barrier.try_begin_simulation());
  EXPECT_FALSE(barrier.simulation_active());

  barrier.release_freeze();
  EXPECT_TRUE(barrier.try_begin_render());
  barrier.end_render();
  EXPECT_TRUE(barrier.try_begin_simulation());
  barrier.end_simulation();
}

TEST(FrameBarrierTest, StalledRenderFrameRefusesTheFreezeAndLeavesTheBarrierOpen) {
  FrameBarrier barrier;
  ASSERT_TRUE(barrier.try_begin_render());

  EXPECT_EQ(barrier.try_freeze(20ms, k_poll), FrameBarrier::FreezeResult::RenderBusy);
  EXPECT_EQ(barrier.refusals(), 1);
  EXPECT_FALSE(barrier.frozen());

  barrier.end_render();
  EXPECT_EQ(barrier.try_freeze(50ms, k_poll), FrameBarrier::FreezeResult::Acquired);
  barrier.release_freeze();
}

TEST(FrameBarrierTest, StalledSimulationTickRefusesTheFreeze) {
  FrameBarrier barrier;
  ASSERT_TRUE(barrier.try_begin_simulation());

  EXPECT_EQ(barrier.try_freeze(20ms, k_poll),
            FrameBarrier::FreezeResult::SimulationBusy);
  EXPECT_EQ(barrier.refusals(), 1);
  EXPECT_FALSE(barrier.frozen());

  barrier.end_simulation();
}

TEST(FrameBarrierTest, RefusedFreezeDoesNotBlockLaterFramesOrLaterFreezes) {
  FrameBarrier barrier;
  ASSERT_TRUE(barrier.try_begin_render());
  ASSERT_EQ(barrier.try_freeze(20ms, k_poll), FrameBarrier::FreezeResult::RenderBusy);

  ASSERT_TRUE(barrier.try_begin_simulation());
  barrier.end_simulation();
  barrier.end_render();

  EXPECT_EQ(barrier.try_freeze(50ms, k_poll), FrameBarrier::FreezeResult::Acquired);
  EXPECT_EQ(barrier.refusals(), 1);
  barrier.release_freeze();
}

TEST(FrameBarrierTest, FreezeWaitsForAFrameThatFinishesWithinTheBudget) {
  FrameBarrier barrier;
  ASSERT_TRUE(barrier.try_begin_render());

  std::thread finisher([&barrier]() {
    std::this_thread::sleep_for(10ms);
    barrier.end_render();
  });

  EXPECT_EQ(barrier.try_freeze(2000ms, k_poll), FrameBarrier::FreezeResult::Acquired);
  finisher.join();
  EXPECT_EQ(barrier.refusals(), 0);
  barrier.release_freeze();
}

TEST(FrameBarrierTest, NestedFreezesAreAcquiredWithoutWaiting) {
  FrameBarrier barrier;
  ASSERT_EQ(barrier.try_freeze(50ms, k_poll), FrameBarrier::FreezeResult::Acquired);
  EXPECT_EQ(barrier.try_freeze(50ms, k_poll), FrameBarrier::FreezeResult::Acquired);
  barrier.release_freeze();
  EXPECT_TRUE(barrier.frozen());
  barrier.release_freeze();
  EXPECT_FALSE(barrier.frozen());
}

TEST(FrameBarrierTest, ARunningLoopNeverOverlapsAFreeze) {
  FrameBarrier barrier;
  std::atomic<bool> stop{false};
  std::atomic<int> overlaps{0};
  std::atomic<bool> inside_freeze{false};

  std::thread renderer([&]() {
    while (!stop.load(std::memory_order_acquire)) {
      if (!barrier.try_begin_render()) {
        std::this_thread::yield();
        continue;
      }
      if (inside_freeze.load(std::memory_order_acquire)) {
        overlaps.fetch_add(1, std::memory_order_acq_rel);
      }
      barrier.end_render();
    }
  });

  int acquired = 0;
  for (int attempt = 0; attempt < 40; ++attempt) {
    if (barrier.try_freeze(500ms, k_poll) != FrameBarrier::FreezeResult::Acquired) {
      continue;
    }
    ++acquired;
    inside_freeze.store(true, std::memory_order_release);
    std::this_thread::sleep_for(100us);
    inside_freeze.store(false, std::memory_order_release);
    barrier.release_freeze();
  }

  stop.store(true, std::memory_order_release);
  renderer.join();

  EXPECT_GT(acquired, 0);
  EXPECT_EQ(overlaps.load(), 0);
}
