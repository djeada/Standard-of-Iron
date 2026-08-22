#include "assault_behavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../../core/ownership_constants.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_assault_update_interval = 1.0F;
constexpr float k_engage_radius = 14.0F;
constexpr float k_formation_spread = 2.4F;

constexpr float k_wall_half_extent = 2.5F;
constexpr float k_gate_half_extent = 5.0F;
constexpr float k_barrier_search_radius = 120.0F;

constexpr float k_breach_stand_off = 2.0F;
constexpr float k_breach_spread = 1.6F;
constexpr float k_breach_engage_radius = 4.0F;

constexpr float k_advance_stall_seconds = 8.0F;
constexpr float k_advance_gain_epsilon = 1.0F;
constexpr float k_objective_drift = 6.0F;

auto is_barrier(const ContactSnapshot& contact) -> bool {
  return contact.spawn_type == Game::Units::SpawnType::WallSegment ||
         contact.spawn_type == Game::Units::SpawnType::WallGate;
}

auto barrier_half_extent(const ContactSnapshot& contact) -> float {
  return contact.spawn_type == Game::Units::SpawnType::WallGate ? k_gate_half_extent
                                                                : k_wall_half_extent;
}

auto distance_to_segment(float point_x,
                         float point_z,
                         float from_x,
                         float from_z,
                         float to_x,
                         float to_z) -> float {
  const float span_x = to_x - from_x;
  const float span_z = to_z - from_z;
  const float span_length_sq = (span_x * span_x) + (span_z * span_z);
  float travel = 0.0F;
  if (span_length_sq > 1.0e-4F) {
    travel = (((point_x - from_x) * span_x) + ((point_z - from_z) * span_z)) /
             span_length_sq;
    travel = std::clamp(travel, 0.0F, 1.0F);
  }
  const float nearest_x = from_x + (span_x * travel);
  const float nearest_z = from_z + (span_z * travel);
  return std::sqrt(
      distance_squared(point_x, 0.0F, point_z, nearest_x, 0.0F, nearest_z));
}

auto select_breach_target(const AISnapshot& snapshot,
                          float reference_x,
                          float reference_z,
                          float objective_x,
                          float objective_z) -> const ContactSnapshot* {
  const ContactSnapshot* best = nullptr;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  auto consider = [&](const ContactSnapshot& candidate) {
    if (!is_barrier(candidate) || candidate.health <= 0) {
      return;
    }
    const float distance_sq = distance_squared(
        candidate.pos_x, 0.0F, candidate.pos_z, reference_x, 0.0F, reference_z);
    if (distance_sq > k_barrier_search_radius * k_barrier_search_radius) {
      return;
    }
    if (distance_to_segment(candidate.pos_x,
                            candidate.pos_z,
                            reference_x,
                            reference_z,
                            objective_x,
                            objective_z) > barrier_half_extent(candidate)) {
      return;
    }

    const bool candidate_is_gate =
        candidate.spawn_type == Game::Units::SpawnType::WallGate;
    const bool best_is_gate =
        best != nullptr && best->spawn_type == Game::Units::SpawnType::WallGate;
    if (best == nullptr || (candidate_is_gate && !best_is_gate) ||
        (candidate_is_gate == best_is_gate && distance_sq < best_distance_sq)) {
      best = &candidate;
      best_distance_sq = distance_sq;
    }
  };

  for (const auto& candidate : snapshot.visible_enemies) {
    consider(candidate);
  }
  for (const auto& candidate : snapshot.strategic_objectives) {
    consider(candidate);
  }

  return best;
}

auto is_wave_quarry(const ContactSnapshot& contact) -> bool {
  return !Game::Core::is_neutral_owner(contact.owner_id) && contact.health > 0;
}

auto select_assault_target(const AISnapshot& snapshot,
                           float reference_x,
                           float reference_z,
                           bool skip_barriers) -> const ContactSnapshot* {
  const ContactSnapshot* best = nullptr;
  float best_score = std::numeric_limits<float>::infinity();

  const bool troops_in_sight =
      std::any_of(snapshot.visible_enemies.begin(),
                  snapshot.visible_enemies.end(),
                  [](const ContactSnapshot& contact) {
                    return !contact.is_building && is_wave_quarry(contact);
                  });

  for (const auto& candidate : snapshot.visible_enemies) {
    if (!is_wave_quarry(candidate)) {
      continue;
    }
    if (troops_in_sight && candidate.is_building) {
      continue;
    }
    if (skip_barriers && is_barrier(candidate)) {
      continue;
    }
    const float score = distance_squared(
        candidate.pos_x, 0.0F, candidate.pos_z, reference_x, 0.0F, reference_z);
    if (score < best_score) {
      best_score = score;
      best = &candidate;
    }
  }

  if (best != nullptr) {
    return best;
  }

  for (const auto& objective : snapshot.strategic_objectives) {
    if (!is_wave_quarry(objective)) {
      continue;
    }
    if (skip_barriers && is_barrier(objective)) {
      continue;
    }
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

auto AssaultBehavior::advance_is_stalled(Engine::Core::EntityID unit_id,
                                         float objective_x,
                                         float objective_z,
                                         float distance_to_objective,
                                         float game_time) -> bool {
  auto [entry, inserted] = m_advance_progress.try_emplace(
      unit_id,
      AdvanceProgress{
          distance_to_objective, game_time, game_time, objective_x, objective_z});
  if (inserted) {
    return false;
  }

  auto& progress = entry->second;
  progress.last_observed_time = game_time;
  if (distance_squared(objective_x,
                       0.0F,
                       objective_z,
                       progress.objective_x,
                       0.0F,
                       progress.objective_z) > k_objective_drift * k_objective_drift) {
    progress = {distance_to_objective, game_time, game_time, objective_x, objective_z};
    return false;
  }
  if (distance_to_objective < progress.best_distance - k_advance_gain_epsilon) {
    progress.best_distance = distance_to_objective;
    progress.last_gain_time = game_time;
    return false;
  }

  return (game_time - progress.last_gain_time) >= k_advance_stall_seconds;
}

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

  const auto* objective = select_assault_target(snapshot, center_x, center_z, true);
  const auto marching =
      std::find_if(assault_units.begin(),
                   assault_units.end(),
                   [](const EntitySnapshot* unit) { return unit->has_march_target; });

  float objective_x = 0.0F;
  float objective_z = 0.0F;
  if (objective != nullptr) {
    objective_x = objective->pos_x;
    objective_z = objective->pos_z;
  } else {
    if (marching != assault_units.end()) {
      objective_x = (*marching)->march_target_x;
      objective_z = (*marching)->march_target_z;
    } else {
      objective = select_assault_target(snapshot, center_x, center_z, false);
      if (objective == nullptr) {
        return;
      }
      objective_x = objective->pos_x;
      objective_z = objective->pos_z;
    }
  }

  const float advance_goal_x =
      marching != assault_units.end() ? (*marching)->march_target_x : objective_x;
  const float advance_goal_z =
      marching != assault_units.end() ? (*marching)->march_target_z : objective_z;

  bool advance_stalled = false;
  for (const auto* unit : assault_units) {
    const float distance_to_objective = std::sqrt(distance_squared(
        unit->pos_x, 0.0F, unit->pos_z, advance_goal_x, 0.0F, advance_goal_z));
    if (advance_is_stalled(unit->id,
                           advance_goal_x,
                           advance_goal_z,
                           distance_to_objective,
                           snapshot.game_time)) {
      advance_stalled = true;
    }
  }
  std::erase_if(m_advance_progress, [&snapshot](const auto& entry) {
    return snapshot.game_time - entry.second.last_observed_time > 30.0F;
  });

  const ContactSnapshot* target = objective;
  const auto* lane_barrier = select_breach_target(
      snapshot, center_x, center_z, advance_goal_x, advance_goal_z);
  const bool hostile_gate_ahead =
      lane_barrier != nullptr &&
      lane_barrier->spawn_type == Game::Units::SpawnType::WallGate;
  const auto* breach = hostile_gate_ahead || advance_stalled ? lane_barrier : nullptr;
  if (breach != nullptr) {
    target = breach;
  }

  std::vector<Engine::Core::EntityID> engage_ids;
  std::vector<Engine::Core::EntityID> advance_ids;
  std::vector<float> advance_x;
  std::vector<float> advance_y;
  std::vector<float> advance_z;

  const bool breaching = breach != nullptr;
  const float attack_point_x = target != nullptr ? target->pos_x : objective_x;
  const float attack_point_z = target != nullptr ? target->pos_z : objective_z;

  const bool has_authored_march = marching != assault_units.end();
  float approach_x = has_authored_march ? advance_goal_x : attack_point_x;
  float approach_z = has_authored_march ? advance_goal_z : attack_point_z;
  float lateral_x = 0.0F;
  float lateral_z = 0.0F;
  if (breaching) {
    float outward_x = center_x - advance_goal_x;
    float outward_z = center_z - advance_goal_z;
    const float span = std::sqrt((outward_x * outward_x) + (outward_z * outward_z));
    if (span > 1.0e-3F) {
      outward_x /= span;
      outward_z /= span;
    } else {
      outward_x = 1.0F;
      outward_z = 0.0F;
    }
    approach_x = attack_point_x + (outward_x * k_breach_stand_off);
    approach_z = attack_point_z + (outward_z * k_breach_stand_off);
    lateral_x = -outward_z;
    lateral_z = outward_x;
  }

  const float engage_radius = breaching ? k_breach_engage_radius : k_engage_radius;
  const float engage_radius_sq = engage_radius * engage_radius;

  for (std::size_t index = 0; index < assault_units.size(); ++index) {
    const auto* unit = assault_units[index];
    const float distance_to_target_sq = distance_squared(
        unit->pos_x, 0.0F, unit->pos_z, attack_point_x, 0.0F, attack_point_z);

    if (target != nullptr &&
        (distance_to_target_sq <= engage_radius_sq || hostile_gate_ahead)) {
      engage_ids.push_back(unit->id);
      continue;
    }

    if (has_authored_march && unit->movement.has_target && !breaching) {
      continue;
    }

    advance_ids.push_back(unit->id);
    if (breaching) {
      const float offset = (static_cast<float>(index % 4) - 1.5F) * k_breach_spread;
      advance_x.push_back(approach_x + (lateral_x * offset));
      advance_y.push_back(0.0F);
      advance_z.push_back(approach_z + (lateral_z * offset));
      continue;
    }

    const float angle = static_cast<float>(index) * 0.9F;
    const float ring = k_formation_spread * static_cast<float>(1 + (index % 3));
    advance_x.push_back(approach_x + std::cos(angle) * ring);
    advance_y.push_back(0.0F);
    advance_z.push_back(approach_z + std::sin(angle) * ring);
  }

  if (!engage_ids.empty()) {
    auto claimed = claim_units(
        engage_ids, get_priority(), "assaulting", context, snapshot.game_time, 1.0F);
    if (!claimed.empty()) {
      AICommand attack;
      attack.type = AICommandType::AttackTarget;
      attack.units = std::move(claimed);
      attack.target_id = target->id;

      attack.should_chase = true;
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
  if (!snapshot.visible_enemies.empty() || !snapshot.strategic_objectives.empty()) {
    return true;
  }

  return std::any_of(snapshot.friendly_units.begin(),
                     snapshot.friendly_units.end(),
                     [](const EntitySnapshot& entity) {
                       return entity.is_assault && entity.has_march_target;
                     });
}

} // namespace Game::Systems::AI
