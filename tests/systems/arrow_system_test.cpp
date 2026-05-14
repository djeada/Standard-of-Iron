#include <gtest/gtest.h>

#include "systems/arrow_system.h"

namespace {

TEST(ArrowSystemTest, VolleyArrowsReceiveLayeredVisualProfile) {
  Game::Systems::ArrowSystem system;

  system.spawn_arrow(QVector3D(0.0F, 0.0F, 0.0F),
                     QVector3D(8.0F, 0.0F, 0.0F),
                     QVector3D(0.9F, 0.8F, 0.7F),
                     12.0F,
                     Game::Systems::ArrowVisualStyle::Volley);

  auto const& arrows = system.arrows();
  ASSERT_EQ(arrows.size(), 1U);

  auto const& arrow = arrows.front();
  EXPECT_EQ(arrow.style, Game::Systems::ArrowVisualStyle::Volley);
  EXPECT_LT(arrow.t, 0.0F);
  EXPECT_GE(arrow.scale, 0.92F);
  EXPECT_LE(arrow.scale, 1.18F);
  EXPECT_GE(arrow.roll_deg, -32.0F);
  EXPECT_LE(arrow.roll_deg, 32.0F);
  EXPECT_GE(arrow.trail_alpha, 0.16F);
  EXPECT_LE(arrow.trail_alpha, 0.28F);
  EXPECT_GE(arrow.trail_length, 0.11F);
  EXPECT_LE(arrow.trail_length, 0.18F);
  EXPECT_GE(arrow.brightness, 0.88F);
  EXPECT_LE(arrow.brightness, 1.08F);
}

TEST(ArrowSystemTest, MarkerArrowsStayImmediateAndClean) {
  Game::Systems::ArrowSystem system;

  system.spawn_arrow(QVector3D(0.0F, 2.0F, 0.0F),
                     QVector3D(0.0F, 0.0F, 0.0F),
                     QVector3D(1.0F, 0.2F, 0.2F),
                     6.0F,
                     Game::Systems::ArrowVisualStyle::Marker);

  auto const& arrows = system.arrows();
  ASSERT_EQ(arrows.size(), 1U);

  auto const& arrow = arrows.front();
  EXPECT_EQ(arrow.style, Game::Systems::ArrowVisualStyle::Marker);
  EXPECT_FLOAT_EQ(arrow.t, 0.0F);
  EXPECT_FLOAT_EQ(arrow.roll_deg, 0.0F);
  EXPECT_FLOAT_EQ(arrow.spin_rate_deg, 0.0F);
  EXPECT_FLOAT_EQ(arrow.trail_alpha, 0.0F);
  EXPECT_FLOAT_EQ(arrow.trail_length, 0.0F);
}

TEST(ArrowSystemTest, UpdateRemovesArrowsAfterDelayedFlightFinishes) {
  Game::Systems::ArrowSystem system;

  system.spawn_arrow(QVector3D(0.0F, 0.0F, 0.0F),
                     QVector3D(6.0F, 0.0F, 0.0F),
                     QVector3D(0.8F, 0.9F, 1.0F),
                     12.0F,
                     Game::Systems::ArrowVisualStyle::Volley);

  ASSERT_EQ(system.arrows().size(), 1U);
  for (int i = 0; i < 8; ++i) {
    system.update(nullptr, 0.1F);
  }

  EXPECT_TRUE(system.arrows().empty());
}

} // namespace
