#include "rpg_damage_resolver.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "../../core/component.h"

namespace Game::Systems::RpgCombat {

namespace {

auto random_float_01() -> float {
  static std::mt19937 rng{std::random_device{}()};
  static std::uniform_real_distribution<float> dist{0.0F, 1.0F};
  return dist(rng);
}

} // namespace

RpgDamageResult resolve_rpg_damage(Engine::Core::World*,
                                   Engine::Core::Entity* target,
                                   int raw_damage,
                                   Engine::Core::EntityID) {
  RpgDamageResult result;
  if (target == nullptr) {
    return result;
  }

  auto* rpg = target->get_component<Engine::Core::RpgHealthComponent>();
  if (rpg == nullptr || !rpg->active || rpg->rpg_hp <= 0) {
    return result;
  }

  if (rpg->dodge_invincible) {
    return result;
  }

  int effective = std::max(1, raw_damage - static_cast<int>(std::roundf(rpg->armor)));

  result.is_crit = (random_float_01() < rpg->crit_chance);
  if (result.is_crit) {
    effective = static_cast<int>(
        std::roundf(static_cast<float>(effective) * rpg->crit_multiplier));
  }

  result.effective_damage = effective;
  rpg->rpg_hp = std::max(0, rpg->rpg_hp - effective);
  result.killed = (rpg->rpg_hp <= 0);

  return result;
}

} // namespace Game::Systems::RpgCombat
