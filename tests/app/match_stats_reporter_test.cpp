#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>

#include "app/platform/match_stats_reporter.h"

namespace {

using App::Platform::MatchStatsReporter;
using App::Platform::MatchSummary;
using App::Platform::StatsSink;

class FakeStats final : public StatsSink {
public:
  auto get_int(const std::string& name, int& value) -> bool override {
    if (!available) {
      return false;
    }
    value = values[name];
    return true;
  }
  void set_int(const std::string& name, int value) override { values[name] = value; }
  void store() override { ++stores; }

  std::map<std::string, int> values;
  int stores = 0;
  bool available = true;
};

TEST(MatchStatsReporter, TotalsAccumulateAcrossMatches) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.report({.victory = true,
                   .campaign_mission = true,
                   .enemies_defeated = 30,
                   .units_recruited = 12});
  reporter.match_ended();
  reporter.report({.victory = true, .enemies_defeated = 20, .units_recruited = 8});
  EXPECT_EQ(sink->values["soi_battles_won"], 2);
  EXPECT_EQ(sink->values["soi_missions_completed"], 1);
  EXPECT_EQ(sink->values["soi_enemies_defeated"], 50);
  EXPECT_EQ(sink->values["soi_units_recruited"], 20);
}

TEST(MatchStatsReporter, MaximumKeepsTheBestMatch) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.report({.enemies_defeated = 40});
  reporter.match_ended();
  reporter.report({.enemies_defeated = 15});
  EXPECT_EQ(sink->values["soi_best_match_kills"], 40);
}

TEST(MatchStatsReporter, DefeatCountsKillsButNotAWin) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.report({.victory = false, .enemies_defeated = 5});
  EXPECT_EQ(sink->values.count("soi_battles_won"), 0U);
  EXPECT_EQ(sink->values["soi_enemies_defeated"], 5);
}

TEST(MatchStatsReporter, RepeatedOutcomeIsFoldedOnce) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.report({.victory = true, .enemies_defeated = 10});
  reporter.report({.victory = true, .enemies_defeated = 10});
  EXPECT_EQ(sink->values["soi_battles_won"], 1);
  EXPECT_EQ(sink->values["soi_enemies_defeated"], 10);
}

TEST(MatchStatsReporter, StoresOncePerMatch) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.report({.victory = true, .enemies_defeated = 500, .units_recruited = 300});
  EXPECT_EQ(sink->stores, 1);
}

TEST(MatchStatsReporter, UnavailablePlatformChangesNothing) {
  auto sink = std::make_shared<FakeStats>();
  sink->available = false;
  MatchStatsReporter reporter(sink);
  reporter.report({.victory = true, .enemies_defeated = 9});
  EXPECT_TRUE(sink->values.empty());
}

TEST(MatchStatsReporter, TotalsSaturateInsteadOfOverflowing) {
  auto sink = std::make_shared<FakeStats>();
  sink->values["soi_enemies_defeated"] = 2147483640;
  MatchStatsReporter reporter(sink);
  reporter.report({.enemies_defeated = 100});
  EXPECT_EQ(sink->values["soi_enemies_defeated"], 2147483647);
}

TEST(MatchStatsReporter, PeakArmySizeSurvivesTheArmyDying) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.note_army_size(40);
  reporter.note_army_size(120);
  reporter.note_army_size(0);
  reporter.report({.victory = false});
  EXPECT_EQ(sink->values["soi_highest_army_size"], 120);
  reporter.match_ended();
  reporter.note_army_size(10);
  reporter.report({.victory = false});
  EXPECT_EQ(sink->values["soi_highest_army_size"], 120);
}

TEST(MatchStatsReporter, WavesClearedAccumulate) {
  auto sink = std::make_shared<FakeStats>();
  MatchStatsReporter reporter(sink);
  reporter.report({.waves_cleared = 3});
  reporter.match_ended();
  reporter.report({.waves_cleared = 4});
  EXPECT_EQ(sink->values["soi_waves_cleared"], 7);
}

TEST(MatchStatsReporter, NullSinkIsSafe) {
  MatchStatsReporter reporter(nullptr);
  reporter.report({.victory = true, .enemies_defeated = 1});
  SUCCEED();
}

} // namespace
