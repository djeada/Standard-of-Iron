#include "equipment_registry.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include <memory>

namespace Render::GL {

void registerBuiltInEquipment() {
  auto &registry = EquipmentRegistry::instance();

  // Register bow renderer
  auto bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow", bow);

  // Register quiver renderer
  auto quiver = std::make_shared<QuiverRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "quiver", quiver);
}

} // namespace Render::GL
