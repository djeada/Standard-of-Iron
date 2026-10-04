#include "stats_sink.h"

#ifdef SOI_STEAMWORKS
#include <steam/steam_api.h>
#endif

namespace App::Platform {

#ifdef SOI_STEAMWORKS
namespace {

class SteamStatsSink final : public StatsSink {
public:
  auto get_int(const std::string& api_name, int& value) -> bool override {
    ISteamUserStats* stats = SteamUserStats();
    return stats != nullptr && stats->GetStat(api_name.c_str(), &value);
  }
  void set_int(const std::string& api_name, int value) override {
    if (ISteamUserStats* stats = SteamUserStats()) {
      stats->SetStat(api_name.c_str(), value);
    }
  }
  void store() override {
    if (ISteamUserStats* stats = SteamUserStats()) {
      stats->StoreStats();
    }
  }
};

} // namespace
#endif

auto make_platform_stats_sink() -> std::unique_ptr<StatsSink> {
#ifdef SOI_STEAMWORKS
  if (SteamAPI_Init()) {
    return std::make_unique<SteamStatsSink>();
  }
#endif
  return std::make_unique<NullStatsSink>();
}

} // namespace App::Platform
