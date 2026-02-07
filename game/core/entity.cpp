#include "entity.h"

#include <mutex>
#include <unordered_map>

namespace Engine::Core {

namespace {
std::mutex g_component_type_ids_mutex;
std::unordered_map<std::type_index, std::size_t> g_component_type_ids;
std::size_t g_next_component_type_id = 0;
} // namespace

Entity::Entity(EntityID id) : m_id(id) {}

auto Entity::get_id() const -> EntityID { return m_id; }

void Entity::set_component_change_callback(ComponentChangeCallback callback) {
  m_component_change_callback = std::move(callback);
}

auto Entity::resolve_component_type_id(std::type_index type) -> std::size_t {
  std::lock_guard<std::mutex> const lock(g_component_type_ids_mutex);
  auto const it = g_component_type_ids.find(type);
  if (it != g_component_type_ids.end()) {
    return it->second;
  }

  std::size_t const new_id = g_next_component_type_id++;
  g_component_type_ids.emplace(type, new_id);
  return new_id;
}

} // namespace Engine::Core
