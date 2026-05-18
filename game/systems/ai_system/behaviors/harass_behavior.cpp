#include "harass_behavior.h"

#include <QVector3D>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

auto get_formation_type_for_player(int player_id) -> FormationType {
  const Nation* nation = NationRegistry::instance().get_nation_for_player(player_id);
  if (nation != nullptr) {
    return nation->formation_type;
  }
  return FormationType::Roman;
}

auto select_visible_harass_target(const AISnapshot& snapshot,
                                  float reference_x,
                                  float reference_y,
                                  float reference_z,
                                  float harassment_range) -> const ContactSnapshot* {
  const ContactSnapshot* best_target = nullptr;
  float best_score = std::numeric_limits<float>::infinity();

  for (const auto& candidate : snapshot.visible_enemies) {
    const float dist_sq = distance_squared(candidate.pos_x,
                                           candidate.pos_y,
                                           candidate.pos_z,
                                           reference_x,
                                           reference_y,
                                           reference_z);
    if (harassment_range > 0.0F && dist_sq > harassment_range * harassment_range) {
      continue;
    }

    float score = dist_sq;
    if (!candidate.is_building) {
      score += 120.0F;
    }
    if (candidate.spawn_type == Game::Units::SpawnType::DefenseTower) {
      score += 800.0F;
    }
    if (candidate.spawn_type == Game::Units::SpawnType::Barracks) {
      score += 250.0F;
    }

    int nearby_support = 0;
    for (const auto& other : snapshot.visible_enemies) {
      if (other.id == candidate.id) {
        continue;
      }
      const float support_dist_sq = distance_squared(
          other.pos_x, other.pos_y, other.pos_z, candidate.pos_x, candidate.pos_y, candidate.pos_z);
      if (support_dist_sq <= 12.0F * 12.0F) {
        nearby_support++;
      }
    }
    score += static_cast<float>(nearby_support) * 180.0F;

    if (score < best_score) {
      best_score = score;
      best_target = &candidate;
    }
  }

  return best_target;
}

auto select_harass_objective(const AISnapshot& snapshot,
                             float reference_x,
                             float reference_z) -> const ContactSnapshot* {
  const ContactSnapshot* best_objective = nullptr;
  float best_score = std::numeric_limits<float>::infinity();

  for (const auto& objective : snapshot.strategic_objectives) {
    const float dx = objective.pos_x - reference_x;
    const float dz = objective.pos_z - reference_z;
    float score = dx * dx + dz * dz;
    if (!objective.is_building) {
      score += 600.0F;
    }

    if (score < best_score) {
      best_score = score;
      best_objective = &objective;
    }
  }

  return best_objective;
}

} // namespace

void HarassBehavior::execute(const AISnapshot& snapshot,
                             AIContext& context,
                             float delta_time,
                             std::vector<AICommand>& out_commands) {
  m_harass_timer += delta_time;
  if (m_harass_timer < 2.0F) {
    return;
  }
  m_harass_timer = 0.0F;

  auto harass_units = collect_harass_force_units(snapshot, context);
  if (harass_units.empty()) {
    return;
  }

  float group_center_x = 0.0F;
  float group_center_y = 0.0F;
  float group_center_z = 0.0F;
  for (const auto* entity : harass_units) {
    group_center_x += entity->pos_x;
    group_center_y += entity->pos_y;
    group_center_z += entity->pos_z;
  }
  const float inv_count = 1.0F / static_cast<float>(harass_units.size());
  group_center_x *= inv_count;
  group_center_y *= inv_count;
  group_center_z *= inv_count;

  std::vector<Engine::Core::EntityID> harass_unit_ids;
  harass_unit_ids.reserve(harass_units.size());
  for (const auto* unit : harass_units) {
    harass_unit_ids.push_back(unit->id);
  }

  auto claimed_units = claim_units(harass_unit_ids,
                                   get_priority(),
                                   "harassing",
                                   context,
                                   snapshot.game_time,
                                   1.5F);
  if (claimed_units.empty()) {
    return;
  }

  const float harassment_range = std::max(
      context.strategy_config.harassment_range, context.strategy_config.scouting_distance * 0.75F);
  const ContactSnapshot* visible_target = select_visible_harass_target(
      snapshot, group_center_x, group_center_y, group_center_z, harassment_range);

  if (visible_target != nullptr) {
    if (visible_target->is_building) {
      FormationType formation_type = get_formation_type_for_player(context.player_id);
      QVector3D const attack_center(visible_target->pos_x, 0.0F, visible_target->pos_z);
      auto formation_positions = FormationSystem::instance().get_formation_positions(
          formation_type,
          static_cast<int>(claimed_units.size()),
          attack_center,
          std::max(1.5F, context.strategy_config.attack_formation_spacing * 0.8F));

      std::vector<float> target_x;
      std::vector<float> target_y;
      std::vector<float> target_z;
      target_x.reserve(claimed_units.size());
      target_y.reserve(claimed_units.size());
      target_z.reserve(claimed_units.size());
      for (size_t i = 0; i < claimed_units.size(); ++i) {
        target_x.push_back(formation_positions[i].x());
        target_y.push_back(formation_positions[i].y());
        target_z.push_back(formation_positions[i].z());
      }

      AICommand move_command;
      move_command.type = AICommandType::MoveUnits;
      move_command.units = claimed_units;
      move_command.move_target_x = std::move(target_x);
      move_command.move_target_y = std::move(target_y);
      move_command.move_target_z = std::move(target_z);
      out_commands.push_back(std::move(move_command));
    }

    AICommand attack_command;
    attack_command.type = AICommandType::AttackTarget;
    attack_command.units = std::move(claimed_units);
    attack_command.target_id = visible_target->id;
    attack_command.should_chase = !visible_target->is_building;
    out_commands.push_back(std::move(attack_command));
    m_last_target = visible_target->id;
    return;
  }

  const ContactSnapshot* objective =
      select_harass_objective(snapshot, group_center_x, group_center_z);
  if (objective == nullptr) {
    release_units(claimed_units, context);
    m_last_target = 0;
    return;
  }

  FormationType formation_type = get_formation_type_for_player(context.player_id);
  const float dx = objective->pos_x - group_center_x;
  const float dz = objective->pos_z - group_center_z;
  const float dist = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
  const float standoff = 8.0F;
  float target_center_x = objective->pos_x;
  float target_center_z = objective->pos_z;
  if (dist > standoff) {
    target_center_x -= (dx / dist) * standoff;
    target_center_z -= (dz / dist) * standoff;
  }

  QVector3D const move_center(target_center_x, 0.0F, target_center_z);
  auto formation_positions = FormationSystem::instance().get_formation_positions(
      formation_type,
      static_cast<int>(claimed_units.size()),
      move_center,
      std::max(1.4F, context.strategy_config.gather_spacing));

  std::vector<float> target_x;
  std::vector<float> target_y;
  std::vector<float> target_z;
  target_x.reserve(claimed_units.size());
  target_y.reserve(claimed_units.size());
  target_z.reserve(claimed_units.size());
  for (size_t i = 0; i < claimed_units.size(); ++i) {
    target_x.push_back(formation_positions[i].x());
    target_y.push_back(formation_positions[i].y());
    target_z.push_back(formation_positions[i].z());
  }

  AICommand move_command;
  move_command.type = AICommandType::MoveUnits;
  move_command.units = std::move(claimed_units);
  move_command.move_target_x = std::move(target_x);
  move_command.move_target_y = std::move(target_y);
  move_command.move_target_z = std::move(target_z);
  out_commands.push_back(std::move(move_command));
  m_last_target = objective->id;
}

auto HarassBehavior::should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool {
  if (context.state == AIState::Retreating || context.state == AIState::Defending) {
    return false;
  }
  if (context.barracks_under_threat || !context.buildings_under_attack.empty()) {
    return false;
  }
  if (context.average_health < context.strategy_config.retreat_threshold) {
    return false;
  }
  if (context.effective_harass_units <= 0) {
    return false;
  }
  if (snapshot.visible_enemies.empty() && snapshot.strategic_objectives.empty()) {
    return false;
  }

  return !collect_harass_force_units(snapshot, context).empty();
}

} // namespace Game::Systems::AI
