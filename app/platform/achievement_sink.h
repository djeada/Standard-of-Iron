#pragma once

#include <memory>
#include <string>

namespace App::Platform {

// Platform achievements (Steam User Stats). Ids are the API names configured
// in Steamworks and are stable once released.
class AchievementSink {
public:
  virtual ~AchievementSink() = default;
  virtual void unlock(const std::string& id) = 0;
};

class NullAchievementSink final : public AchievementSink {
public:
  void unlock(const std::string&) override {}
};

auto make_platform_achievement_sink() -> std::unique_ptr<AchievementSink>;

} // namespace App::Platform
