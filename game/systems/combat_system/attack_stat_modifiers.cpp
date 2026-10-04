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

auto wall_walker_of(const Engine::Core::Entity* entity)
    -> const Engine::Core::WallWalkerComponent* {
  auto const* registry = entity != nullptr ? entity->registry() : nullptr;
  return registry != nullptr
             ? registry->try_get<Engine::Core::WallWalkerComponent>(entity->get_id())
             : nullptr;
}

auto standing_height(const Engine::Core::Entity* entity) -> std::optional<float> {
  auto const* registry = entity != nullptr ? entity->registry() : nullptr;
  auto const* transform =
      registry != nullptr
          ? registry->try_get<Engine::Core::TransformComponent>(entity->get_id())
          : nullptr;
  if (transform == nullptr) {
    return std::nullopt;
  }
  return transform->position.y;
}

auto is_high_ground_advantage(const Engine::Core::Entity* high,
                              const Engine::Core::Entity* low) -> bool {
  auto const high_y = standing_height(high);
  auto const low_y = standing_height(low);
  if (!high_y.has_value() || !low_y.has_value()) {
    return false;
  }
  return *high_y - *low_y > Constants::k_high_ground_height_threshold;
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
  range *= Game::Systems::ranged_reach_multiplier(*attacker, unit_comp->spawn_type);
  auto* hold_mode = attacker->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode == nullptr) || !hold_mode->active) {
    return;
  }

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

  if (!is_high_ground_advantage(target, attacker)) {
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
    if (is_high_ground_advantage(attacker, target)) {
      auto const* walker = wall_walker_of(attacker);
      bool const from_wall = walker != nullptr && walker->aloft();
      multiplier *= from_wall ? Constants::k_wall_walk_high_ground_multiplier
                    : archer  ? Constants::k_archer_high_ground_multiplier
                              : Constants::k_spearman_high_ground_multiplier;
    }
  }

  if (is_ranged_mode(attacker->get_component<Engine::Core::AttackComponent>())) {
    if (auto const* cover = target->get_component<Engine::Core::ForestCoverComponent>();
        cover != nullptr && cover->in_forest) {
      multiplier *= Constants::k_forest_ranged_cover_multiplier;
    }
    auto const* target_walker = wall_walker_of(target);
    auto const* attacker_walker = wall_walker_of(attacker);
    if (target_walker != nullptr &&
        target_walker->phase == Engine::Core::WallWalkerComponent::Phase::OnDeck &&
        (attacker_walker == nullptr || !attacker_walker->aloft())) {
      multiplier *= Constants::k_wall_walk_ranged_cover_multiplier;
    }
  }

  return multiplier;
}
} // namespace Game::Systems::Combat
