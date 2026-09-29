#pragma once

#include "systems/navigation/walkability.h"

namespace Engine::Core {
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

[[nodiscard]] auto body_profile_for(const Engine::Core::Entity& entity) -> BodyProfile;

[[nodiscard]] auto motor_profile_for(const Engine::Core::Entity& entity) -> BodyProfile;

} // namespace Game::Systems
