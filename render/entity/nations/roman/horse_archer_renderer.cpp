#include "horse_archer_renderer.h"

#include "../../../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../submitter.h"
#include "../../horse_archer_renderer_base.h"

#include <memory>

namespace Render::GL::Roman {
namespace {

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  config.bow_equipment_id = "bow_roman";
  config.quiver_equipment_id = "quiver";
  config.helmet_equipment_id = "roman_light";
  config.armor_equipment_id = "roman_light_armor";
  config.cloak_equipment_id = "cloak_carthage";
  config.has_cloak = true;
  config.cloak_color = {0.70F, 0.15F, 0.18F};
  config.cloak_trim_color = {0.78F, 0.72F, 0.58F};
  config.cloak_back_material_id = 12;
  config.cloak_shoulder_material_id = 13;
  config.helmet_offset_moving = 0.04F;
  config.fletching_color = {0.85F, 0.40F, 0.40F};
  config.horse_attachments.emplace_back(
      std::make_shared<RomanSaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry &registry) {
	  registry.register_renderer(
	      "troops/roman/horse_archer", [](const DrawContext &ctx, ISubmitter &out) {
	        static HorseArcherRendererBase const static_renderer(
	            make_horse_archer_config());
	        static_renderer.render(ctx, out);
	      });
}

} // namespace Render::GL::Roman
