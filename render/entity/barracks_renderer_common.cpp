#include "barracks_renderer_common.h"

#include <array>

#include "../entity_appearance.h"
#include "game/core/component.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"

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
        const QVector3D team = Render::entity_color(*ctx.entity);

        BarracksFlagRenderer::ClothBannerResources cloth;
        if (ctx.backend != nullptr) {
          cloth.cloth_mesh = ctx.backend->banner_mesh();
          cloth.banner_shader = ctx.backend->banner_shader();
        }

        const std::array<QVector3D, 1> palette{team};
        submit_building_instance(
            out,
            ctx,
            config.archetype(resolve_building_state(ctx), unit, white),
            palette);
        config.draw_ornaments(ctx, out, unit, white, team, &cloth);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
