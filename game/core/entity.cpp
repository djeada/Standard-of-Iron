#include "entity.h"
#include <typeindex>

namespace Engine::Core {

Entity::Entity(EntityID id) : m_id(id) {}

EntityID Entity::getId() const { return m_id; }

// Template methods remain header-only in previous layout.
// For simplicity, we keep non-template parts here.

} // namespace Engine::Core
