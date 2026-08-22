#include "stamina_system.h"

#include <algorithm>

#include "../core/component.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "troop_profile_service.h"

namespace Game::Systems {

namespace {

[[nodiscard]] auto has_locomotion_intent(
    const Engine::Core::MotionPresentationComponent* motion,
    const Engine::Core::MovementComponent* movement) noexcept -> bool {
  if (motion != nullptr && motion->initialized) {
    return motion->has_locomotion();
  }
  if (motion != nullptr && motion->has_locomotion()) {
    return true;
  }
  return movement != nullptr &&
         ((movement->get_vx() * movement->get_vx() +
           movement->get_vz() * movement->get_vz()) > 1.0e-5F ||
          movement->get_has_target() || movement->has_waypoints());
}
void deplete_stamina(Engine::Core::StaminaComponent& stamina,
                     float delta_time) noexcept {
  stamina.stamina =
      std::max(0.0F, stamina.stamina - stamina.depletion_rate * delta_time);
}

void regenerate_stamina(Engine::Core::StaminaComponent& stamina,
                        float delta_time) noexcept {
  stamina.stamina =
      std::min(stamina.max_stamina, stamina.stamina + stamina.regen_rate * delta_time);
}

} // namespace

void StaminaSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();

  for (auto [entity, stamina_ref, unit_ref] :
       context.entity_view<Engine::Core::StaminaComponent,
                           Engine::Core::UnitComponent>()) {
    auto* stamina = &stamina_ref;
    const auto* unit = &unit_ref;

    if (unit->health <= 0) {
      stamina->is_running = false;
      continue;
    }

    if (!Game::Units::can_use_run_mode(unit->spawn_type)) {
      stamina->is_running = false;
      stamina->run_requested = false;
      continue;
    }

    const auto* motion =
        entity.get_component<Engine::Core::MotionPresentationComponent>();
    const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
    const bool is_moving = has_locomotion_intent(motion, movement);

    if (stamina->run_requested && is_moving) {
      if (!stamina->is_running && stamina->can_start_running()) {
        stamina->is_running = true;
      }
      if (stamina->is_running) {
        deplete_stamina(*stamina, delta_time);
        if (!stamina->has_stamina()) {
          stamina->is_running = false;
        }
      } else {

        regenerate_stamina(*stamina, delta_time);
      }
    } else {
      stamina->is_running = false;
      regenerate_stamina(*stamina, delta_time);
    }
  }
}

auto StaminaSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent, MotionPresentationComponent, MovementComponent>{},
      Writes<StaminaComponent>{});
}

} // namespace Game::Systems
