#include "horse_archer_renderer.h"

#include "../../../submitter.h"
#include "../../horse_archer_renderer_base.h"
#include "../equipment_loadout_catalog.h"

namespace Render::GL::Carthage {
namespace {

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_archer");
  config.bow_equipment_id = loadout.ids.bow;
  config.quiver_equipment_id = loadout.ids.quiver;
  config.helmet_equipment_id = loadout.ids.helmet;
  config.armor_equipment_id = loadout.ids.armor;
  config.cloak_equipment_id = loadout.ids.cloak;
  config.horse_saddle_equipment_id = loadout.ids.horse_saddle;
  config.horse_bridle_equipment_id = loadout.ids.horse_bridle;
  config.horse_reins_equipment_id = loadout.ids.horse_reins;
  config.horse_blanket_equipment_id = loadout.ids.horse_blanket;
  config.horse_barding_equipment_id = loadout.ids.horse_barding;
  config.horse_crupper_equipment_id = loadout.ids.horse_crupper;
  config.horse_decoration_equipment_id = loadout.ids.horse_decoration;
  config.bow_handle = loadout.bow_handle;
  config.quiver_handle = loadout.quiver_handle;
  config.helmet_handle = loadout.helmet_handle;
  config.armor_handle = loadout.armor_handle;
  config.cloak_handle = loadout.cloak_handle;
  config.horse_saddle_handle = loadout.horse_saddle_handle;
  config.horse_bridle_handle = loadout.horse_bridle_handle;
  config.horse_reins_handle = loadout.horse_reins_handle;
  config.horse_blanket_handle = loadout.horse_blanket_handle;
  config.horse_barding_handle = loadout.horse_barding_handle;
  config.horse_crupper_handle = loadout.horse_crupper_handle;
  config.horse_decoration_handle = loadout.horse_decoration_handle;
  config.has_cloak = loadout.cloak_handle != k_invalid_equipment_handle;
  config.rider_debug_name = "troops/carthage/horse_archer/rider";
  config.mount_debug_name = "troops/carthage/horse_archer/mount";
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer("troops/carthage/horse_archer",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static HorseArcherRendererBase const static_renderer(
                                   make_horse_archer_config());
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
