#include "register_equipment_internal.h"

namespace Render::GL {

void register_built_in_equipment() {
  auto& registry = EquipmentRegistry::instance();
  EquipmentRegistration::register_equipment_ids(registry);
  EquipmentRegistration::register_equipment_descriptors();
  EquipmentRegistration::register_equipment_archetypes();
}

} // namespace Render::GL
