#include <chrono>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <string_view>

#include "game/core/nav_profile.h"

namespace {

using Engine::Core::NavCounter;
using Engine::Core::NavProfile;
using Engine::Core::NavTickScope;

class NavProfileTest : public ::testing::Test {
protected:
  void SetUp() override {
    Engine::Core::nav_profile().clear();
    Engine::Core::nav_profile().set_enabled(true);
  }

  void TearDown() override {
    Engine::Core::nav_profile().set_enabled(false);
    Engine::Core::nav_profile().clear();
  }

  static auto profile() -> NavProfile& { return Engine::Core::nav_profile(); }
};

TEST_F(NavProfileTest, DisabledProfileCountsNothing) {
  profile().set_enabled(false);
  Engine::Core::count_nav(NavCounter::PositionTests, 100);
  { const NavTickScope tick; }
  EXPECT_EQ(profile().total(NavCounter::PositionTests), 0U);
  EXPECT_EQ(profile().ticks(), 0U);
}

TEST_F(NavProfileTest, CountersAccumulateIntoTheClosedTick) {
  {
    const NavTickScope tick;
    Engine::Core::count_nav(NavCounter::PositionTests, 7);
    Engine::Core::count_nav(NavCounter::CellsExpanded, 3);
    EXPECT_EQ(profile().ticks(), 0U);
  }
  EXPECT_EQ(profile().ticks(), 1U);
  EXPECT_EQ(profile().last_tick(NavCounter::PositionTests), 7U);
  EXPECT_EQ(profile().total(NavCounter::PositionTests), 7U);
  EXPECT_EQ(profile().total(NavCounter::CellsExpanded), 3U);
}

TEST_F(NavProfileTest, EachTickStartsFromZero) {
  {
    const NavTickScope tick;
    Engine::Core::count_nav(NavCounter::PositionTests, 10);
  }
  {
    const NavTickScope tick;
    Engine::Core::count_nav(NavCounter::PositionTests, 4);
  }
  EXPECT_EQ(profile().ticks(), 2U);
  EXPECT_EQ(profile().last_tick(NavCounter::PositionTests), 4U);
  EXPECT_EQ(profile().total(NavCounter::PositionTests), 14U);
  EXPECT_DOUBLE_EQ(profile().per_tick_average(NavCounter::PositionTests), 7.0);
}

TEST_F(NavProfileTest, PerTickAverageIsZeroBeforeAnyTick) {
  Engine::Core::count_nav(NavCounter::PositionTests, 5);
  EXPECT_DOUBLE_EQ(profile().per_tick_average(NavCounter::PositionTests), 0.0);
}

TEST_F(NavProfileTest, ScopeAttributesTimeToElapsedAndItsOwnCounter) {
  {
    const NavTickScope tick;
    const Engine::Core::NavScope scope(NavCounter::IndividualRoutes);
  }
  EXPECT_EQ(profile().last_tick(NavCounter::IndividualRoutes), 1U);
  EXPECT_EQ(profile().ticks(), 1U);
}

TEST_F(NavProfileTest, NestedScopesDoNotDoubleCountElapsedTime) {
  std::uint64_t nested_elapsed = 0;
  std::uint64_t flat_elapsed = 0;

  auto burn = [] {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(4);
    while (std::chrono::steady_clock::now() < until) {
    }
  };

  {
    const NavTickScope tick;
    {
      const Engine::Core::NavScope outer(NavCounter::GroupRoutes);
      const Engine::Core::NavScope inner(NavCounter::IndividualRoutes);
      burn();
    }
  }
  nested_elapsed = profile().last_tick(NavCounter::ElapsedUs);

  {
    const NavTickScope tick;
    {
      const Engine::Core::NavScope only(NavCounter::GroupRoutes);
      burn();
    }
  }
  flat_elapsed = profile().last_tick(NavCounter::ElapsedUs);

  EXPECT_GT(nested_elapsed, 0U);
  EXPECT_LT(nested_elapsed, flat_elapsed * 2U)
      << "nested scopes counted their inner time twice";
}

TEST_F(NavProfileTest, TickTimeDistributionTracksElapsedMicroseconds) {
  for (std::uint64_t microseconds : {500U, 1500U, 2500U}) {
    const NavTickScope tick;
    Engine::Core::count_nav(NavCounter::ElapsedUs, microseconds);
  }
  const auto spread = profile().tick_time_ms();
  EXPECT_EQ(spread.count, 3U);
  EXPECT_DOUBLE_EQ(spread.p50, 1.5);
  EXPECT_DOUBLE_EQ(spread.maximum, 2.5);
  EXPECT_NEAR(spread.average, 1.5, 1e-9);
}

TEST_F(NavProfileTest, QueueAgeKeepsTheWorstObservation) {
  profile().observe_queue_age_ticks(3);
  profile().observe_queue_age_ticks(11);
  profile().observe_queue_age_ticks(5);
  EXPECT_EQ(profile().max_queue_age_ticks(), 11U);
}

TEST_F(NavProfileTest, EveryCounterHasADistinctName) {
  std::set<std::string_view> names;
  for (std::size_t i = 0; i < NavProfile::k_count; ++i) {
    const auto counter = static_cast<NavCounter>(i);
    const std::string_view name = Engine::Core::nav_counter_name(counter);
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, "unknown");
    EXPECT_TRUE(names.insert(name).second) << "duplicate counter name " << name;
  }
}

TEST_F(NavProfileTest, ReportNamesTheCountersItRecorded) {
  {
    const NavTickScope tick;
    Engine::Core::count_nav(NavCounter::StandabilityTests, 12);
  }
  const std::string report = profile().format_report();
  EXPECT_NE(report.find("standability_tests"), std::string::npos);
  EXPECT_NE(report.find("navigation over 1 ticks"), std::string::npos);
  EXPECT_EQ(report.find("group_routes"), std::string::npos);
}

TEST_F(NavProfileTest, ClearResetsTotalsAndTicks) {
  {
    const NavTickScope tick;
    Engine::Core::count_nav(NavCounter::SegmentTests, 9);
  }
  profile().clear();
  EXPECT_EQ(profile().ticks(), 0U);
  EXPECT_EQ(profile().total(NavCounter::SegmentTests), 0U);
  EXPECT_EQ(profile().max_queue_age_ticks(), 0U);
}

} // namespace
