#include "body_profile.h"

#include "../core/component_commander.h"
#include "../core/component_core.h"
#include "../core/entity.h"

namespace Game::Systems {

auto body_profile_for(const Engine::Core::Entity& entity) -> BodyProfile {
  BodyProfile profile;
  if (auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
      movement != nullptr) {
    profile.radius = movement->get_navigation_clearance();
    profile.passability = movement->get_can_enter_forest()
                              ? Pathfinding::Passability::Light
                              : Pathfinding::Passability::Heavy;
  }
  if (entity.has_component<Engine::Core::CommanderComponent>()) {
    profile.radius = k_person_body_radius;
    profile.stops_at_building_facade = true;
  }
  return profile;
}

} // namespace Game::Systems
