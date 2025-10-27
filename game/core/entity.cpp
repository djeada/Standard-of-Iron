#include "entity.h"

namespace Engine::Core {

Entity::Entity(EntityID id) : m_id(id) {}

auto Entity::getId() const -> EntityID { return m_id; }

} // namespace Engine::Core
