#include "assault_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_assault_update_interval = 1.0F;
constexpr float k_engage_radius = 14.0F;
constexpr float k_formation_spread = 2.4F;

auto select_assault_target(const AISnapshot& snapshot,
                           float reference_x,
                           float reference_z) -> const ContactSnapshot* {
  const ContactSnapshot* best = nullptr;
  float best_score = std::numeric_limits<float>::infinity();

  for (const auto& candidate : snapshot.visible_enemies) {
    float score = distance_squared(
        candidate.pos_x, 0.0F, candidate.pos_z, reference_x, 0.0F, reference_z);
    if (candidate.is_building) {
      score += 400.0F;
    }
    if (score < best_score) {
      best_score = score;
      best = &candidate;
    }
  }

  if (best != nullptr) {
    return best;
  }

  for (const auto& objective : snapshot.strategic_objectives) {
    const float score = distance_squared(
        objective.pos_x, 0.0F, objective.pos_z, reference_x, 0.0F, reference_z);
    if (score < best_score) {
      best_score = score;
      best = &objective;
    }
  }

  return best;
}

} // namespace

void AssaultBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  m_assault_timer += delta_time;
  if (m_assault_timer < k_assault_update_interval) {
    return;
  }
  m_assault_timer = 0.0F;

  std::vector<const EntitySnapshot*> assault_units;
  assault_units.reserve(context.assault_unit_ids.size());
  float center_x = 0.0F;
  float center_z = 0.0F;

  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_assault || !is_combat_role_unit(entity)) {
      continue;
    }
    assault_units.push_back(&entity);
    center_x += entity.pos_x;
    center_z += entity.pos_z;
  }

  if (assault_units.empty()) {
    return;
  }

  const float scale = 1.0F / static_cast<float>(assault_units.size());
  center_x *= scale;
  center_z *= scale;

  const auto* target = select_assault_target(snapshot, center_x, center_z);
  if (target == nullptr) {
    return;
  }

  std::vector<Engine::Core::EntityID> engage_ids;
  std::vector<Engine::Core::EntityID> advance_ids;
  std::vector<float> advance_x;
  std::vector<float> advance_y;
  std::vector<float> advance_z;

  const float engage_radius_sq = k_engage_radius * k_engage_radius;

  for (std::size_t index = 0; index < assault_units.size(); ++index) {
    const auto* unit = assault_units[index];
    const float distance_to_target_sq = distance_squared(
        unit->pos_x, 0.0F, unit->pos_z, target->pos_x, 0.0F, target->pos_z);

    if (distance_to_target_sq <= engage_radius_sq) {
      engage_ids.push_back(unit->id);
      continue;
    }

    const float angle = static_cast<float>(index) * 0.9F;
    const float ring = k_formation_spread * static_cast<float>(1 + (index % 3));
    advance_ids.push_back(unit->id);
    advance_x.push_back(target->pos_x + std::cos(angle) * ring);
    advance_y.push_back(0.0F);
    advance_z.push_back(target->pos_z + std::sin(angle) * ring);
  }

  if (!engage_ids.empty()) {
    auto claimed = claim_units(
        engage_ids, get_priority(), "assaulting", context, snapshot.game_time, 1.0F);
    if (!claimed.empty()) {
      AICommand attack;
      attack.type = AICommandType::AttackTarget;
      attack.units = std::move(claimed);
      attack.target_id = target->id;
      attack.should_chase = !target->is_building;
      out_commands.push_back(std::move(attack));
    }
  }

  if (advance_ids.empty()) {
    return;
  }

  auto claimed_advance = claim_units(
      advance_ids, get_priority(), "assaulting", context, snapshot.game_time, 1.0F);
  if (claimed_advance.empty()) {
    return;
  }

  const std::unordered_set<Engine::Core::EntityID> claimed_set(claimed_advance.begin(),
                                                               claimed_advance.end());
  std::vector<float> filtered_x;
  std::vector<float> filtered_y;
  std::vector<float> filtered_z;
  for (std::size_t i = 0; i < advance_ids.size(); ++i) {
    if (claimed_set.contains(advance_ids[i])) {
      filtered_x.push_back(advance_x[i]);
      filtered_y.push_back(advance_y[i]);
      filtered_z.push_back(advance_z[i]);
    }
  }

  AICommand move;
  move.type = AICommandType::MoveUnits;
  move.units = std::move(claimed_advance);
  move.move_target_x = std::move(filtered_x);
  move.move_target_y = std::move(filtered_y);
  move.move_target_z = std::move(filtered_z);
  out_commands.push_back(std::move(move));
}

auto AssaultBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  if (context.assault_unit_count <= 0) {
    return false;
  }
  return !snapshot.visible_enemies.empty() || !snapshot.strategic_objectives.empty();
}

} // namespace Game::Systems::AI
