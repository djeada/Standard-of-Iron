#pragma once

#include <memory>
#include <set>
#include <string>

#include "achievement_sink.h"

namespace App::Platform {

// Maps authoritative session results to achievements. It holds no gameplay
// rules: a mission victory is a victory the engine already decided. Each id
// is sent once per process, so reloads and repeated events cannot re-award;
// the platform itself ignores an unlock that is already recorded.
//
//   soi_first_victory      any match won
//   soi_first_mission      any campaign mission won
//   soi_campaign_complete  a campaign finished
//   soi_battle_cannae      battle_of_cannae won
//   soi_battle_zama        battle_of_zama won
//   soi_iron_sepulcher     iron_sepulcher_watch won
//   soi_army_of_100        an army of 100 or more men at once
class AchievementTracker {
public:
  static constexpr int k_large_army = 100;

  explicit AchievementTracker(std::shared_ptr<AchievementSink> sink);

  void match_won(const std::string& mission_id, bool campaign_mission);
  void campaign_completed();
  void army_size(int men);

private:
  void unlock_once(const std::string& id);

  std::shared_ptr<AchievementSink> m_sink;
  std::set<std::string> m_sent;
};

} // namespace App::Platform
