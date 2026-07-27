#include "building_asset_key.h"

namespace Game::Visuals {

auto building_asset_key(std::string_view nation_slug,
                        std::string_view building_type) -> std::string {
  return "troops/" + std::string(nation_slug) + "/" + std::string(building_type);
}

auto building_asset_key(Game::Systems::NationID nation_id,
                        std::string_view building_type) -> std::string {
  switch (nation_id) {
  case Game::Systems::NationID::Carthage:
    return building_asset_key("carthage", building_type);
  case Game::Systems::NationID::IronSepulcher:
    return building_asset_key("iron_sepulcher", building_type);
  case Game::Systems::NationID::RomanRepublic:
  default:
    return building_asset_key("roman", building_type);
  }
}

auto canonicalize_building_asset_key(std::string_view asset_key) -> std::string_view {
  if (asset_key == "barracks_roman") {
    return "troops/roman/barracks";
  }
  if (asset_key == "barracks_carthage") {
    return "troops/carthage/barracks";
  }
  return asset_key;
}

auto resolve_building_asset_key(std::string_view asset_key,
                                std::string_view building_type,
                                Game::Systems::NationID nation_id) -> std::string {
  if (asset_key.empty() || asset_key == building_type) {
    return building_asset_key(nation_id, building_type);
  }
  return std::string(canonicalize_building_asset_key(asset_key));
}

} // namespace Game::Visuals
