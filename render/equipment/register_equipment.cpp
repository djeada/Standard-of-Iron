#include "equipment_registry.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include <memory>

namespace Render::GL {

void registerBuiltInEquipment() {
  auto &registry = EquipmentRegistry::instance();

  // Register Carthaginian bow (composite recurve bow - Phoenician style)
  // Carthaginians used composite bows influenced by Eastern Mediterranean designs
  auto carthage_bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_carthage", carthage_bow);

  // Register Roman bow (simple self bow with moderate curve)
  // Romans primarily used simple wooden bows, less curved than composite bows
  auto roman_bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_roman", roman_bow);

  // Register Kingdom/European bow (longbow style - taller and straighter)
  // European medieval longbows were characteristically tall and relatively straight
  auto kingdom_bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_kingdom", kingdom_bow);

  // Register generic bow for backward compatibility
  auto bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow", bow);

  // Register quiver renderer
  auto quiver = std::make_shared<QuiverRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "quiver", quiver);
}

} // namespace Render::GL
