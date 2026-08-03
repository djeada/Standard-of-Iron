#include <gtest/gtest.h>

#include "render/gl/backend/instance_draw_guard.h"

namespace {

using Render::GL::BackendPipelines::InstanceDrawGuard;

TEST(InstanceDrawGuard, PassesThroughWhenBufferHoldsEnoughInstances) {
  InstanceDrawGuard guard("test");

  EXPECT_EQ(guard.clamp(0, 0), 0U);
  EXPECT_EQ(guard.clamp(128, 128), 128U);
  EXPECT_EQ(guard.clamp(64, 512), 64U);
  EXPECT_EQ(guard.overflow_count(), 0U);
}

TEST(InstanceDrawGuard, ClampsDrawToResidentInstanceCount) {
  InstanceDrawGuard guard("test");

  EXPECT_EQ(guard.clamp(9000, 8192), 8192U);
  EXPECT_EQ(guard.clamp(1, 0), 0U);
  EXPECT_EQ(guard.overflow_count(), 2U);
}

TEST(InstanceDrawGuard, KeepsCountingOverflowsAfterFirstReport) {
  InstanceDrawGuard guard("test");

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(guard.clamp(10, 4), 4U);
  }
  EXPECT_EQ(guard.overflow_count(), 5U);
}

} // namespace
