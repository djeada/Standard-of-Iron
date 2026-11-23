#include "render/entity/lot_building_renderer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "game/core/entity.h"
#include "game/visuals/building_asset_key.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/entity_presentation.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/market_lot.h"
#include "render/entity/registry.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/submitter.h"

namespace Render::GL {

void register_lot_building_renderer(EntityRendererRegistry& registry,
                                    const LotBuildingConfig& config) {
  auto sets = std::make_shared<std::vector<const BuildingArchetypeSet*>>();
  for (int variant = 0; variant < config.variant_count; ++variant) {
    sets->push_back(&building_archetype_set(std::string(config.archetype_prefix) +
                                            std::to_string(variant)));
  }
  register_building_renderer(
      registry,
      config.nation_slug,
      Game::Visuals::styled_building_type(config.building_type, config.style),
      [config, sets](const DrawContext& ctx,
                     const EntityPresentation& presentation,
                     ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        const float x = ctx.model(0, 3);
        const float z = ctx.model(2, 3);
        const bool run = config.run_variant != nullptr;
        const std::uint32_t seed =
            run ? lot_run_seed(x, z, ctx.model(0, 2), ctx.model(2, 2))
                : home_placement_seed(
                      x, z, static_cast<std::uint32_t>(ctx.entity->get_id()));
        const int variant =
            std::clamp(run ? config.run_variant(seed)
                           : (config.variant != nullptr ? config.variant(x, z) : 0),
                       0,
                       config.variant_count - 1);
        const BuildingState state = presented_building_state(presentation, ctx);
        const TownPalette palette =
            config.palette(Render::entity_color(*ctx.entity), seed, state);
        submit_building_instance(
            out,
            ctx,
            (*sets)[static_cast<std::size_t>(variant)]->for_state(state),
            palette,
            run ? 1.0F + static_cast<float>(seed & 0xFFFU) : 0.0F);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

void register_market_lot_renderers(EntityRendererRegistry& registry) {
  struct Nation {
    std::string_view slug;
    TownPaletteResolver palette;
  };
  for (const Nation& nation : {Nation{"roman", &roman_town_palette},
                               Nation{"carthage", &carthage_town_palette}}) {
    static const std::string k_roman_portico = "roman_portico_variant_";
    static const std::string k_punic_portico = "carthage_portico_variant_";
    static const std::string k_roman_dock = "roman_dock_variant_";
    static const std::string k_punic_dock = "carthage_dock_variant_";
    const bool roman = nation.slug == "roman";
    register_lot_building_renderer(
        registry,
        LotBuildingConfig{.nation_slug = nation.slug,
                          .building_type = "marketplace",
                          .style = "portico",
                          .archetype_prefix = roman ? k_roman_portico : k_punic_portico,
                          .variant_count = k_portico_variant_count,
                          .run_variant = &portico_run_variant,
                          .palette = nation.palette,
                          .selection = BuildingSelectionStyle{3.1F, 3.1F}});
    register_lot_building_renderer(
        registry,
        LotBuildingConfig{
            .nation_slug = nation.slug,
            .building_type = "marketplace",
            .style = "basilica",
            .archetype_prefix = roman ? k_roman_portico : k_punic_portico,
            .variant_count = k_portico_variant_count,
            .run_variant = [](std::uint32_t) { return k_portico_basilica_variant; },
            .palette = nation.palette,
            .selection = BuildingSelectionStyle{3.1F, 3.1F}});
    register_lot_building_renderer(
        registry,
        LotBuildingConfig{.nation_slug = nation.slug,
                          .building_type = "marketplace",
                          .style = "dock",
                          .archetype_prefix = roman ? k_roman_dock : k_punic_dock,
                          .variant_count = k_dock_variant_count,
                          .variant = &dock_visual_variant,
                          .palette = nation.palette,
                          .selection = BuildingSelectionStyle{3.1F, 3.1F}});
  }
}

} // namespace Render::GL
