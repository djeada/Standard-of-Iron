#include "siege_renderer_common.h"

#include "../entity_appearance.h"
#include "game/core/component.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"

namespace Render::GL {

void register_siege_renderer_variant(EntityRendererRegistry& registry,
                                     const SiegeRendererConfig& config) {
  registry.register_renderer(
      std::string(config.renderer_key),
      [config](const DrawContext& ctx, ISubmitter& out) {
        Mesh* unit = get_unit_cube();
        Texture* white = nullptr;

        if (ctx.resources != nullptr) {
          unit = ctx.resources->unit();
          white = ctx.resources->white();
        }
        if (auto* scene_renderer = dynamic_cast<Renderer*>(out.unwrap_submitter())) {
          unit = scene_renderer->get_mesh_cube();
          white = scene_renderer->get_white_texture();
        }

        if (unit == nullptr) {
          return;
        }

        QVector3D team_color = config.default_team;
        if (ctx.entity != nullptr) {
          if (ctx.entity->get_component<Engine::Core::RenderableComponent>() !=
              nullptr) {
            team_color = Render::entity_color(*ctx.entity);
          }
        }

        config.draw_body(ctx, out, unit, white, team_color);
      });
}

} // namespace Render::GL
