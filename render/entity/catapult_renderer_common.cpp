#include "catapult_renderer_common.h"

#include "../../game/core/component.h"
#include "../gl/primitives.h"
#include "../scene_renderer.h"

namespace Render::GL {

void register_catapult_renderer_variant(EntityRendererRegistry& registry,
                                        const CatapultRendererConfig& config) {
  registry.register_renderer(
      std::string(config.renderer_key),
      [config](const DrawContext& ctx, ISubmitter& out) {
        Mesh* unit = get_unit_cube();
        Texture* white = nullptr;

        if (auto* scene_renderer = dynamic_cast<Renderer*>(&out)) {
          unit = scene_renderer->get_mesh_cube();
          white = scene_renderer->get_white_texture();
        }

        if (unit == nullptr || white == nullptr) {
          return;
        }

        QVector3D team_color = config.default_team;
        if (ctx.entity != nullptr) {
          if (auto* r =
                  ctx.entity->get_component<Engine::Core::RenderableComponent>()) {
            team_color = QVector3D(r->color[0], r->color[1], r->color[2]);
          }
        }

        config.draw_body(ctx, out, unit, white, team_color);
      });
}

} // namespace Render::GL
