#include <QString>

#include <gtest/gtest.h>

#include "app/economy/harvest_targeting.h"
#include "app/input/cursor_mode.h"

namespace {

using App::Economy::interaction_highlights_armed;

TEST(HarvestTargetingTest, SelectingAWorkerAloneDoesNotLightUpEveryResource) {
  EXPECT_FALSE(interaction_highlights_armed(CursorMode::Normal, false, QString()));
  EXPECT_FALSE(
      interaction_highlights_armed(CursorMode::Attack, false, QStringLiteral("")));
  EXPECT_FALSE(
      interaction_highlights_armed(CursorMode::Patrol, false, QStringLiteral("")));
}

TEST(HarvestTargetingTest, AskingForCollectArmsTheResourceHighlights) {
  EXPECT_TRUE(interaction_highlights_armed(CursorMode::Collect, false, QString()));
  EXPECT_TRUE(interaction_highlights_armed(CursorMode::Repair, false, QString()));
  EXPECT_TRUE(interaction_highlights_armed(CursorMode::Dismantle, false, QString()));
  EXPECT_TRUE(interaction_highlights_armed(CursorMode::Deliver, false, QString()));
}

TEST(HarvestTargetingTest, PlacingAHarvestTaskArmsTheHighlightsToo) {
  EXPECT_TRUE(interaction_highlights_armed(
      CursorMode::Build, true, QStringLiteral("collect_stone")));
  EXPECT_TRUE(interaction_highlights_armed(
      CursorMode::Build, true, QStringLiteral("cut_tree")));
  EXPECT_TRUE(
      interaction_highlights_armed(CursorMode::Build, true, QStringLiteral("collect")));
}

TEST(HarvestTargetingTest, PlacingAnOrdinaryBuildingLeavesResourcesAlone) {
  EXPECT_FALSE(interaction_highlights_armed(
      CursorMode::Build, true, QStringLiteral("barracks")));
  EXPECT_FALSE(
      interaction_highlights_armed(CursorMode::Build, true, QStringLiteral("home")));

  EXPECT_FALSE(interaction_highlights_armed(
      CursorMode::Normal, false, QStringLiteral("collect_stone")))
      << "a stale pending item must not arm the highlights once placement ends";
}

} // namespace
