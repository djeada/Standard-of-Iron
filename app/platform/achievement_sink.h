#pragma once

#include <memory>
#include <string>

namespace App::Platform {

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
