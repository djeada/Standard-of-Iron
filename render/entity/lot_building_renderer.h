#pragma once

#include <cstdint>
#include <string_view>

#include "render/entity/building_render_common.h"
#include "render/entity/town_palette.h"

namespace Render::GL {

class EntityRendererRegistry;

// A building style that is one baked mesh per variant and nothing else: no
// activity, no attachments. The variant is a function of where it stands.
struct LotBuildingConfig {
  std::string_view nation_slug;
  std::string_view building_type;
  std::string_view style;
  // "<prefix><variant>" names the baked archetype set of a variant.
  std::string_view archetype_prefix;
  int variant_count{1};
  // Which variant a lot draws, from where it stands...
  int (*variant)(float world_x, float world_z){nullptr};
  // ...or, for a building that is one of a run, from the run's seed: the whole
  // run then shares its variant, its colours and its weathering.
  int (*run_variant)(std::uint32_t run_seed){nullptr};
  TownPaletteResolver palette{nullptr};
  BuildingSelectionStyle selection{3.0F, 3.0F};
};

void register_lot_building_renderer(EntityRendererRegistry& registry,
                                    const LotBuildingConfig& config);

// The marketplace styles both nations draw: portico and dock.
void register_market_lot_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
