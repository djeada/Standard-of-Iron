#pragma once

#include "render/equipment/equipment_registry.h"

namespace Render::GL {

// Shared field-worker props. A straw sun hat is not faction dress, so both
// nations hang the same one off their own civilian rig; the tilted variant is
// the same hat pulled down over a sleeping face.
struct FarmWorkerProps {
  EquipmentHandle sun_hat{k_invalid_equipment_handle};
  EquipmentHandle tilted_sun_hat{k_invalid_equipment_handle};
  EquipmentHandle wheat_sheaf{k_invalid_equipment_handle};
};

// Registers the ids and their humanoid contributions on first call.
[[nodiscard]] auto farm_worker_props() -> const FarmWorkerProps&;

} // namespace Render::GL
