#pragma once

#include "walkability.h"

namespace Engine::Core {
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

inline constexpr float k_person_body_radius = 0.34F;

[[nodiscard]] auto body_profile_for(const Engine::Core::Entity& entity) -> BodyProfile;

} // namespace Game::Systems
