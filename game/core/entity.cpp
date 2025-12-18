#include "entity.h"

namespace Engine::Core {

Entity::Entity(EntityID id) : m_id(id) {}

auto Entity::get_id() const -> EntityID { return m_id; }

void Entity::set_component_change_callback(ComponentChangeCallback callback) {
  m_component_change_callback = std::move(callback);
}

} 
