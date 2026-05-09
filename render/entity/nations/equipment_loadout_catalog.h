#pragma once

#include "../../equipment/equipment_registry.h"

#include <string>
#include <string_view>

namespace Render::GL::Nation {

struct EquipmentLoadoutIds {
  std::string bow{};
  std::string quiver{};
  std::string sword{};
  std::string spear{};
  std::string shield{};
  std::string helmet{};
  std::string armor{};
  std::string shoulder{};
  std::string cloak{};
};

struct ResolvedEquipmentLoadout {
  EquipmentLoadoutIds ids{};

  EquipmentHandle bow_handle{kInvalidEquipmentHandle};
  EquipmentHandle quiver_handle{kInvalidEquipmentHandle};
  EquipmentHandle sword_handle{kInvalidEquipmentHandle};
  EquipmentHandle spear_handle{kInvalidEquipmentHandle};
  EquipmentHandle shield_handle{kInvalidEquipmentHandle};
  EquipmentHandle helmet_handle{kInvalidEquipmentHandle};
  EquipmentHandle armor_handle{kInvalidEquipmentHandle};
  EquipmentHandle shoulder_handle{kInvalidEquipmentHandle};
  EquipmentHandle cloak_handle{kInvalidEquipmentHandle};

  bool found{false};
};

[[nodiscard]] auto resolve_equipment_loadout(std::string_view renderer_key)
    -> ResolvedEquipmentLoadout;

} // namespace Render::GL::Nation
