#include "horse_spearman_renderer.h"

#include "../../../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../gl/backend.h"
#include "../../../gl/shader.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../horse_spearman_renderer_base.h"

#include <memory>

namespace Render::GL::Roman {
namespace {

auto make_horse_spearman_config() -> HorseSpearmanRendererConfig {
  HorseSpearmanRendererConfig config;
  config.spear_equipment_id = "spear";
  config.helmet_equipment_id = "roman_heavy";
  config.armor_equipment_id = "roman_heavy_armor";
  config.shoulder_equipment_id = "roman_shoulder_cover_cavalry";
  config.has_shoulder = true;
  config.helmet_offset_moving = 0.06F;
  config.horse_attachments.emplace_back(
      std::make_shared<RomanSaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void register_horse_spearman_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/roman/horse_spearman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static HorseSpearmanRendererBase const static_renderer(
            make_horse_spearman_config());
        Shader *horse_spearman_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          horse_spearman_shader = ctx.backend->shader(shader_key);
          if (horse_spearman_shader == nullptr) {
            horse_spearman_shader =
                ctx.backend->shader(QStringLiteral("horse_spearman"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (horse_spearman_shader != nullptr)) {
          scene_renderer->setCurrentShader(horse_spearman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Roman
