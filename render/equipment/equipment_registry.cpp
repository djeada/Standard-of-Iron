#include "equipment_registry.h"
#include "horse/i_horse_equipment_renderer.h"
#include <limits>
#include <utility>

namespace Render::GL {

auto EquipmentRegistry::instance() -> EquipmentRegistry & {
  static EquipmentRegistry registry;
  return registry;
}

void EquipmentRegistry::register_equipment_entry(
    EquipmentCategory category, const std::string &id,
    std::shared_ptr<IEquipmentRenderer> renderer) {
  m_renderers[category][id] = std::move(renderer);
  const HandleKey key{category, id};
  if (const auto it = m_handles.find(key); it != m_handles.end()) {
    const std::size_t index = static_cast<std::size_t>(it->second - 1U);
    if (index < m_renderers_by_handle.size()) {
      m_renderers_by_handle[index] = m_renderers[category][id];
    }
    return;
  }

  if (m_renderers_by_handle.size() >=
      static_cast<std::size_t>(std::numeric_limits<EquipmentHandle>::max())) {
    return;
  }

  const EquipmentHandle handle =
      static_cast<EquipmentHandle>(m_renderers_by_handle.size() + 1U);
  m_renderers_by_handle.push_back(m_renderers[category][id]);
  m_handles.emplace(key, handle);
}

void EquipmentRegistry::register_equipment(
    EquipmentCategory category, const std::string &id,
    std::shared_ptr<IEquipmentRenderer> renderer) {
  if (renderer == nullptr) {
    return;
  }
  register_equipment_entry(category, id, std::move(renderer));
}

void EquipmentRegistry::register_placeholder_equipment(
    EquipmentCategory category, const std::string &id) {
  register_equipment_entry(category, id, nullptr);
}

void EquipmentRegistry::register_horse_equipment(
    EquipmentCategory category, const std::string &id,
    std::shared_ptr<IHorseEquipmentRenderer> renderer) {
  register_equipment(
      category, id,
      std::static_pointer_cast<IEquipmentRenderer>(std::move(renderer)));
}

auto EquipmentRegistry::get(EquipmentCategory category, const std::string &id)
    const -> std::shared_ptr<IEquipmentRenderer> {
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

auto EquipmentRegistry::get_horse(EquipmentCategory category,
                                  const std::string &id) const
    -> std::shared_ptr<IHorseEquipmentRenderer> {
  return std::dynamic_pointer_cast<IHorseEquipmentRenderer>(get(category, id));
}

auto EquipmentRegistry::has(EquipmentCategory category,
                            const std::string &id) const -> bool {
  auto category_it = m_renderers.find(category);
  if (category_it == m_renderers.end()) {
    return false;
  }

  return category_it->second.find(id) != category_it->second.end();
}

auto EquipmentRegistry::resolve_handle(EquipmentCategory category,
                                       const std::string &id) const
    -> EquipmentHandle {
  const HandleKey key{category, id};
  if (const auto it = m_handles.find(key); it != m_handles.end()) {
    return it->second;
  }
  return k_invalid_equipment_handle;
}

auto EquipmentRegistry::get(EquipmentHandle handle) const
    -> std::shared_ptr<IEquipmentRenderer> {
  if (handle == k_invalid_equipment_handle) {
    return nullptr;
  }
  const std::size_t index = static_cast<std::size_t>(handle - 1U);
  if (index >= m_renderers_by_handle.size()) {
    return nullptr;
  }
  return m_renderers_by_handle[index];
}

auto EquipmentRegistry::get_horse(EquipmentHandle handle) const
    -> std::shared_ptr<IHorseEquipmentRenderer> {
  return std::dynamic_pointer_cast<IHorseEquipmentRenderer>(get(handle));
}

auto EquipmentRegistry::has(EquipmentHandle handle) const -> bool {
  return get(handle) != nullptr;
}

} // namespace Render::GL
