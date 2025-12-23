#include "attack_behavior.h"
#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_tactical.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

#include <QVector3D>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace Game::Systems::AI {

namespace {
auto get_formation_type_for_player(int player_id) -> FormationType {
  const Nation *nation =
      NationRegistry::instance().get_nation_for_player(player_id);
  if (nation != nullptr) {
    return nation->formation_type;
  }
  return FormationType::Roman;
}
} // namespace

void AttackBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float delta_time,
                             std::vector<AICommand> &outCommands) {
  m_attackTimer += delta_time;
  m_targetLockDuration += delta_time;

  if (m_attackTimer < 1.5F) {
    return;
  }
  m_attackTimer = 0.0F;

  std::vector<const EntitySnapshot *> engaged_units;
  std::vector<const EntitySnapshot *> ready_units;
  engaged_units.reserve(snapshot.friendly_units.size());
  ready_units.reserve(snapshot.friendly_units.size());

  float group_center_x = 0.0F;
  float group_center_y = 0.0F;
  float group_center_z = 0.0F;

  for (const auto &entity : snapshot.friendly_units) {
    if (entity.is_building) {
      continue;
    }

    if (isEntityEngaged(entity, snapshot.visible_enemies)) {
      engaged_units.push_back(&entity);
      continue;
    }

    ready_units.push_back(&entity);
    group_center_x += entity.posX;
    group_center_y += entity.posY;
    group_center_z += entity.posZ;
  }

  if (ready_units.empty()) {

    if (!engaged_units.empty()) {
    }
    return;
  }

  float const inv_count = 1.0F / static_cast<float>(ready_units.size());
  group_center_x *= inv_count;
  group_center_y *= inv_count;
  group_center_z *= inv_count;

  if (snapshot.visible_enemies.empty()) {

    constexpr int MIN_UNITS_FOR_SCOUTING = 3;
    if (context.state == AIState::Attacking &&
        context.total_units >= MIN_UNITS_FOR_SCOUTING) {

      constexpr float SCOUT_ADVANCE_DISTANCE = 40.0F;
      constexpr float SCOUT_ROTATION_INTERVAL = 10.0F;

      m_lastScoutTime += delta_time;
      if (m_lastScoutTime > SCOUT_ROTATION_INTERVAL) {
        m_scoutDirection = (m_scoutDirection + 1) % 4;
        m_lastScoutTime = 0.0F;
      }

      float scout_x = 0.0F;
      float scout_z = 0.0F;

      if (context.primary_barracks != 0) {

        switch (m_scoutDirection) {
        case 0:
          scout_x = context.base_pos_x;
          scout_z = context.base_pos_z + SCOUT_ADVANCE_DISTANCE;
          break;
        case 1:
          scout_x = context.base_pos_x + SCOUT_ADVANCE_DISTANCE;
          scout_z = context.base_pos_z;
          break;
        case 2:
          scout_x = context.base_pos_x;
          scout_z = context.base_pos_z - SCOUT_ADVANCE_DISTANCE;
          break;
        case 3:
          scout_x = context.base_pos_x - SCOUT_ADVANCE_DISTANCE;
          scout_z = context.base_pos_z;
          break;
        }
      } else {

        scout_x = 0.0F;
        scout_z = 0.0F;
      }

      std::vector<Engine::Core::EntityID> unit_ids;
      unit_ids.reserve(ready_units.size());
      for (const auto *unit : ready_units) {
        unit_ids.push_back(unit->id);
      }

      // Use nation-specific formation for scouting movement
      FormationType formation_type =
          get_formation_type_for_player(context.player_id);

      QVector3D const scout_center(scout_x, 0.0F, scout_z);
      auto formation_positions =
          FormationSystem::instance().get_formation_positions(
              formation_type, static_cast<int>(ready_units.size()), scout_center,
              2.5F);

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
      outCommands.push_back(cmd);
    }
    return;
  }

  std::vector<const ContactSnapshot *> nearby_enemies;
  nearby_enemies.reserve(snapshot.visible_enemies.size());

  const float engagement_range =
      (context.damaged_units_count > 0) ? 35.0F : 20.0F;
  const float engage_range_sq = engagement_range * engagement_range;

  for (const auto &enemy : snapshot.visible_enemies) {
    float const dist_sq =
        distance_squared(enemy.posX, enemy.posY, enemy.posZ, group_center_x,
                         group_center_y, group_center_z);
    if (dist_sq <= engage_range_sq) {
      nearby_enemies.push_back(&enemy);
    }
  }

  if (nearby_enemies.empty()) {

    bool const should_advance =
        (context.state == AIState::Attacking) ||
        (context.state == AIState::Gathering && context.total_units >= 3);

    if (should_advance && !snapshot.visible_enemies.empty()) {

      const ContactSnapshot *closest_enemy = nullptr;
      float closest_dist_sq = std::numeric_limits<float>::max();

      for (const auto &enemy : snapshot.visible_enemies) {
        if (enemy.is_building) {
          continue;
        }
        float const dist_sq =
            distance_squared(enemy.posX, enemy.posY, enemy.posZ, group_center_x,
                             group_center_y, group_center_z);
        if (dist_sq < closest_dist_sq) {
          closest_dist_sq = dist_sq;
          closest_enemy = &enemy;
        }
      }

      const ContactSnapshot *target = closest_enemy;

      if ((target != nullptr) && !ready_units.empty()) {

        float attack_pos_x = target->posX;
        float attack_pos_z = target->posZ;

        bool needs_new_command = false;
        if (m_lastTarget != target->id) {
          needs_new_command = true;
          m_lastTarget = target->id;
          m_targetLockDuration = 0.0F;
        } else {

          for (const auto *unit : ready_units) {
            float const dx = unit->posX - attack_pos_x;
            float const dz = unit->posZ - attack_pos_z;
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
          for (const auto *unit : ready_units) {
            unit_ids.push_back(unit->id);
          }

          // Use nation-specific formation when advancing to attack
          FormationType formation_type =
              get_formation_type_for_player(context.player_id);

          QVector3D const attack_center(attack_pos_x, 0.0F, attack_pos_z);
          auto formation_positions =
              FormationSystem::instance().get_formation_positions(
                  formation_type, static_cast<int>(ready_units.size()),
                  attack_center, 2.5F);

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
          outCommands.push_back(cmd);
        }
      }
    }

    m_lastTarget = 0;
    m_targetLockDuration = 0.0F;
    return;
  }

  auto assessment = TacticalUtils::assess_engagement(
      ready_units, nearby_enemies,
      context.state == AIState::Attacking ? 0.7F : 0.9F);

  bool const being_attacked = context.damaged_units_count > 0;

  if (!assessment.should_engage && !context.barracks_under_threat &&
      !being_attacked) {

    m_lastTarget = 0;
    m_targetLockDuration = 0.0F;
    return;
  }

  bool last_target_still_valid = false;
  if (m_lastTarget != 0) {
    for (const auto *enemy : nearby_enemies) {
      if (enemy->id == m_lastTarget) {
        last_target_still_valid = true;
        break;
      }
    }
  }

  if (!last_target_still_valid || m_targetLockDuration > 8.0F) {
    m_lastTarget = 0;
    m_targetLockDuration = 0.0F;
  }

  auto target_info = TacticalUtils::select_focus_fire_target(
      ready_units, nearby_enemies, group_center_x, group_center_y,
      group_center_z, context, m_lastTarget);

  if (target_info.target_id == 0) {
    return;
  }

  if (target_info.target_id != m_lastTarget) {
    m_lastTarget = target_info.target_id;
    m_targetLockDuration = 0.0F;
  }

  // Find the target's position from nearby_enemies
  const ContactSnapshot *target_snapshot = nullptr;
  for (const auto *enemy : nearby_enemies) {
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
  for (const auto *unit : ready_units) {
    unit_ids.push_back(unit->id);
  }

  auto claimed_units = claim_units(unit_ids, get_priority(), "attacking",
                                   context, m_attackTimer + delta_time, 2.5F);

  if (claimed_units.empty()) {
    return;
  }

  // Use formation positions to spread units out when engaging nearby enemies
  FormationType formation_type =
      get_formation_type_for_player(context.player_id);

  QVector3D const attack_center(target_snapshot->posX, 0.0F, target_snapshot->posZ);
  auto formation_positions =
      FormationSystem::instance().get_formation_positions(
          formation_type, static_cast<int>(claimed_units.size()),
          attack_center, 2.5F);

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

  outCommands.push_back(std::move(move_command));

  // Set attack target so units engage when in range, but don't chase
  // This prevents units from moving and breaking formation
  AICommand attack_command;
  attack_command.type = AICommandType::AttackTarget;
  attack_command.units = std::move(claimed_units);
  attack_command.target_id = target_info.target_id;
  attack_command.should_chase = false;

  outCommands.push_back(std::move(attack_command));
}
auto AttackBehavior::should_execute(const AISnapshot &snapshot,
                                    const AIContext &context) const -> bool {

  if (context.state == AIState::Retreating) {
    return false;
  }

  int ready_units = 0;
  for (const auto &entity : snapshot.friendly_units) {
    if (entity.is_building) {
      continue;
    }

    if (isEntityEngaged(entity, snapshot.visible_enemies)) {
      continue;
    }

    ++ready_units;
  }

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
    return context.barracks_under_threat && ready_units >= 2;
  }

  return ready_units >= 1;
}

} // namespace Game::Systems::AI
