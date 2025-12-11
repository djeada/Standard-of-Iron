#include "horse_swordsman_renderer.h"

#include "../../../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../gl/backend.h"
#include "../../../gl/shader.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"

#include <memory>

namespace Render::GL::Roman {
namespace {

auto makeMountedKnightConfig() -> MountedKnightRendererConfig {
  MountedKnightRendererConfig config;
  config.sword_equipment_id = "sword_roman";
  config.shield_equipment_id = "shield_roman";
  config.helmet_equipment_id = "roman_heavy";
  config.armor_equipment_id = "roman_heavy_armor";
  config.shoulder_equipment_id = "roman_shoulder_cover_cavalry";
  config.has_shoulder = true;
  config.helmet_offset_moving = 0.035F;
  config.horse_attachments.emplace_back(
      std::make_shared<RomanSaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void registerMountedKnightRenderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/roman/horse_swordsman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static MountedKnightRendererBase const static_renderer(
            makeMountedKnightConfig());
        Shader *horse_swordsman_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          horse_swordsman_shader = ctx.backend->shader(shader_key);
          if (horse_swordsman_shader == nullptr) {
            horse_swordsman_shader =
                ctx.backend->shader(QStringLiteral("horse_swordsman"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) &&
            (horse_swordsman_shader != nullptr)) {
          scene_renderer->set_current_shader(horse_swordsman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->set_current_shader(nullptr);
        }
      });
}

} // namespace Render::GL::Roman
