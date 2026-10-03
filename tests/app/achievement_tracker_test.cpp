#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "app/platform/achievement_tracker.h"

namespace {

using App::Platform::AchievementSink;
using App::Platform::AchievementTracker;

class RecordingSink final : public AchievementSink {
public:
  void unlock(const std::string& id) override { ids.push_back(id); }
  [[nodiscard]] auto count(const std::string& id) const -> int {
    int n = 0;
    for (const auto& i : ids) {
      n += i == id ? 1 : 0;
    }
    return n;
  }
  std::vector<std::string> ids;
};

TEST(AchievementTracker, SkirmishWinUnlocksOnlyTheFirstVictory) {
  auto sink = std::make_shared<RecordingSink>();
  AchievementTracker tracker(sink);
  tracker.match_won("", false);
  ASSERT_EQ(sink->ids.size(), 1U);
  EXPECT_EQ(sink->ids[0], "soi_first_victory");
}

TEST(AchievementTracker, CampaignMissionAddsTheMissionAchievements) {
  auto sink = std::make_shared<RecordingSink>();
  AchievementTracker tracker(sink);
  tracker.match_won("battle_of_cannae", true);
  EXPECT_EQ(sink->count("soi_first_victory"), 1);
  EXPECT_EQ(sink->count("soi_first_mission"), 1);
  EXPECT_EQ(sink->count("soi_battle_cannae"), 1);
  EXPECT_EQ(sink->count("soi_battle_zama"), 0);
}

TEST(AchievementTracker, IronSepulcherAndZamaHaveTheirOwn) {
  auto sink = std::make_shared<RecordingSink>();
  AchievementTracker tracker(sink);
  tracker.match_won("iron_sepulcher_watch", true);
  tracker.match_won("battle_of_zama", true);
  EXPECT_EQ(sink->count("soi_iron_sepulcher"), 1);
  EXPECT_EQ(sink->count("soi_battle_zama"), 1);
}

TEST(AchievementTracker, RepeatedEventsDoNotReAward) {
  auto sink = std::make_shared<RecordingSink>();
  AchievementTracker tracker(sink);
  for (int i = 0; i < 3; ++i) {
    tracker.match_won("battle_of_zama", true);
    tracker.campaign_completed();
    tracker.army_size(150);
  }
  for (const auto& id : sink->ids) {
    EXPECT_EQ(sink->count(id), 1) << id;
  }
}

TEST(AchievementTracker, LargeArmyNeedsTheThreshold) {
  auto sink = std::make_shared<RecordingSink>();
  AchievementTracker tracker(sink);
  tracker.army_size(AchievementTracker::k_large_army - 1);
  EXPECT_TRUE(sink->ids.empty());
  tracker.army_size(AchievementTracker::k_large_army);
  EXPECT_EQ(sink->count("soi_army_of_100"), 1);
}

TEST(AchievementTracker, NullSinkIsSafe) {
  AchievementTracker tracker(nullptr);
  tracker.match_won("battle_of_zama", true);
  tracker.campaign_completed();
  SUCCEED();
}

} // namespace
