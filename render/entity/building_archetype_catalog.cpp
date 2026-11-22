#include "render/entity/building_archetype_catalog.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "game/visuals/building_asset_key.h"
#include "render/entity/farm_field.h"
#include "render/entity/home_yard.h"
#include "render/entity/nations/carthage/barracks_authoring.h"
#include "render/entity/nations/carthage/defense_tower_authoring.h"
#include "render/entity/nations/carthage/farm_authoring.h"
#include "render/entity/nations/carthage/home_authoring.h"
#include "render/entity/nations/carthage/marketplace_authoring.h"
#include "render/entity/nations/carthage/temple_authoring.h"
#include "render/entity/nations/carthage/wall_authoring.h"
#include "render/entity/nations/iberians/wall_authoring.h"
#include "render/entity/nations/roman/barracks_authoring.h"
#include "render/entity/nations/roman/defense_tower_authoring.h"
#include "render/entity/nations/roman/farm_authoring.h"
#include "render/entity/nations/roman/home_authoring.h"
#include "render/entity/nations/roman/marketplace_authoring.h"
#include "render/entity/nations/roman/temple_authoring.h"
#include "render/entity/nations/roman/wall_authoring.h"
#include "render/entity/wall_authoring.h"
#include "render/entity/wall_gate_authoring.h"

namespace Render::GL {
namespace {

auto kind_of(const std::string& nation, std::string_view type) -> std::string {
  return Game::Visuals::building_asset_key(nation, type);
}

void add_nation_buildings(std::vector<BuildingArchetypeCatalogEntry>& out,
                          const std::string& nation,
                          BuildingArchetypeDesc (*home)(BuildingState, int),
                          BuildingArchetypeDesc (*barracks)(BuildingState),
                          BuildingArchetypeDesc (*tower)(BuildingState),
                          BuildingArchetypeDesc (*marketplace)(BuildingState),
                          BuildingArchetypeDesc (*temple)(BuildingState),
                          BuildingArchetypeDesc (*farm)(BuildingState, int)) {
  for (int variant = 0; variant < k_home_visual_variant_count; ++variant) {
    const std::string name =
        nation + "_home" + (variant == 0 ? "" : "_variant_" + std::to_string(variant));
    out.push_back(
        {name,
         [home, variant](BuildingState state) { return home(state, variant); },
         {kind_of(nation, "home")}});
  }
  out.push_back({nation + "_barracks", barracks, {kind_of(nation, "barracks")}});
  out.push_back({nation + "_defense_tower", tower, {kind_of(nation, "defense_tower")}});
  out.push_back(
      {nation + "_marketplace", marketplace, {kind_of(nation, "marketplace")}});
  out.push_back({nation + "_temple", temple, {kind_of(nation, "temple")}});

  for (int stage = 0; stage < k_farm_render_stage_count; ++stage) {
    out.push_back({nation + "_farm_stage_" + std::to_string(stage),
                   [farm, stage](BuildingState state) { return farm(state, stage); },
                   {kind_of(nation, "farm")}});
  }
}

void add_nation_walls(std::vector<BuildingArchetypeCatalogEntry>& out,
                      const std::string& nation,
                      const WallPalette& palette,
                      const WallGeometry& geometry) {
  const std::string prefix = nation + "_wall_variant";
  // Every segment kind draws from the whole variant and walk set.
  std::vector<std::string> segment_kinds;
  for (const auto& [name, variant] : wall_renderer_variants()) {
    (void)variant;
    segment_kinds.push_back(kind_of(nation, name));
  }
  for (int variant = 0; variant < 6; ++variant) {
    out.push_back({prefix + "_" + std::to_string(variant),
                   [prefix, &palette, &geometry, variant](BuildingState) {
                     return build_wall_variant_desc(
                         prefix, palette, geometry, static_cast<WallVariant>(variant));
                   },
                   segment_kinds});
  }
  out.push_back({prefix + "_gate",
                 [prefix, &palette, &geometry](BuildingState) {
                   return build_wall_gate_desc(prefix, palette, geometry);
                 },
                 {kind_of(nation, "wall_gate")}});
  out.push_back({prefix + "_gate_leaf",
                 [prefix, &palette, &geometry](BuildingState) {
                   return build_wall_gate_leaf_desc(prefix, palette, geometry);
                 },
                 {kind_of(nation, "wall_gate")}});
  out.push_back({prefix + "_walk_span",
                 [prefix, &palette](BuildingState) {
                   return build_wall_walk_span_desc(prefix, palette);
                 },
                 segment_kinds});
  out.push_back({prefix + "_walk_landing",
                 [prefix, &palette](BuildingState) {
                   return build_wall_walk_landing_desc(prefix, palette);
                 },
                 segment_kinds});
  out.push_back({prefix + "_walk_stair",
                 [prefix, &palette](BuildingState) {
                   return build_wall_walk_stair_desc(prefix, palette);
                 },
                 segment_kinds});
  out.push_back({prefix + "_ladder",
                 [prefix, &palette](BuildingState) {
                   return build_wall_ladder_desc(prefix, palette);
                 },
                 {kind_of(nation, "wall_ladder")}});
}

auto build_catalog() -> std::vector<BuildingArchetypeCatalogEntry> {
  std::vector<BuildingArchetypeCatalogEntry> out;

  add_nation_buildings(out,
                       "roman",
                       &Roman::build_home_desc,
                       &Roman::build_barracks_desc,
                       &Roman::build_tower_desc,
                       &Roman::build_marketplace_desc,
                       &Roman::build_temple_desc,
                       &Roman::build_farm_desc);
  add_nation_buildings(out,
                       "carthage",
                       &Carthage::build_home_desc,
                       &Carthage::build_barracks_desc,
                       &Carthage::build_tower_desc,
                       &Carthage::build_marketplace_desc,
                       &Carthage::build_temple_desc,
                       &Carthage::build_farm_desc);

  out.push_back({"carthage_barracks_palms",
                 &Carthage::build_barracks_palms_desc,
                 {kind_of("carthage", "barracks")}});
  out.push_back({"roman_barracks_grounds",
                 &Roman::build_barracks_grounds_desc,
                 {kind_of("roman", "barracks")}});
  out.push_back({"carthage_barracks_grounds",
                 &Carthage::build_barracks_grounds_desc,
                 {kind_of("carthage", "barracks")}});
  out.push_back({"carthage_barracks_firs",
                 &Carthage::build_barracks_firs_desc,
                 {kind_of("carthage", "barracks")}});

  add_nation_walls(out, "roman", Roman::wall_palette(), Roman::wall_geometry());
  add_nation_walls(
      out, "carthage", Carthage::wall_palette(), Carthage::wall_geometry());
  add_nation_walls(
      out, "iberians", Iberians::wall_palette(), Iberians::wall_geometry());

  return out;
}

} // namespace

auto building_archetype_catalog() -> const std::vector<BuildingArchetypeCatalogEntry>& {
  static const std::vector<BuildingArchetypeCatalogEntry> k_catalog = build_catalog();
  return k_catalog;
}

} // namespace Render::GL
