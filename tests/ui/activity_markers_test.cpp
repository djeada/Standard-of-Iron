#include <algorithm>
#include <gtest/gtest.h>

#include "app/models/activity_markers.h"

namespace {

using App::Models::activity_deserves_marker;
using App::Models::ActivityMarker;
using App::Models::ActivityMarkerOptions;
using App::Models::ActivityMarkerSource;
using App::Models::group_activity_markers;
using Game::Systems::ActivityKind;
using Game::Systems::ActivityState;

auto source(std::uint64_t id,
            ActivityKind kind,
            float x,
            float z,
            ActivityState state = ActivityState::Active) -> ActivityMarkerSource {
  ActivityMarkerSource entry;
  entry.entity_id = id;
  entry.activity = {kind, state, 0};
  entry.x = x;
  entry.z = z;
  return entry;
}

auto find(const std::vector<ActivityMarker>& markers,
          const char* activity) -> const ActivityMarker* {
  auto match = std::find_if(
      markers.begin(), markers.end(), [activity](const ActivityMarker& marker) {
        return marker.activity == QLatin1String(activity);
      });
  return match != markers.end() ? &*match : nullptr;
}

TEST(ActivityMarkerPolicyTest, MarchingUnitsAreLeftAlone) {
  EXPECT_FALSE(
      activity_deserves_marker({ActivityKind::Move, ActivityState::Active, 0}, false));
  EXPECT_FALSE(
      activity_deserves_marker({ActivityKind::Move, ActivityState::Active, 0}, true));
  EXPECT_FALSE(
      activity_deserves_marker({ActivityKind::Idle, ActivityState::Active, 0}, true));
}

TEST(ActivityMarkerPolicyTest, StandingWorkIsShownWhetherOrNotItIsSelected) {
  for (const auto kind : {ActivityKind::Construct,
                          ActivityKind::Repair,
                          ActivityKind::ChopWood,
                          ActivityKind::MineStone,
                          ActivityKind::MineIron,
                          ActivityKind::Deliver,
                          ActivityKind::Blocked}) {
    EXPECT_TRUE(activity_deserves_marker({kind, ActivityState::Active, 0}, false))
        << Game::Systems::activity_kind_id(kind);
  }
}

TEST(ActivityMarkerPolicyTest, StancesOnlyEarnAMarkerWhileSelected) {
  const Game::Systems::UnitActivity guarding{
      ActivityKind::Guard, ActivityState::Active, 0};
  EXPECT_FALSE(activity_deserves_marker(guarding, false));
  EXPECT_TRUE(activity_deserves_marker(guarding, true));
}

TEST(ActivityMarkerPolicyTest, AStalledOrderIsAlwaysWorthShowing) {
  EXPECT_TRUE(activity_deserves_marker(
      {ActivityKind::Attack, ActivityState::Unavailable, 0}, false));
  EXPECT_TRUE(activity_deserves_marker(
      {ActivityKind::Patrol, ActivityState::Interrupted, 0}, false));
}

TEST(ActivityMarkerGroupingTest, NeighboursDoingTheSameJobShareOneMarker) {
  const std::vector<ActivityMarkerSource> sources{
      source(1, ActivityKind::ChopWood, 0.0F, 0.0F),
      source(2, ActivityKind::ChopWood, 1.5F, 0.5F),
      source(3, ActivityKind::ChopWood, 2.0F, -1.0F),
  };

  const auto markers = group_activity_markers(sources);
  ASSERT_EQ(markers.size(), 1U);
  EXPECT_EQ(markers.front().count, 3);
  EXPECT_EQ(markers.front().activity, QStringLiteral("chop_wood"));
  EXPECT_EQ(markers.front().lead_entity_id, 1U);
  EXPECT_NEAR(markers.front().x, 1.1667F, 0.01F);
}

TEST(ActivityMarkerGroupingTest, DistantCrewsKeepTheirOwnMarkers) {
  ActivityMarkerOptions options;
  options.cluster_radius = 5.0F;

  const std::vector<ActivityMarkerSource> sources{
      source(1, ActivityKind::MineStone, 0.0F, 0.0F),
      source(2, ActivityKind::MineStone, 40.0F, 0.0F),
  };

  const auto markers = group_activity_markers(sources, options);
  EXPECT_EQ(markers.size(), 2U);
}

TEST(ActivityMarkerGroupingTest, DifferentWorkNeverMergesEvenSideBySide) {
  const std::vector<ActivityMarkerSource> sources{
      source(1, ActivityKind::ChopWood, 0.0F, 0.0F),
      source(2, ActivityKind::MineIron, 0.2F, 0.2F),
      source(3, ActivityKind::ChopWood, 0.4F, 0.1F, ActivityState::Interrupted),
  };

  const auto markers = group_activity_markers(sources);
  ASSERT_EQ(markers.size(), 3U);
  const auto* chopping = find(markers, "chop_wood");
  ASSERT_NE(chopping, nullptr);
  EXPECT_EQ(chopping->count, 1)
      << "an interrupted crew must not be folded into the working one";
}

TEST(ActivityMarkerGroupingTest, ACrowdedFieldCollapsesToOneMarkerPerActivity) {
  ActivityMarkerOptions options;
  options.cluster_radius = 2.0F;
  options.max_markers = 4;

  std::vector<ActivityMarkerSource> sources;
  for (int i = 0; i < 12; ++i) {
    sources.push_back(source(static_cast<std::uint64_t>(i + 1),
                             ActivityKind::ChopWood,
                             static_cast<float>(i) * 10.0F,
                             0.0F));
  }
  sources.push_back(source(99, ActivityKind::Repair, 500.0F, 0.0F));

  const auto markers = group_activity_markers(sources, options);
  ASSERT_EQ(markers.size(), 2U);
  const auto* chopping = find(markers, "chop_wood");
  ASSERT_NE(chopping, nullptr);
  EXPECT_EQ(chopping->count, 12);
  EXPECT_NE(find(markers, "repair"), nullptr);
}

TEST(ActivityMarkerGroupingTest, TooManyDistinctActivitiesKeepTheBusiestOnes) {
  ActivityMarkerOptions options;
  options.cluster_radius = 1.0F;
  options.max_markers = 2;

  const std::vector<ActivityMarkerSource> sources{
      source(1, ActivityKind::ChopWood, 0.0F, 0.0F),
      source(2, ActivityKind::ChopWood, 0.1F, 0.0F),
      source(3, ActivityKind::ChopWood, 0.2F, 0.0F),
      source(4, ActivityKind::MineIron, 50.0F, 0.0F),
      source(5, ActivityKind::MineIron, 50.1F, 0.0F),
      source(6, ActivityKind::Repair, 100.0F, 0.0F),
  };

  const auto markers = group_activity_markers(sources, options);
  ASSERT_EQ(markers.size(), 2U);
  EXPECT_EQ(markers[0].count, 3);
  EXPECT_EQ(markers[1].count, 2);
}

TEST(ActivityMarkerGroupingTest, NothingToReportProducesNothing) {
  EXPECT_TRUE(group_activity_markers({}).empty());
}

} // namespace
