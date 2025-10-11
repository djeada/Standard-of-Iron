#include "entity.h"
#include <typeindex>

namespace Engine::Core {

Entity::Entity(EntityID id) : m_id(id) {}

EntityID Entity::getId() const { return m_id; }

} // namespace Engine::Core
