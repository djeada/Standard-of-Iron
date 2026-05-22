#include "equipment_registry.h"

#include <limits>

namespace Render::GL {

auto EquipmentRegistry::instance() -> EquipmentRegistry& {
  static EquipmentRegistry registry;
  return registry;
}

void EquipmentRegistry::register_equipment_entry(EquipmentCategory category,
                                                 const std::string& id) {
  m_registered_ids[category].insert(id);
  const HandleKey key{category, id};
  if (const auto it = m_handles.find(key); it != m_handles.end()) {
    return;
  }

  if (m_keys_by_handle.size() >=
      static_cast<std::size_t>(std::numeric_limits<EquipmentHandle>::max())) {
    return;
  }

  const auto handle = static_cast<EquipmentHandle>(m_keys_by_handle.size() + 1U);
  m_keys_by_handle.push_back(key);
  m_handles.emplace(key, handle);
}

void EquipmentRegistry::register_equipment_id(EquipmentCategory category,
                                              const std::string& id) {
  register_equipment_entry(category, id);
}

void EquipmentRegistry::register_placeholder_equipment(EquipmentCategory category,
                                                       const std::string& id) {
  register_equipment_entry(category, id);
}

auto EquipmentRegistry::has(EquipmentCategory category,
                            const std::string& id) const -> bool {
  const auto category_it = m_registered_ids.find(category);
  if (category_it == m_registered_ids.end()) {
    return false;
  }

  return category_it->second.contains(id);
}

auto EquipmentRegistry::resolve_handle(EquipmentCategory category,
                                       const std::string& id) const -> EquipmentHandle {
  const HandleKey key{category, id};
  if (const auto it = m_handles.find(key); it != m_handles.end()) {
    return it->second;
  }
  return k_invalid_equipment_handle;
}

auto EquipmentRegistry::has(EquipmentHandle handle) const -> bool {
  if (handle == k_invalid_equipment_handle) {
    return false;
  }
  const auto index = static_cast<std::size_t>(handle - 1U);
  return index < m_keys_by_handle.size();
}

} // namespace Render::GL
