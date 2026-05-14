#include "horse_spearman_renderer.h"

#include "../../../submitter.h"
#include "../../horse_spearman_renderer_base.h"
#include "../equipment_loadout_catalog.h"

namespace Render::GL::Carthage {
namespace {

auto make_horse_spearman_config() -> HorseSpearmanRendererConfig {
  HorseSpearmanRendererConfig config;
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_spearman");
  config.spear_equipment_id = loadout.ids.spear;
  config.helmet_equipment_id = loadout.ids.helmet;
  config.armor_equipment_id = loadout.ids.armor;
  config.shoulder_equipment_id = loadout.ids.shoulder;
  config.horse_saddle_equipment_id = loadout.ids.horse_saddle;
  config.horse_bridle_equipment_id = loadout.ids.horse_bridle;
  config.horse_reins_equipment_id = loadout.ids.horse_reins;
  config.horse_blanket_equipment_id = loadout.ids.horse_blanket;
  config.horse_barding_equipment_id = loadout.ids.horse_barding;
  config.horse_crupper_equipment_id = loadout.ids.horse_crupper;
  config.horse_decoration_equipment_id = loadout.ids.horse_decoration;
  config.spear_handle = loadout.spear_handle;
  config.helmet_handle = loadout.helmet_handle;
  config.armor_handle = loadout.armor_handle;
  config.shoulder_handle = loadout.shoulder_handle;
  config.horse_saddle_handle = loadout.horse_saddle_handle;
  config.horse_bridle_handle = loadout.horse_bridle_handle;
  config.horse_reins_handle = loadout.horse_reins_handle;
  config.horse_blanket_handle = loadout.horse_blanket_handle;
  config.horse_barding_handle = loadout.horse_barding_handle;
  config.horse_crupper_handle = loadout.horse_crupper_handle;
  config.horse_decoration_handle = loadout.horse_decoration_handle;
  config.has_shoulder = loadout.shoulder_handle != k_invalid_equipment_handle;
  config.rider_debug_name = "troops/carthage/horse_spearman/rider";
  config.mount_debug_name = "troops/carthage/horse_spearman/mount";
  return config;
}

} // namespace

void register_horse_spearman_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer("troops/carthage/horse_spearman",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static HorseSpearmanRendererBase const static_renderer(
                                   make_horse_spearman_config());
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
