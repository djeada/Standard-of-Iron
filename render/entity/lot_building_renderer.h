#pragma once

#include <cstdint>
#include <string_view>

#include "render/entity/building_render_common.h"
#include "render/entity/town_palette.h"

namespace Render::GL {

class EntityRendererRegistry;

struct LotBuildingConfig {
  std::string_view nation_slug;
  std::string_view building_type;
  std::string_view style;

  std::string_view archetype_prefix;
  int variant_count{1};

  int (*variant)(float world_x, float world_z){nullptr};

  int (*run_variant)(std::uint32_t run_seed){nullptr};
  TownPaletteResolver palette{nullptr};
  BuildingSelectionStyle selection{3.0F, 3.0F};
};

void register_lot_building_renderer(EntityRendererRegistry& registry,
                                    const LotBuildingConfig& config);

void register_market_lot_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
