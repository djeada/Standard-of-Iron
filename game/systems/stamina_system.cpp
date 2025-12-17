#include "stamina_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {

void StaminaSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto entities = world->get_entities_with<Engine::Core::StaminaComponent>();

  for (auto *entity : entities) {
    auto *stamina = entity->get_component<Engine::Core::StaminaComponent>();
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    auto *movement = entity->get_component<Engine::Core::MovementComponent>();

    if (stamina == nullptr || unit == nullptr) {
      continue;
    }

    if (unit->health <= 0) {
      stamina->is_running = false;
      continue;
    }

    if (!Game::Units::can_use_run_mode(unit->spawn_type)) {
      stamina->is_running = false;
      stamina->run_requested = false;
      continue;
    }

    bool is_moving = false;
    if (movement != nullptr) {
      float const speed_sq = movement->vx * movement->vx + movement->vz * movement->vz;
      is_moving = speed_sq > 0.01F;
    }

    if (stamina->run_requested && is_moving) {
      if (!stamina->is_running && stamina->can_start_running()) {
        stamina->is_running = true;
      }

      if (stamina->is_running) {
        stamina->stamina = std::max(
            0.0F, stamina->stamina - stamina->depletion_rate * delta_time);

        if (stamina->stamina <= 0.0F) {
          stamina->is_running = false;
        }
      }
    } else {
      stamina->is_running = false;

      stamina->stamina = std::min(
          stamina->max_stamina,
          stamina->stamina + stamina->regen_rate * delta_time);
    }
  }
}

} // namespace Game::Systems
