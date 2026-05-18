#include "attack_behavior.h"

#include <QVector3D>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_tactical.h"
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

constexpr float k_gathering_advance_aggression_threshold = 0.70F;

auto can_advance_from_gathering(const AIContext& context, int ready_units) -> bool {
  return context.state == AIState::Attacking ||
          (context.state == AIState::Gathering &&
           ready_units >= context.strategy_config.reactive_attack_size &&
           context.strategy_config.aggression_modifier >=
               k_gathering_advance_aggression_threshold);
}

auto select_strategic_objective(const AISnapshot& snapshot,
                                float reference_x,
                                float reference_z) -> const ContactSnapshot* {
  const ContactSnapshot* best_objective = nullptr;
  float best_score = std::numeric_limits<float>::infinity();

  for (const auto& objective : snapshot.strategic_objectives) {
    const float dx = objective.pos_x - reference_x;
    const float dz = objective.pos_z - reference_z;
    float score = dx * dx + dz * dz;
    if (!objective.is_building) {
      score += 400.0F;
    }

    if (score < best_score) {
      best_score = score;
      best_objective = &objective;
    }
  }

  return best_objective;
}
} // namespace

void AttackBehavior::execute(const AISnapshot& snapshot,
                             AIContext& context,
                             float delta_time,
                             std::vector<AICommand>& out_commands) {
  m_attack_timer += delta_time;
  m_target_lock_duration += delta_time;

  if (m_attack_timer < 1.5F) {
    return;
  }
  m_attack_timer = 0.0F;

  auto ready_units = collect_attack_force_units(snapshot, context);

  float group_center_x = 0.0F;
  float group_center_y = 0.0F;
  float group_center_z = 0.0F;

  for (const auto* entity : ready_units) {
    group_center_x += entity->pos_x;
    group_center_y += entity->pos_y;
    group_center_z += entity->pos_z;
  }

  if (ready_units.empty()) {
    return;
  }

  float const inv_count = 1.0F / static_cast<float>(ready_units.size());
  group_center_x *= inv_count;
  group_center_y *= inv_count;
  group_center_z *= inv_count;

  if (snapshot.visible_enemies.empty()) {

    constexpr int MIN_UNITS_FOR_SCOUTING = 3;
    if (context.state == AIState::Attacking &&
        static_cast<int>(ready_units.size()) >=
            std::max(MIN_UNITS_FOR_SCOUTING, context.strategy_config.reactive_attack_size)) {

      constexpr float SCOUT_ROTATION_INTERVAL = 10.0F;
      const float scout_advance_distance =
          context.strategy_config.scouting_distance *
          context.strategy_config.difficulty.scouting_distance_multiplier;

      m_last_scout_time += delta_time;
      if (m_last_scout_time > SCOUT_ROTATION_INTERVAL) {
        m_scout_direction = (m_scout_direction + 1) % 4;
        m_last_scout_time = 0.0F;
      }

      float scout_x = 0.0F;
      float scout_z = 0.0F;
      const ContactSnapshot* strategic_objective =
          select_strategic_objective(snapshot, group_center_x, group_center_z);

      if (strategic_objective != nullptr) {
        scout_x = strategic_objective->pos_x;
        scout_z = strategic_objective->pos_z;
      } else if (context.has_base_anchor) {

        switch (m_scout_direction) {
        case 0:
          scout_x = context.base_pos_x;
          scout_z = context.base_pos_z + scout_advance_distance;
          break;
        case 1:
          scout_x = context.base_pos_x + scout_advance_distance;
          scout_z = context.base_pos_z;
          break;
        case 2:
          scout_x = context.base_pos_x;
          scout_z = context.base_pos_z - scout_advance_distance;
          break;
        case 3:
          scout_x = context.base_pos_x - scout_advance_distance;
          scout_z = context.base_pos_z;
          break;
        }
      }

      std::vector<Engine::Core::EntityID> unit_ids;
      unit_ids.reserve(ready_units.size());
      for (const auto* unit : ready_units) {
        unit_ids.push_back(unit->id);
      }

      FormationType formation_type = get_formation_type_for_player(context.player_id);

      QVector3D const scout_center(scout_x, 0.0F, scout_z);
      auto formation_positions = FormationSystem::instance().get_formation_positions(
          formation_type,
          static_cast<int>(ready_units.size()),
          scout_center,
          context.strategy_config.attack_formation_spacing);

      std::vector<float> target_x;
      std::vector<float> target_y;
      std::vector<float> target_z;
      target_x.reserve(ready_units.size());
      target_y.reserve(ready_units.size());
      target_z.reserve(ready_units.size());

      for (size_t i = 0; i < ready_units.size(); ++i) {
        target_x.push_back(formation_positions[i].x());
        target_y.push_back(formation_positions[i].y());
        target_z.push_back(formation_positions[i].z());
      }

      AICommand cmd;
      cmd.type = AICommandType::MoveUnits;
      cmd.units = std::move(unit_ids);
      cmd.move_target_x = std::move(target_x);
      cmd.move_target_y = std::move(target_y);
      cmd.move_target_z = std::move(target_z);
      out_commands.push_back(cmd);
    }
    return;
  }

  std::vector<const ContactSnapshot*> nearby_enemies;
  nearby_enemies.reserve(snapshot.visible_enemies.size());

  const float harassment_r = context.strategy_config.harassment_range;
  const float engagement_range =
      (harassment_r > 0.0F) ? harassment_r
                            : ((context.damaged_units_count > 0) ? 35.0F : 20.0F);
  const float engage_range_sq = engagement_range * engagement_range;

  for (const auto& enemy : snapshot.visible_enemies) {
    float const dist_sq = distance_squared(enemy.pos_x,
                                           enemy.pos_y,
                                           enemy.pos_z,
                                           group_center_x,
                                           group_center_y,
                                           group_center_z);
    if (dist_sq <= engage_range_sq) {
      nearby_enemies.push_back(&enemy);
    }
  }

  if (nearby_enemies.empty()) {

    bool const should_advance =
        can_advance_from_gathering(context, static_cast<int>(ready_units.size()));

    if (should_advance && !snapshot.visible_enemies.empty()) {

      const ContactSnapshot* closest_enemy = nullptr;
      float closest_dist_sq = std::numeric_limits<float>::max();

      for (const auto& enemy : snapshot.visible_enemies) {
        if (enemy.is_building) {
          continue;
        }
        float const dist_sq = distance_squared(enemy.pos_x,
                                               enemy.pos_y,
                                               enemy.pos_z,
                                               group_center_x,
                                               group_center_y,
                                               group_center_z);
        if (dist_sq < closest_dist_sq) {
          closest_dist_sq = dist_sq;
          closest_enemy = &enemy;
        }
      }

      const ContactSnapshot* target = closest_enemy;

      if ((target != nullptr) && !ready_units.empty()) {

        float attack_pos_x = target->pos_x;
        float attack_pos_z = target->pos_z;

        bool needs_new_command = false;
        if (m_last_target != target->id) {
          needs_new_command = true;
          m_last_target = target->id;
          m_target_lock_duration = 0.0F;
        } else {

          for (const auto* unit : ready_units) {
            float const dx = unit->pos_x - attack_pos_x;
            float const dz = unit->pos_z - attack_pos_z;
            float const dist_sq = dx * dx + dz * dz;
            if (dist_sq > 15.0F * 15.0F) {
              needs_new_command = true;
              break;
            }
          }
        }

        if (needs_new_command) {
          std::vector<Engine::Core::EntityID> unit_ids;
          unit_ids.reserve(ready_units.size());
          for (const auto* unit : ready_units) {
            unit_ids.push_back(unit->id);
          }

          FormationType formation_type =
              get_formation_type_for_player(context.player_id);

          QVector3D const attack_center(attack_pos_x, 0.0F, attack_pos_z);
          auto formation_positions =
              FormationSystem::instance().get_formation_positions(
                  formation_type,
                  static_cast<int>(ready_units.size()),
                  attack_center,
                  context.strategy_config.attack_formation_spacing);

          std::vector<float> target_x;
          std::vector<float> target_y;
          std::vector<float> target_z;
          target_x.reserve(ready_units.size());
          target_y.reserve(ready_units.size());
          target_z.reserve(ready_units.size());

          for (size_t i = 0; i < ready_units.size(); ++i) {
            target_x.push_back(formation_positions[i].x());
            target_y.push_back(formation_positions[i].y());
            target_z.push_back(formation_positions[i].z());
          }

          AICommand cmd;
          cmd.type = AICommandType::MoveUnits;
          cmd.units = std::move(unit_ids);
          cmd.move_target_x = std::move(target_x);
          cmd.move_target_y = std::move(target_y);
          cmd.move_target_z = std::move(target_z);
          out_commands.push_back(cmd);
        }
      }
    }

    m_last_target = 0;
    m_target_lock_duration = 0.0F;
    return;
  }

  auto assessment = TacticalUtils::assess_engagement(
      ready_units, nearby_enemies, context.state == AIState::Attacking ? 0.7F : 0.9F);

  bool const being_attacked = context.damaged_units_count > 0;

  if (!assessment.should_engage && !context.barracks_under_threat && !being_attacked) {

    m_last_target = 0;
    m_target_lock_duration = 0.0F;
    return;
  }

  bool last_target_still_valid = false;
  if (m_last_target != 0) {
    for (const auto* enemy : nearby_enemies) {
      if (enemy->id == m_last_target) {
        last_target_still_valid = true;
        break;
      }
    }
  }

  if (!last_target_still_valid || m_target_lock_duration > 8.0F) {
    m_last_target = 0;
    m_target_lock_duration = 0.0F;
  }

  auto target_info = TacticalUtils::select_focus_fire_target(ready_units,
                                                             nearby_enemies,
                                                             group_center_x,
                                                             group_center_y,
                                                             group_center_z,
                                                             context,
                                                             m_last_target);

  if (target_info.target_id == 0) {
    return;
  }

  if (target_info.target_id != m_last_target) {
    m_last_target = target_info.target_id;
    m_target_lock_duration = 0.0F;
  }

  const ContactSnapshot* target_snapshot = nullptr;
  for (const auto* enemy : nearby_enemies) {
    if (enemy->id == target_info.target_id) {
      target_snapshot = enemy;
      break;
    }
  }

  if (target_snapshot == nullptr) {
    return;
  }

  std::vector<Engine::Core::EntityID> unit_ids;
  unit_ids.reserve(ready_units.size());
  for (const auto* unit : ready_units) {
    unit_ids.push_back(unit->id);
  }

  auto claimed_units = claim_units(unit_ids,
                                   get_priority(),
                                   "attacking",
                                   context,
                                   snapshot.game_time,
                                   2.5F);

  if (claimed_units.empty()) {
    return;
  }

  FormationType formation_type = get_formation_type_for_player(context.player_id);

  if (target_snapshot->is_building) {
    QVector3D const attack_center(target_snapshot->pos_x, 0.0F, target_snapshot->pos_z);
    auto formation_positions = FormationSystem::instance().get_formation_positions(
        formation_type,
        static_cast<int>(claimed_units.size()),
        attack_center,
        context.strategy_config.attack_formation_spacing);

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
  attack_command.target_id = target_info.target_id;
  attack_command.should_chase = !target_snapshot->is_building;

  out_commands.push_back(std::move(attack_command));
}
auto AttackBehavior::should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool {

  if (context.state == AIState::Retreating) {
    return false;
  }

  int ready_units = 0;
  ready_units = static_cast<int>(collect_attack_force_units(snapshot, context).size());

  if (ready_units == 0) {
    return false;
  }

  if (context.state == AIState::Attacking) {
    return true;
  }

  if (snapshot.visible_enemies.empty()) {
    return false;
  }

  if (context.state == AIState::Defending) {
    if (context.primary_barracks == 0) {
      return !snapshot.visible_enemies.empty() && ready_units >= 1;
    }
    return context.barracks_under_threat && ready_units >= 2;
  }

  return ready_units >= 1;
}

} // namespace Game::Systems::AI
