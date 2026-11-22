#include "render/entity/farm_renderer_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "game/core/component_economy.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/farm_activity_submit.h"
#include "render/entity/farm_worker_props.h"
#include "render/frame/submitter.h"

namespace Render::GL {
auto farm_stage_archetype(std::string_view nation_slug,
                          BuildingState state,
                          int stage) -> const RenderArchetype& {
  const int clamped_stage = std::clamp(stage, 0, k_farm_render_stage_count - 1);
  std::string name(nation_slug);
  name += "_farm_stage_";
  name += std::to_string(clamped_stage);
  return building_archetype_set(name).for_state(state);
}

void register_farm_renderer_variant(EntityRendererRegistry& registry,
                                    const FarmRendererConfig& config) {

  static const bool props_registered = [] {
    register_farm_worker_prop_archetypes();
    return true;
  }();
  (void)props_registered;
  register_building_renderer(
      registry,
      config.nation_slug,
      "farm",
      [config](const DrawContext& ctx,
               const EntityPresentation& presentation,
               ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        int stage = 0;
        if (const auto* farm =
                ctx.entity->get_component<Engine::Core::FarmComponent>()) {
          stage = farm->growth_stage();
        }
        const BuildingState state = presented_building_state(presentation, ctx);
        submit_building_instance(out, ctx, config.archetype(state, stage));
        submit_farm_activity(
            ctx, out, config.nation_slug == "carthage", presentation.farm);
        submit_building_torches(ctx, out, config.torches);
        draw_building_selection_overlay(out, ctx, config.selection);
      },
      PresentationSystems{
          farm_activity_presentation(config.nation_slug == "carthage")});
}

} // namespace Render::GL
