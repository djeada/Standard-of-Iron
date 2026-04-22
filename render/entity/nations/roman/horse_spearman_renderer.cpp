#include "horse_spearman_renderer.h"

#include "../../../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
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
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Roman
