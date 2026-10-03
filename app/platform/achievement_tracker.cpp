#include "achievement_tracker.h"

#include <utility>

namespace App::Platform {

AchievementTracker::AchievementTracker(std::shared_ptr<AchievementSink> sink)
    : m_sink(std::move(sink)) {
  if (!m_sink) {
    m_sink = std::make_shared<NullAchievementSink>();
  }
}

void AchievementTracker::unlock_once(const std::string& id) {
  if (m_sent.insert(id).second) {
    m_sink->unlock(id);
  }
}

void AchievementTracker::match_won(const std::string& mission_id,
                                   bool campaign_mission) {
  unlock_once("soi_first_victory");
  if (!campaign_mission) {
    return;
  }
  unlock_once("soi_first_mission");
  if (mission_id == "battle_of_cannae") {
    unlock_once("soi_battle_cannae");
  } else if (mission_id == "battle_of_zama") {
    unlock_once("soi_battle_zama");
  } else if (mission_id == "iron_sepulcher_watch") {
    unlock_once("soi_iron_sepulcher");
  }
}

void AchievementTracker::campaign_completed() {
  unlock_once("soi_campaign_complete");
}

void AchievementTracker::army_size(int men) {
  if (men >= k_large_army) {
    unlock_once("soi_army_of_100");
  }
}

} // namespace App::Platform
