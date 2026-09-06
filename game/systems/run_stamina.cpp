#include "run_stamina.h"

#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "troop_profile_service.h"

namespace Game::Systems {

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

} // namespace Game::Systems
