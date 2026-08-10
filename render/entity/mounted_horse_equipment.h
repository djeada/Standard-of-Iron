#pragma once

#include <array>
#include <string>

#include "render/equipment/equipment_registry.h"

namespace Render::GL {

struct MountedHorseHandles {
  static constexpr std::size_t k_slot_count = 7;

  EquipmentHandle saddle{k_invalid_equipment_handle};
  EquipmentHandle bridle{k_invalid_equipment_handle};
  EquipmentHandle reins{k_invalid_equipment_handle};
  EquipmentHandle blanket{k_invalid_equipment_handle};
  EquipmentHandle barding{k_invalid_equipment_handle};
  EquipmentHandle crupper{k_invalid_equipment_handle};
  EquipmentHandle decoration{k_invalid_equipment_handle};

  [[nodiscard]] auto as_array() const -> std::array<EquipmentHandle, k_slot_count> {
    return {saddle, bridle, reins, blanket, barding, crupper, decoration};
  }
};

template <typename Config>
auto resolve_mounted_horse_handles(const Config& config) -> MountedHorseHandles {
  auto& registry = EquipmentRegistry::instance();
  auto resolve = [&registry](EquipmentHandle preset,
                             EquipmentCategory category,
                             const std::string& equipment_id) {
    if (preset == k_invalid_equipment_handle && !equipment_id.empty()) {
      return registry.resolve_handle(category, equipment_id);
    }
    return preset;
  };

  return {resolve(config.horse_saddle_handle,
                  EquipmentCategory::HorseTack,
                  config.horse_saddle_equipment_id),
          resolve(config.horse_bridle_handle,
                  EquipmentCategory::HorseTack,
                  config.horse_bridle_equipment_id),
          resolve(config.horse_reins_handle,
                  EquipmentCategory::HorseTack,
                  config.horse_reins_equipment_id),
          resolve(config.horse_blanket_handle,
                  EquipmentCategory::HorseTack,
                  config.horse_blanket_equipment_id),
          resolve(config.horse_barding_handle,
                  EquipmentCategory::HorseArmor,
                  config.horse_barding_equipment_id),
          resolve(config.horse_crupper_handle,
                  EquipmentCategory::HorseArmor,
                  config.horse_crupper_equipment_id),
          resolve(config.horse_decoration_handle,
                  EquipmentCategory::HorseDecoration,
                  config.horse_decoration_equipment_id)};
}

} // namespace Render::GL
