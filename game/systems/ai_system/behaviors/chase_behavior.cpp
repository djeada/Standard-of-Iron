#include "chase_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_chase_update_interval = 1.0F;
constexpr int k_army_fraction_divisor = 3;

auto is_chase_eligible(const EntitySnapshot& entity, const AIContext& context) -> bool {
  return is_combat_role_unit(entity) && !entity.is_assault &&
         !is_reserved_unit(entity.id, context) && !is_harass_unit(entity.id, context);
}

} // namespace

auto ChaseBehavior::chase_detachment_size(const AIContext& context) -> int {
  const int configured = context.strategy_config.max_chase_units;
  if (configured <= 0) {
    return 0;
  }

  const int available =
      std::max(0,
               context.total_units - context.builder_count -
                   context.effective_reserve_units - context.assault_unit_count);
  if (available <= 0) {
    return 0;
  }

  const int fair_share = std::max(1, available / k_army_fraction_divisor);
  return std::min(configured, fair_share);
}

void ChaseBehavior::execute(const AISnapshot& snapshot,
                            AIContext& context,
                            float delta_time,
                            std::vector<AICommand>& out_commands) {
  m_chase_timer += delta_time;
  if (m_chase_timer < k_chase_update_interval) {
    return;
  }
  m_chase_timer = 0.0F;

  const int detachment_size = chase_detachment_size(context);
  if (detachment_size <= 0) {
    return;
  }

  const float chase_radius = context.strategy_config.chase_radius;
  const float chase_radius_sq = chase_radius * chase_radius;

  const ContactSnapshot* target = nullptr;
  float best_distance_sq = chase_radius_sq;

  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building) {
      continue;
    }

    for (const auto& entity : snapshot.friendly_units) {
      if (!is_chase_eligible(entity, context)) {
        continue;
      }

      const float distance_sq = distance_squared(
          entity.pos_x, 0.0F, entity.pos_z, enemy.pos_x, 0.0F, enemy.pos_z);
      if (distance_sq <= best_distance_sq) {
        best_distance_sq = distance_sq;
        target = &enemy;
      }
    }
  }

  if (target == nullptr) {
    return;
  }

  std::vector<const EntitySnapshot*> candidates;
  candidates.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!is_chase_eligible(entity, context)) {
      continue;
    }
    candidates.push_back(&entity);
  }

  if (candidates.empty()) {
    return;
  }

  std::sort(candidates.begin(),
            candidates.end(),
            [target](const EntitySnapshot* a, const EntitySnapshot* b) {
              const float da = distance_squared(
                  a->pos_x, 0.0F, a->pos_z, target->pos_x, 0.0F, target->pos_z);
              const float db = distance_squared(
                  b->pos_x, 0.0F, b->pos_z, target->pos_x, 0.0F, target->pos_z);
              return da < db;
            });

  candidates.resize(std::min<std::size_t>(candidates.size(),
                                          static_cast<std::size_t>(detachment_size)));

  std::vector<Engine::Core::EntityID> chaser_ids;
  chaser_ids.reserve(candidates.size());
  for (const auto* unit : candidates) {
    chaser_ids.push_back(unit->id);
  }

  auto claimed = claim_units(
      chaser_ids, get_priority(), "chasing", context, snapshot.game_time, 2.0F);
  if (claimed.empty()) {
    return;
  }

  AICommand attack;
  attack.type = AICommandType::AttackTarget;
  attack.units = std::move(claimed);
  attack.target_id = target->id;
  attack.should_chase = true;
  out_commands.push_back(std::move(attack));
}

auto ChaseBehavior::should_execute(const AISnapshot& snapshot,
                                   const AIContext& context) const -> bool {
  if (context.strategy_config.chase_radius <= 0.0F ||
      context.strategy_config.max_chase_units <= 0) {
    return false;
  }

  if (context.state == AIState::Attacking || context.state == AIState::Retreating) {
    return false;
  }

  if (context.barracks_under_threat || context.any_base_under_threat) {
    return false;
  }

  return !snapshot.visible_enemies.empty();
}

} // namespace Game::Systems::AI
