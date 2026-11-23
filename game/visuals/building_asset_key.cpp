#include "building_asset_key.h"

namespace Game::Visuals {

auto building_asset_key(std::string_view nation_slug,
                        std::string_view building_type) -> std::string {
  return "troops/" + std::string(nation_slug) + "/" + std::string(building_type);
}

auto nation_asset_slug(Game::Systems::NationID nation_id) -> std::string_view {
  switch (nation_id) {
  case Game::Systems::NationID::Carthage:
  case Game::Systems::NationID::Gauls:
  case Game::Systems::NationID::Iberians:
    return "carthage";
  case Game::Systems::NationID::IronSepulcher:
    return "iron_sepulcher";
  case Game::Systems::NationID::RomanRepublic:
  default:
    return "roman";
  }
}

auto building_asset_key(Game::Systems::NationID nation_id,
                        std::string_view building_type) -> std::string {
  if (nation_id == Game::Systems::NationID::Iberians &&
      building_type.starts_with("wall")) {
    return building_asset_key("iberians", building_type);
  }
  return building_asset_key(nation_asset_slug(nation_id), building_type);
}

auto styled_building_type(std::string_view building_type,
                          std::string_view style) -> std::string {
  return std::string(building_type) + "_" + std::string(style);
}

auto styled_building_asset_key(Game::Systems::NationID nation_id,
                               std::string_view building_type,
                               std::string_view style) -> std::string {
  if (building_type.starts_with("wall")) {
    return apply_wall_style(building_asset_key(nation_id, building_type), style);
  }
  if (!style.empty()) {
    const std::string_view slug = nation_asset_slug(nation_id);
    for (const auto& entry : k_building_visual_styles) {
      if (entry.building_type == building_type && entry.style == style &&
          entry.nation_slug == slug) {
        return building_asset_key(slug, styled_building_type(building_type, style));
      }
    }
  }
  return building_asset_key(nation_id, building_type);
}

auto apply_wall_style(std::string asset_key, std::string_view style) -> std::string {
  if (style.empty()) {
    return asset_key;
  }
  constexpr std::string_view k_root = "troops/";
  if (!asset_key.starts_with(k_root)) {
    return asset_key;
  }
  const auto slash = asset_key.find('/', k_root.size());
  if (slash == std::string::npos) {
    return asset_key;
  }
  const std::string_view slug =
      std::string_view(asset_key).substr(k_root.size(), slash - k_root.size());
  for (const auto& entry : k_building_visual_styles) {
    if (entry.building_type.starts_with("wall") && entry.style == style &&
        entry.nation_slug == slug) {
      asset_key.insert(slash, "_" + std::string(style));
      return asset_key;
    }
  }
  return asset_key;
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
