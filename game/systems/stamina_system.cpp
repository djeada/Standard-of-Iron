#include "stamina_system.h"

#include "../core/component.h"
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
} // namespace

auto ensure_run_stamina(Engine::Core::Entity& entity)
    -> Engine::Core::StaminaComponent* {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || !Game::Units::can_use_run_mode(unit->spawn_type)) {
    return nullptr;
  }
  if (auto* existing = entity.get_component<Engine::Core::StaminaComponent>()) {
    return existing;
  }

  auto* stamina = entity.add_component<Engine::Core::StaminaComponent>();
  const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
  if (stamina != nullptr && troop_type.has_value()) {
    const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
        unit->nation_id, *troop_type);
    stamina->initialize_from_stats(profile.combat.max_stamina,
                                   profile.combat.stamina_regen_rate,
                                   profile.combat.stamina_depletion_rate);
  }
  return stamina;
}

void StaminaSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto* entity : world->get_entities_with<Engine::Core::StaminaComponent>()) {
    auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
    if (stamina == nullptr) {
      continue;
    }

    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      stamina->is_running = false;
      continue;
    }

    if (!Game::Units::can_use_run_mode(unit->spawn_type)) {
      stamina->is_running = false;
      stamina->run_requested = false;
      continue;
    }

    const auto* motion =
        entity->get_component<Engine::Core::MotionPresentationComponent>();
    const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    const bool is_moving = has_locomotion_intent(motion, movement);

    if (stamina->run_requested && is_moving) {
      if (!stamina->is_running && stamina->can_start_running()) {
        stamina->is_running = true;
      }
      if (stamina->is_running) {
        stamina->deplete(delta_time);
        if (!stamina->has_stamina()) {
          stamina->is_running = false;
        }
      } else {

        stamina->regenerate(delta_time);
      }
    } else {
      stamina->is_running = false;
      stamina->regenerate(delta_time);
    }
  }
}

} // namespace Game::Systems
