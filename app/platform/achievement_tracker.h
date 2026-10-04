#pragma once

#include <memory>
#include <set>
#include <string>

#include "achievement_sink.h"

namespace App::Platform {

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
