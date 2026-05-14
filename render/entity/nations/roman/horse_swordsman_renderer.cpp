#include "horse_swordsman_renderer.h"

#include "../../../../render/creature/pipeline/creature_asset.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"
#include "../equipment_loadout_catalog.h"

namespace Render::GL::Roman {
namespace {

auto make_mounted_knight_config() -> MountedKnightRendererConfig {
  MountedKnightRendererConfig config;
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/horse_swordsman");
  config.sword_equipment_id = loadout.ids.sword;
  config.shield_equipment_id = loadout.ids.shield;
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
  config.sword_handle = loadout.sword_handle;
  config.shield_handle = loadout.shield_handle;
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
  config.rider_debug_name = "troops/roman/horse_swordsman/rider";
  config.mount_debug_name = "troops/roman/horse_swordsman/mount";
  config.rider_creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  return config;
}

} // namespace

void register_mounted_knight_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer("troops/roman/horse_swordsman",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static MountedKnightRendererBase const static_renderer(
                                   make_mounted_knight_config());
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
