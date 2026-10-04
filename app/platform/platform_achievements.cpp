#include "achievement_sink.h"

#ifdef SOI_STEAMWORKS
#include <steam/steam_api.h>
#endif

namespace App::Platform {

#ifdef SOI_STEAMWORKS
namespace {

class SteamAchievementSink final : public AchievementSink {
public:
  void unlock(const std::string& id) override {
    ISteamUserStats* stats = SteamUserStats();
    if (stats == nullptr) {
      return;
    }
    bool already = false;
    if (stats->GetAchievement(id.c_str(), &already) && already) {
      return;
    }
    if (stats->SetAchievement(id.c_str())) {
      stats->StoreStats();
    }
  }
};

} // namespace
#endif

auto make_platform_achievement_sink() -> std::unique_ptr<AchievementSink> {
#ifdef SOI_STEAMWORKS
  if (SteamAPI_Init()) {
    return std::make_unique<SteamAchievementSink>();
  }
#endif
  return std::make_unique<NullAchievementSink>();
}

} // namespace App::Platform
