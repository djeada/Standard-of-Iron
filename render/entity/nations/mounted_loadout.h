#pragma once

#include <string>
#include <string_view>

#include "equipment_loadout_catalog.h"

namespace Render::GL::Nation {

template <typename Config>
void apply_mount_loadout(Config& config,
                         const ResolvedEquipmentLoadout& loadout,
                         std::string_view troop_path) {
  config.horse_saddle_equipment_id = loadout.ids.horse_saddle;
  config.horse_bridle_equipment_id = loadout.ids.horse_bridle;
  config.horse_reins_equipment_id = loadout.ids.horse_reins;
  config.horse_blanket_equipment_id = loadout.ids.horse_blanket;
  config.horse_barding_equipment_id = loadout.ids.horse_barding;
  config.horse_crupper_equipment_id = loadout.ids.horse_crupper;
  config.horse_decoration_equipment_id = loadout.ids.horse_decoration;

  config.horse_saddle_handle = loadout.horse_saddle_handle;
  config.horse_bridle_handle = loadout.horse_bridle_handle;
  config.horse_reins_handle = loadout.horse_reins_handle;
  config.horse_blanket_handle = loadout.horse_blanket_handle;
  config.horse_barding_handle = loadout.horse_barding_handle;
  config.horse_crupper_handle = loadout.horse_crupper_handle;
  config.horse_decoration_handle = loadout.horse_decoration_handle;

  config.rider_debug_name = std::string(troop_path) + "/rider";
  config.mount_debug_name = std::string(troop_path) + "/mount";
}

} // namespace Render::GL::Nation
