#include "horse_archer_renderer.h"

#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../gl/backend.h"
#include "../../../gl/shader.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../horse_archer_renderer_base.h"

#include <memory>

namespace Render::GL::Carthage {
namespace {

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  config.bow_equipment_id = "bow_carthage";
  config.quiver_equipment_id = "quiver";
  config.helmet_equipment_id = "carthage_light";
  config.armor_equipment_id = "armor_light_carthage";
  config.cloak_equipment_id = "cloak_carthage";
  config.has_cloak = true;
  config.cloak_color = {0.14F, 0.38F, 0.54F};
  config.cloak_trim_color = {0.75F, 0.66F, 0.42F};
  config.cloak_back_material_id = 12;
  config.cloak_shoulder_material_id = 13;
  config.helmet_offset_moving = 0.035F;
  config.fletching_color = {0.85F, 0.40F, 0.40F};
  config.horse_attachments.emplace_back(
      std::make_shared<CarthageSaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_archer",
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
          scene_renderer->set_current_shader(horse_archer_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->set_current_shader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
