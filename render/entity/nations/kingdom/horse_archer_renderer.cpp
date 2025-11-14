#include "horse_archer_renderer.h"

#include "../../../equipment/horse/saddles/light_cavalry_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../gl/backend.h"
#include "../../../gl/shader.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../horse_archer_renderer_base.h"

#include <memory>

namespace Render::GL::Kingdom {
namespace {

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  config.bow_equipment_id = "bow_kingdom";
  config.quiver_equipment_id = "quiver";
  config.helmet_equipment_id = "kingdom_light";
  config.armor_equipment_id = "kingdom_light_armor";
  config.fletching_color = {0.85F, 0.40F, 0.40F};
  config.horse_attachments.emplace_back(
      std::make_shared<LightCavalrySaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/kingdom/horse_archer",
      [](const DrawContext &ctx, ISubmitter &out) {
        static HorseArcherRendererBase const static_renderer(
            make_horse_archer_config());
        Shader *horse_archer_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          horse_archer_shader = ctx.backend->shader(shader_key);
          if (horse_archer_shader == nullptr) {
            horse_archer_shader =
                ctx.backend->shader(QStringLiteral("horse_archer"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (horse_archer_shader != nullptr)) {
          scene_renderer->setCurrentShader(horse_archer_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Kingdom
