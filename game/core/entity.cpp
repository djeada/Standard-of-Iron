#include "entity.h"

#include <mutex>
#include <unordered_map>
#include <utility>

namespace Engine::Core {

namespace {
std::mutex g_component_type_ids_mutex;
std::unordered_map<std::type_index, ComponentTypeId> g_component_type_ids;
ComponentTypeId g_next_component_type_id = 0;
} // namespace

Entity::Entity(EntityID id)
    : m_id(id) {
}

auto Entity::get_id() const -> EntityID {
  return m_id;
}

void Entity::set_component_change_callback(ComponentChangeCallback callback) {
  m_component_change_callback = std::move(callback);
}

auto resolve_component_type_id(std::type_index type) -> ComponentTypeId {
  const std::lock_guard<std::mutex> lock(g_component_type_ids_mutex);
  const auto it = g_component_type_ids.find(type);
  if (it != g_component_type_ids.end()) {
    return it->second;
  }

  const ComponentTypeId new_id = g_next_component_type_id++;
  g_component_type_ids.emplace(type, new_id);
  return new_id;
}

auto component_type_count() -> std::size_t {
  const std::lock_guard<std::mutex> lock(g_component_type_ids_mutex);
  return g_next_component_type_id;
}

} // namespace Engine::Core
