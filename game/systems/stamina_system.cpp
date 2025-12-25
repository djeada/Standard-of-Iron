#include "stamina_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"

namespace Game::Systems {

namespace {
constexpr float kMinMovementSpeedSq = 0.01F;

[[nodiscard]] inline auto is_unit_moving(
    const Engine::Core::MovementComponent *movement) noexcept -> bool {
  if (movement == nullptr) {
    return false;
  }
  const float speed_sq =
      movement->vx * movement->vx + movement->vz * movement->vz;
  return speed_sq > kMinMovementSpeedSq;
}
} 

void StaminaSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto *entity :
       world->get_entities_with<Engine::Core::StaminaComponent>()) {
    auto *stamina = entity->get_component<Engine::Core::StaminaComponent>();
    if (stamina == nullptr) {
      continue;
    }

    const auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      stamina->is_running = false;
      continue;
    }

    if (!Game::Units::can_use_run_mode(unit->spawn_type)) {
      stamina->is_running = false;
      stamina->run_requested = false;
      continue;
    }

    const auto *movement =
        entity->get_component<Engine::Core::MovementComponent>();
    const bool is_moving = is_unit_moving(movement);

    if (stamina->run_requested && is_moving) {
      if (!stamina->is_running && stamina->can_start_running()) {
        stamina->is_running = true;
      }
      if (stamina->is_running) {
        stamina->deplete(delta_time);
        if (!stamina->has_stamina()) {
          stamina->is_running = false;
        }
      }
    } else {
      stamina->is_running = false;
      stamina->regenerate(delta_time);
    }
  }
}

} 
