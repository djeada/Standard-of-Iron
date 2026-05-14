#pragma once

#include <string>
#include <string_view>

#include "../../equipment/equipment_registry.h"

namespace Render::GL::Nation {

struct EquipmentLoadoutIds {
  std::string bow{};
  std::string quiver{};
  std::string sword{};
  std::string spear{};
  std::string shield{};
  std::string helmet{};
  std::string greaves{};
  std::string armor{};
  std::string shoulder{};
  std::string cloak{};
  std::string tool_belt{};
  std::string work_apron{};
  std::string arm_guards{};
  std::string horse_saddle{};
  std::string horse_bridle{};
  std::string horse_reins{};
  std::string horse_blanket{};
  std::string horse_barding{};
  std::string horse_crupper{};
  std::string horse_decoration{};
};

struct ResolvedEquipmentLoadout {
  EquipmentLoadoutIds ids{};

  EquipmentHandle bow_handle{k_invalid_equipment_handle};
  EquipmentHandle quiver_handle{k_invalid_equipment_handle};
  EquipmentHandle sword_handle{k_invalid_equipment_handle};
  EquipmentHandle spear_handle{k_invalid_equipment_handle};
  EquipmentHandle shield_handle{k_invalid_equipment_handle};
  EquipmentHandle helmet_handle{k_invalid_equipment_handle};
  EquipmentHandle greaves_handle{k_invalid_equipment_handle};
  EquipmentHandle armor_handle{k_invalid_equipment_handle};
  EquipmentHandle shoulder_handle{k_invalid_equipment_handle};
  EquipmentHandle cloak_handle{k_invalid_equipment_handle};
  EquipmentHandle tool_belt_handle{k_invalid_equipment_handle};
  EquipmentHandle work_apron_handle{k_invalid_equipment_handle};
  EquipmentHandle arm_guards_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_saddle_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_bridle_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_reins_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_blanket_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_barding_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_crupper_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_decoration_handle{k_invalid_equipment_handle};

  bool found{false};
};

[[nodiscard]] auto
resolve_equipment_loadout(std::string_view renderer_key) -> ResolvedEquipmentLoadout;

} // namespace Render::GL::Nation
