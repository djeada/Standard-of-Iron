#include "equipment_registry.h"
#include <utility>

namespace Render::GL {

auto EquipmentRegistry::instance() -> EquipmentRegistry & {
  static EquipmentRegistry registry;
  return registry;
}

void EquipmentRegistry::registerEquipment(
    EquipmentCategory category, const std::string &id,
    std::shared_ptr<IEquipmentRenderer> renderer) {
  if (renderer == nullptr) {
    return;
  }
  m_renderers[category][id] = std::move(renderer);
}

auto EquipmentRegistry::get(EquipmentCategory category,
                            const std::string &id) const
    -> std::shared_ptr<IEquipmentRenderer> {
  auto category_it = m_renderers.find(category);
  if (category_it == m_renderers.end()) {
    return nullptr;
  }

  auto renderer_it = category_it->second.find(id);
  if (renderer_it == category_it->second.end()) {
    return nullptr;
  }

  return renderer_it->second;
}

auto EquipmentRegistry::has(EquipmentCategory category,
                            const std::string &id) const -> bool {
  auto category_it = m_renderers.find(category);
  if (category_it == m_renderers.end()) {
    return false;
  }

  return category_it->second.find(id) != category_it->second.end();
}

} // namespace Render::GL
