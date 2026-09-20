#pragma once

#include "render/equipment/equipment_registry.h"

namespace Render::GL {

struct FarmWorkerProps {
  EquipmentHandle sun_hat{k_invalid_equipment_handle};
  EquipmentHandle tilted_sun_hat{k_invalid_equipment_handle};
  EquipmentHandle wheat_sheaf{k_invalid_equipment_handle};
};

[[nodiscard]] auto farm_worker_props() -> const FarmWorkerProps&;

void register_farm_worker_prop_archetypes();

} // namespace Render::GL
