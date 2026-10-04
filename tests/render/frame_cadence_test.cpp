#include <gtest/gtest.h>

#include "render/frame_cadence.h"

namespace {

using Render::FrameCadence;
using namespace std::chrono_literals;

TEST(FrameCadenceTest, AnActiveWindowNeverWaits) {
  FrameCadence cadence;
  const auto start = FrameCadence::Clock::now();
  EXPECT_EQ(cadence.wait_before_next_frame(start, start),
            FrameCadence::Clock::duration::zero());
}

TEST(FrameCadenceTest, ABackgroundWindowIsHeldToThirtyFramesPerSecond) {
  FrameCadence cadence;
  cadence.set_window_active(false);
  const auto start = FrameCadence::Clock::now();
  EXPECT_EQ(cadence.wait_before_next_frame(start, start + 3ms),
            FrameCadence::k_background_frame_interval - 3ms);
  EXPECT_EQ(cadence.wait_before_next_frame(start, start + 40ms),
            FrameCadence::Clock::duration::zero())
      << "a frame that already took longer than the interval is not delayed further";
}

} // namespace
