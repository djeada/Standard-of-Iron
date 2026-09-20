#include "home_renderer_common.h"

#include "../entity_appearance.h"
#include "game/core/component_core.h"
#include "home_activity.h"
#include "home_props.h"

namespace Render::GL {

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config) {
  // Build the house props during warm_all() at renderer init, not on the
  // first frame that happens to draw a house.
  static const bool props_registered = [] {
    register_home_prop_archetypes();
    return true;
  }();
  (void)props_registered;
  register_building_renderer(
      registry,
      config.nation_slug,
      "home",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }

        auto* r = ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if (r == nullptr) {
          return;
        }

        const QVector3D team = Render::entity_color(*ctx.entity);
        const auto palette_slots = config.palette_slots(team);
        submit_building_instance(
            out, ctx, config.archetype(resolve_building_state(ctx)), palette_slots);
        submit_home_activity(ctx, out, config.nation_slug == "carthage");
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
