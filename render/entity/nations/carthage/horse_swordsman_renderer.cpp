#include "horse_swordsman_renderer.h"

#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../gl/backend.h"
#include "../../../gl/shader.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"

#include <memory>

namespace Render::GL::Carthage {
namespace {

auto makeMountedKnightConfig() -> MountedKnightRendererConfig {
  MountedKnightRendererConfig config;
  config.sword_equipment_id = "sword_carthage";
  config.shield_equipment_id = "shield_carthage_cavalry";
  config.helmet_equipment_id = "carthage_heavy";
  config.armor_equipment_id = "armor_heavy_carthage";
  config.helmet_offset_moving = 0.03F;
  config.horse_attachments.emplace_back(
      std::make_shared<CarthageSaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void registerMountedKnightRenderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_swordsman",
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
          scene_renderer->setCurrentShader(horse_swordsman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
