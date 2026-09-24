#include "rpg_damage_resolver.h"

#include <algorithm>
#include <cmath>

#include "../../core/ambient_session.h"
#include "../../core/component_commander.h"
#include "../../session/deterministic_rng.h"
#include "../combat_system/damage_application.h"

namespace Game::Systems::RpgCombat {

namespace {

auto random_float_01(const Engine::Core::World* world) -> float {
  const Game::Session::AmbientServices* services =
      world != nullptr ? Game::Session::services_for_or_null(*world) : nullptr;
  if (services == nullptr) {
    services = Game::Session::ambient_services_or_null();
  }
  if (services != nullptr && services->rng != nullptr) {
    return services->rng->next_float();
  }
  return 1.0F;
}

} // namespace

RpgDamageResult resolve_rpg_damage(Engine::Core::World* world,
                                   Engine::Core::Entity* target,
                                   int raw_damage,
                                   Engine::Core::EntityID attacker_id,
                                   std::optional<QVector3D> contact_point,
                                   float impact_speed) {
  RpgDamageResult result;
  if (target == nullptr || raw_damage <= 0) {
    return result;
  }

  auto* rpg = target->get_component<Engine::Core::RpgHealthComponent>();
  auto const* unit = target->get_component<Engine::Core::UnitComponent>();
  if (rpg == nullptr || !rpg->active || unit == nullptr || unit->health <= 0) {
    return result;
  }

  float const scaled = static_cast<float>(raw_damage) *
                       std::clamp(rpg->incoming_damage_scale, 0.05F, 4.0F);
  int effective = std::max(1,
                           static_cast<int>(std::lround(scaled)) -
                               static_cast<int>(std::lround(rpg->armor)));

  result.is_crit = (random_float_01(world) < rpg->crit_chance);
  if (result.is_crit) {
    effective = static_cast<int>(
        std::roundf(static_cast<float>(effective) * rpg->crit_multiplier));
  }

  auto const application = Game::Systems::Combat::apply_unit_damage(
      world, target, effective, attacker_id, contact_point, std::nullopt, impact_speed);
  result.effective_damage = application.applied_damage;
  result.killed = application.killed;
  return result;
}

} // namespace Game::Systems::RpgCombat
