#include "app/commander/commander_primary_scan.h"

#include <algorithm>

#include "app/commander/commander_targeting.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/combat_system/target_rules.h"

namespace App::Core {

namespace {

constexpr float k_commander_attack_cone_dot = 0.45F;

auto buildings_of(const Engine::Core::World& world)
    -> const Game::Systems::BuildingCollisionRegistry& {
  return Game::Session::session_for(world).building_collision();
}

} // namespace

auto PrimaryScan::eligible_samples(Engine::Core::EntityID entity_id) const
    -> std::vector<Game::Systems::RpgCombat::SoldierTarget> {
  auto& owners = Game::Session::session_for(world).owners();
  auto* entity = world.get_entity(entity_id);
  auto* unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
  if (entity == nullptr || unit == nullptr ||
      Game::Systems::Combat::evaluate_target(
          owners,
          local_owner_id,
          entity,
          {.intent = Game::Systems::Combat::EngagementIntent::AutoAcquired,
           .allow_buildings = false}) != Game::Systems::Combat::TargetRefusal::None) {
    return {};
  }
  return Game::Systems::RpgCombat::live_soldier_targets(*entity);
}

auto PrimaryScan::choose_sample(Engine::Core::EntityID entity_id,
                                std::uint16_t preferred_slot,
                                float minimum_facing) const
    -> std::optional<Game::Systems::RpgCombat::SoldierTarget> {
  auto samples = eligible_samples(entity_id);
  std::optional<Game::Systems::RpgCombat::SoldierTarget> best;
  float best_score = -1000000.0F;
  for (auto const& sample : samples) {
    QVector3D to_target = sample.position - origin;
    to_target.setY(0.0F);
    float const distance = to_target.length();
    if (distance <= 0.0001F ||
        distance > max_range + std::max(0.0F, sample.body_radius) ||
        !Game::Systems::has_clear_building_los(
            buildings_of(world), origin, sample.position)) {
      continue;
    }
    to_target /= distance;
    float const facing = QVector3D::dotProduct(forward, to_target);
    if (facing < minimum_facing) {
      continue;
    }
    float score = (facing * 10.0F) - distance;
    if (sample.soldier_slot == preferred_slot) {
      score += 3.0F;
    }
    if (!best.has_value() || score > best_score) {
      best = sample;
      best_score = score;
    }
  }
  return best;
}

auto PrimaryScan::best_in_world() const
    -> std::optional<Game::Systems::RpgCombat::SoldierTarget> {
  std::optional<Game::Systems::RpgCombat::SoldierTarget> best;
  float best_score = -1000000.0F;
  for (auto* candidate : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate == commander) {
      continue;
    }
    auto target = choose_sample(candidate->get_id(),
                                CommanderTargeting::k_no_slot,
                                k_commander_attack_cone_dot);
    if (!target.has_value()) {
      continue;
    }
    QVector3D to_target = target->position - origin;
    to_target.setY(0.0F);
    float const distance = to_target.length();
    to_target /= std::max(distance, 0.0001F);
    float const score = (QVector3D::dotProduct(forward, to_target) * 10.0F) - distance;
    if (score > best_score) {
      best_score = score;
      best = *target;
    }
  }
  return best;
}

auto commander_reach(const Engine::Core::Entity& commander,
                     const Engine::Core::AttackComponent* attack) -> float {
  float max_range = 2.05F;
  if (attack == nullptr) {
    return max_range;
  }
  auto const* commander_unit = commander.get_component<Engine::Core::UnitComponent>();
  auto const family = commander_unit != nullptr
                          ? Engine::Core::resolve_combat_attack_family(
                                commander_unit->spawn_type,
                                Engine::Core::AttackComponent::CombatMode::Melee)
                          : Engine::Core::CombatAttackFamily::Sword;
  if (family == Engine::Core::CombatAttackFamily::Spear) {
    return 2.75F;
  }
  if (family == Engine::Core::CombatAttackFamily::Bow && attack->can_ranged) {
    return attack->range;
  }
  return std::max(max_range, attack->melee_range);
}

auto pick_primary(const PrimaryScan& scan,
                  Engine::Core::Entity& commander,
                  Engine::Core::EntityID locked_id,
                  std::uint16_t& locked_slot,
                  Engine::Core::EntityID soft_id,
                  std::uint16_t soft_slot)
    -> std::optional<Game::Systems::RpgCombat::SoldierTarget> {
  if (locked_id != 0) {
    auto target = scan.choose_sample(locked_id, locked_slot, -0.05F);
    if (target.has_value()) {
      locked_slot = target->soldier_slot;
    }
    return target;
  }

  if (soft_id != 0) {
    if (auto target = scan.choose_sample(soft_id, soft_slot, 0.15F);
        target.has_value()) {
      return target;
    }
  }

  if (auto* engagement =
          commander.get_component<Engine::Core::RpgEngagementComponent>()) {
    for (auto const& slot : engagement->engagement_slots) {
      if (!slot.pressing) {
        continue;
      }
      if (auto target =
              scan.choose_sample(slot.entity_id, CommanderTargeting::k_no_slot, 0.05F);
          target.has_value()) {
        return target;
      }
    }
  }

  return scan.best_in_world();
}

} // namespace App::Core
