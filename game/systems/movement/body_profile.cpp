#include "body_profile.h"

#include <algorithm>

#include "core/component_commander.h"
#include "core/component_core.h"
#include "core/entity.h"

namespace Game::Systems {

auto body_profile_for(const Engine::Core::Entity& entity) -> BodyProfile {
  BodyProfile profile;
  if (auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
      movement != nullptr) {

    profile.radius =
        std::min(movement->get_navigation_clearance(), k_person_body_radius);
    profile.passability = movement->get_can_enter_forest()
                              ? Pathfinding::Passability::Light
                              : Pathfinding::Passability::Heavy;
  }
  if (auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
      commander != nullptr) {
    profile.radius = k_person_body_radius;
    profile.stops_at_building_facade = true;
    profile.scrambles_hill_slopes = commander->fpv_controlled;
  }
  return profile;
}

auto motor_profile_for(const Engine::Core::Entity& entity) -> BodyProfile {
  BodyProfile profile = body_profile_for(entity);
  if (!profile.stops_at_building_facade) {
    profile.radius = 0.0F;
  }
  return profile;
}

} // namespace Game::Systems
