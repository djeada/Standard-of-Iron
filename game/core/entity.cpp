#include "entity.h"

namespace Engine::Core {

Entity::Entity(EntityID id) : m_id(id) {}

auto Entity::get_id() const -> EntityID { return m_id; }

} // namespace Engine::Core
