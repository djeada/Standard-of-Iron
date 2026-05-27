#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

class MovementSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

private:
  struct MovingUnitStepState {
    Engine::Core::EntityID entity_id{0};
    Engine::Core::Entity* entity{nullptr};
    float last_x{0.0F};
    float last_z{0.0F};
    float goal_x{0.0F};
    float goal_y{0.0F};
    float active_target_x{0.0F};
    float active_target_z{0.0F};
    float last_target_distance{0.0F};
    float stationary_seconds{0.0F};
    int recovery_attempts{0};
  };

  void prune_moving_units(Engine::Core::World* world);
  auto track_moving_unit(Engine::Core::Entity* entity,
                         const Engine::Core::TransformComponent* transform,
                         const Engine::Core::MovementComponent* movement)
      -> MovingUnitStepState*;
  void untrack_moving_unit(Engine::Core::EntityID entity_id);
  auto sample_moving_unit_progress(Engine::Core::Entity* entity,
                                   const Engine::Core::TransformComponent* transform,
                                   const Engine::Core::MovementComponent* movement,
                                   bool current_position_allowed,
                                   float delta_time) -> MovingUnitStepState*;

  void
  move_unit(Engine::Core::Entity* entity, Engine::Core::World* world, float delta_time);

  std::vector<MovingUnitStepState> m_moving_units;
  std::unordered_map<Engine::Core::EntityID, std::size_t> m_moving_unit_indices;
};

} // namespace Game::Systems
