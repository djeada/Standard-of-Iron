#include "attack_stat_modifiers.h"

#include <algorithm>
#include <optional>

#include "../../core/component.h"
#include "../../units/spawn_type.h"
#include "../../units/troop_config.h"
#include "../attack_range.h"
#include "../combat_rules.h"
#include "../troop_profile_service.h"
#include "combat_types.h"
#include "combat_utils.h"

namespace Game::Systems::Combat {

namespace {

auto get_base_max_health(const Engine::Core::UnitComponent* unit)
    -> std::optional<int> {
  if (unit == nullptr) {
    return std::nullopt;
  }
  auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit->spawn_type);
  if (!troop_type_opt) {
    return std::nullopt;
  }
  auto const& profile = Game::Systems::TroopProfileService::instance().get_profile_ref(
      unit->nation_id, *troop_type_opt);
  return profile.combat.max_health;
}

auto is_high_ground_advantage(const Engine::Core::TransformComponent* high_transform,
                              const Engine::Core::TransformComponent* low_transform)
    -> bool {
  if ((high_transform == nullptr) || (low_transform == nullptr)) {
    return false;
  }
  float const height_diff = high_transform->position.y - low_transform->position.y;
  return height_diff > Constants::k_high_ground_height_threshold;
}

void raise_max_health_keeping_ratio(Engine::Core::UnitComponent& unit,
                                    int max_health_bonus) {
  if (unit.max_health >= max_health_bonus) {
    return;
  }
  int const safe_max_health = std::max(1, unit.max_health);
  int const health_percentage = (unit.health * 100) / safe_max_health;
  unit.max_health = max_health_bonus;
  unit.health = (max_health_bonus * health_percentage) / 100;
}

void apply_health_bonus(Engine::Core::UnitComponent* unit_comp) {
  auto base_max_health_opt = get_base_max_health(unit_comp);
  int const base_max_health =
      base_max_health_opt.value_or(std::max(1, unit_comp->max_health));
  raise_max_health_keeping_ratio(*unit_comp,
                                 static_cast<int>(static_cast<float>(base_max_health) *
                                                  Constants::k_health_multiplier_hold));
}

} // namespace

void apply_hold_mode_bonuses(Engine::Core::Entity* attacker,
                             Engine::Core::UnitComponent* unit_comp,
                             float& range,
                             int& damage) {
  auto* hold_mode = attacker->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode == nullptr) || !hold_mode->active) {
    return;
  }

  range *= Game::Systems::hold_mode_range_multiplier(*attacker, unit_comp->spawn_type);

  if (unit_comp->spawn_type == Game::Units::SpawnType::Archer) {
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::k_damage_multiplier_archer_hold);
    apply_health_bonus(unit_comp);
  } else if (unit_comp->spawn_type == Game::Units::SpawnType::Spearman) {
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::k_damage_multiplier_spearman_hold);
    apply_health_bonus(unit_comp);
  } else {
    damage = static_cast<int>(static_cast<float>(damage) *
                              Constants::k_damage_multiplier_default_hold);
  }
}

void apply_high_ground_defense_bonuses(Engine::Core::Entity* attacker,
                                       Engine::Core::Entity* target,
                                       Engine::Core::UnitComponent* target_unit,
                                       int& damage) {
  if (target_unit == nullptr) {
    return;
  }

  if (target_unit->spawn_type != Game::Units::SpawnType::Archer &&
      target_unit->spawn_type != Game::Units::SpawnType::Spearman) {
    return;
  }

  auto* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
  if (!is_high_ground_advantage(target_transform, attacker_transform)) {
    return;
  }

  damage = std::max(1,
                    static_cast<int>(static_cast<float>(damage) *
                                     Constants::k_high_ground_armor_multiplier));

  auto base_max_health_opt = get_base_max_health(target_unit);
  if (!base_max_health_opt || *base_max_health_opt <= 0) {
    return;
  }

  raise_max_health_keeping_ratio(
      *target_unit,
      static_cast<int>(static_cast<float>(*base_max_health_opt) *
                       Constants::k_high_ground_health_multiplier));
}

auto calculate_tactical_damage_multiplier(Engine::Core::Entity* attacker,
                                          Engine::Core::Entity* target,
                                          Engine::Core::UnitComponent* attacker_unit,
                                          Engine::Core::UnitComponent* target_unit)
    -> float {
  using Game::Units::SpawnType;
  float multiplier = 1.0F;

  auto const attacker_type = attacker_unit->spawn_type;
  auto const target_type = target_unit->spawn_type;
  if (is_melee_mode(attacker->get_component<Engine::Core::AttackComponent>()) &&
      is_infantry_spawn(attacker_type) && attacker_type != SpawnType::Barracks &&
      Game::Units::is_siege_engine_spawn(target_type)) {
    multiplier *= Constants::k_infantry_melee_vs_siege_multiplier;
  }

  if (attacker_type == SpawnType::Spearman && Game::Units::is_cavalry(target_type)) {
    multiplier *= Constants::k_spearman_vs_cavalry_multiplier;
  }

  bool const archer =
      attacker_type == SpawnType::Archer || attacker_type == SpawnType::HorseArcher;
  if (archer && target->has_component<Engine::Core::ElephantComponent>()) {
    multiplier *= Constants::k_archer_vs_elephant_multiplier;
  }

  if (archer || attacker_type == SpawnType::Spearman) {
    auto const* attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    auto const* target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    if (is_high_ground_advantage(attacker_transform, target_transform)) {
      multiplier *= archer ? Constants::k_archer_high_ground_multiplier
                           : Constants::k_spearman_high_ground_multiplier;
    }
  }

  if (is_ranged_mode(attacker->get_component<Engine::Core::AttackComponent>())) {
    if (auto const* cover = target->get_component<Engine::Core::ForestCoverComponent>();
        cover != nullptr && cover->in_forest) {
      multiplier *= Constants::k_forest_ranged_cover_multiplier;
    }
  }

  return multiplier;
}
} // namespace Game::Systems::Combat
