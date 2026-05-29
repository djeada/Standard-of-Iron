#include "barracks_renderer_common.h"

#include "../../game/core/component.h"
#include "../gl/backend.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"

namespace Render::GL {

void register_barracks_renderer_variant(EntityRendererRegistry& registry,
                                        const BarracksRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "barracks",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if ((ctx.resources == nullptr) || (ctx.entity == nullptr)) {
          return;
        }

        auto* t = ctx.entity->get_component<Engine::Core::TransformComponent>();
        auto* r = ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if ((t == nullptr) || (r == nullptr)) {
          return;
        }

        Mesh* unit = ctx.resources->unit();
        if (unit == nullptr) {
          unit = get_unit_cube();
        }
        Texture* white = ctx.resources->white();
        const QVector3D team(r->color[0], r->color[1], r->color[2]);

        BarracksFlagRenderer::ClothBannerResources cloth;
        if (ctx.backend != nullptr) {
          cloth.cloth_mesh = ctx.backend->banner_mesh();
          cloth.banner_shader = ctx.backend->banner_shader();
        }

        submit_building_instance(
            out, ctx, config.archetype(resolve_building_state(ctx), unit, white));
        config.draw_ornaments(ctx, out, unit, white, team, &cloth);
        draw_building_health_bar(out, ctx, config.health_bar);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
