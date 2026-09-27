#pragma once

#include "walkability.h"

namespace Engine::Core {
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

[[nodiscard]] auto body_profile_for(const Engine::Core::Entity& entity) -> BodyProfile;

// Where a body's centre may go: the cell under it must be open, the way A*
// routes. The RTS motor and the directly controlled commander both ask this,
// so control mode never changes what ground a commander can cross. The body's
// circle (body_profile_for) spaces bodies from each other and from thin
// obstacles; it is not a second ground rule.
[[nodiscard]] auto motor_profile_for(const Engine::Core::Entity& entity) -> BodyProfile;

} // namespace Game::Systems
