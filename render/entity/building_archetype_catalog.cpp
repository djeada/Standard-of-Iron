#include "building_archetype_catalog.h"

#include <string>
#include <utility>

#include "farm_renderer_common.h"
#include "nations/carthage/barracks_renderer.h"
#include "nations/carthage/defense_tower_renderer.h"
#include "nations/carthage/farm_renderer.h"
#include "nations/carthage/home_renderer.h"
#include "nations/carthage/marketplace_renderer.h"
#include "nations/carthage/temple_renderer.h"
#include "nations/carthage/wall_renderer.h"
#include "nations/roman/barracks_renderer.h"
#include "nations/roman/defense_tower_renderer.h"
#include "nations/roman/farm_renderer.h"
#include "nations/roman/home_renderer.h"
#include "nations/roman/marketplace_renderer.h"
#include "nations/roman/temple_renderer.h"
#include "nations/roman/wall_renderer.h"
#include "wall_gate_renderer_common.h"
#include "wall_renderer_common.h"

namespace Render::GL {
namespace {

void add_nation_buildings(std::vector<BuildingArchetypeCatalogEntry>& out,
                          const std::string& nation,
                          BuildingArchetypeDesc (*home)(BuildingState),
                          BuildingArchetypeDesc (*barracks)(BuildingState),
                          BuildingArchetypeDesc (*tower)(BuildingState),
                          BuildingArchetypeDesc (*marketplace)(BuildingState),
                          BuildingArchetypeDesc (*temple)(BuildingState),
                          BuildingArchetypeDesc (*farm)(BuildingState, int)) {
  out.push_back({nation + "_home", home});
  out.push_back({nation + "_barracks", barracks});
  out.push_back({nation + "_defense_tower", tower});
  out.push_back({nation + "_marketplace", marketplace});
  out.push_back({nation + "_temple", temple});

  for (int stage = 0; stage < k_farm_render_stage_count; ++stage) {
    out.push_back({nation + "_farm_stage_" + std::to_string(stage),
                   [farm, stage](BuildingState state) {
                     return farm(state, stage);
                   }});
  }
}

void add_nation_walls(std::vector<BuildingArchetypeCatalogEntry>& out,
                      const std::string& nation,
                      const WallPalette& palette,
                      const WallGeometry& geometry) {
  const std::string prefix = nation + "_wall_variant";
  for (int variant = 0; variant < 6; ++variant) {
    out.push_back({prefix + "_" + std::to_string(variant),
                   [prefix, &palette, &geometry, variant](BuildingState) {
                     return build_wall_variant_desc(
                         prefix, palette, geometry, static_cast<WallVariant>(variant));
                   }});
  }
  out.push_back({prefix + "_gate", [prefix, &palette, &geometry](BuildingState) {
                   return build_wall_gate_desc(prefix, palette, geometry);
                 }});
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

  add_nation_walls(out, "roman", Roman::wall_palette(), Roman::wall_geometry());
  add_nation_walls(
      out, "carthage", Carthage::wall_palette(), Carthage::wall_geometry());

  return out;
}

} // namespace

auto building_archetype_catalog() -> const std::vector<BuildingArchetypeCatalogEntry>& {
  static const std::vector<BuildingArchetypeCatalogEntry> k_catalog = build_catalog();
  return k_catalog;
}

} // namespace Render::GL
