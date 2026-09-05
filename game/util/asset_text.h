#ifndef GAME_UTIL_ASSET_TEXT_H
#define GAME_UTIL_ASSET_TEXT_H

#include <QString>

#include <string>

namespace Game::Util {

inline constexpr const char* k_missions_context = "Missions";
inline constexpr const char* k_campaigns_context = "Campaigns";
inline constexpr const char* k_maps_context = "Maps";
inline constexpr const char* k_nations_context = "Nations";

inline constexpr const char* k_formations_context = "Formation";
inline constexpr const char* k_campaign_map_context = "CampaignMap";
inline constexpr const char* k_units_context = "Units";
inline constexpr const char* k_commanders_context = "Commanders";
inline constexpr const char* k_commander_voices_context = "CommanderVoices";

[[nodiscard]] auto tr_asset(const char* context, const QString& source) -> QString;
[[nodiscard]] auto tr_asset(const char* context, const std::string& source) -> QString;

[[nodiscard]] auto tr_asset_std(const char* context,
                                const std::string& source) -> std::string;

} // namespace Game::Util

#endif
