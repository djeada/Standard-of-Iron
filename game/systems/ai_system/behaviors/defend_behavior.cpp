#include "defend_behavior.h"
#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_tactical.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <qvectornd.h>
#include <utility>
#include <vector>

namespace Game::Systems::AI {

void DefendBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float delta_time,
                             std::vector<AICommand> &outCommands) {
  m_defendTimer += delta_time;

  float const update_interval = context.barracksUnderThreat ? 0.5F : 1.5F;

  if (m_defendTimer < update_interval) {
    return;
  }
  m_defendTimer = 0.0F;

  if (context.primaryBarracks == 0) {
    return;
  }

  float defend_pos_x = 0.0F;
  float defend_pos_y = 0.0F;
  float defend_pos_z = 0.0F;
  bool found_barracks = false;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.id == context.primaryBarracks) {
      defend_pos_x = entity.posX;
      defend_pos_y = entity.posY;
      defend_pos_z = entity.posZ;
      found_barracks = true;
      break;
    }
  }

  if (!found_barracks) {
    return;
  }

  std::vector<const EntitySnapshot *> ready_defenders;
  std::vector<const EntitySnapshot *> engaged_defenders;
  ready_defenders.reserve(snapshot.friendlies.size());
  engaged_defenders.reserve(snapshot.friendlies.size());

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (isEntityEngaged(entity, snapshot.visibleEnemies)) {
      engaged_defenders.push_back(&entity);
    } else {
      ready_defenders.push_back(&entity);
    }
  }

  if (ready_defenders.empty() && engaged_defenders.empty()) {
    return;
  }

  auto sort_by_distance = [&](std::vector<const EntitySnapshot *> &list) {
    std::sort(list.begin(), list.end(),
              [&](const EntitySnapshot *a, const EntitySnapshot *b) {
                float const da =
                    distance_squared(a->posX, a->posY, a->posZ, defend_pos_x,
                                     defend_pos_y, defend_pos_z);
                float const db =
                    distance_squared(b->posX, b->posY, b->posZ, defend_pos_x,
                                     defend_pos_y, defend_pos_z);
                return da < db;
              });
  };

  sort_by_distance(ready_defenders);
  sort_by_distance(engaged_defenders);

  const std::size_t total_available =
      ready_defenders.size() + engaged_defenders.size();
  std::size_t desired_count = total_available;

  if (context.barracksUnderThreat || !context.buildingsUnderAttack.empty()) {

    desired_count = total_available;
  } else {

    desired_count =
        std::min<std::size_t>(desired_count, static_cast<std::size_t>(6));
  }

  std::size_t const ready_count =
      std::min(desired_count, ready_defenders.size());
  ready_defenders.resize(ready_count);

  if (ready_defenders.empty()) {
    return;
  }

  if (context.barracksUnderThreat || !context.buildingsUnderAttack.empty()) {

    std::vector<const ContactSnapshot *> nearby_threats;
    nearby_threats.reserve(snapshot.visibleEnemies.size());

    constexpr float defend_radius = 40.0F;
    const float defend_radius_sq = defend_radius * defend_radius;

    for (const auto &enemy : snapshot.visibleEnemies) {
      float const dist_sq =
          distance_squared(enemy.posX, enemy.posY, enemy.posZ, defend_pos_x,
                           defend_pos_y, defend_pos_z);
      if (dist_sq <= defend_radius_sq) {
        nearby_threats.push_back(&enemy);
      }
    }

    if (!nearby_threats.empty()) {

      auto target_info = TacticalUtils::selectFocusFireTarget(
          ready_defenders, nearby_threats, defend_pos_x, defend_pos_y,
          defend_pos_z, context, 0);

      if (target_info.target_id != 0) {

        std::vector<Engine::Core::EntityID> defender_ids;
        defender_ids.reserve(ready_defenders.size());
        for (const auto *unit : ready_defenders) {
          defender_ids.push_back(unit->id);
        }

        auto claimed_units =
            claimUnits(defender_ids, getPriority(), "defending", context,
                       m_defendTimer + delta_time, 3.0F);

        if (!claimed_units.empty()) {
          AICommand attack;
          attack.type = AICommandType::AttackTarget;
          attack.units = std::move(claimed_units);
          attack.target_id = target_info.target_id;
          attack.should_chase = true;
          outCommands.push_back(std::move(attack));
          return;
        }
      }
    } else if (context.barracksUnderThreat ||
               !context.buildingsUnderAttack.empty()) {

      const ContactSnapshot *closest_threat = nullptr;
      float closest_dist_sq = std::numeric_limits<float>::max();

      for (const auto &enemy : snapshot.visibleEnemies) {
        float const dist_sq =
            distance_squared(enemy.posX, enemy.posY, enemy.posZ, defend_pos_x,
                             defend_pos_y, defend_pos_z);
        if (dist_sq < closest_dist_sq) {
          closest_dist_sq = dist_sq;
          closest_threat = &enemy;
        }
      }

      if ((closest_threat != nullptr) && !ready_defenders.empty()) {

        std::vector<Engine::Core::EntityID> defender_ids;
        std::vector<float> target_x;
        std::vector<float> target_y;
        std::vector<float> target_z;

        for (const auto *unit : ready_defenders) {
          defender_ids.push_back(unit->id);
          target_x.push_back(closest_threat->posX);
          target_y.push_back(closest_threat->posY);
          target_z.push_back(closest_threat->posZ);
        }

        auto claimed_units =
            claimUnits(defender_ids, getPriority(), "intercepting", context,
                       m_defendTimer + delta_time, 2.0F);

        if (!claimed_units.empty()) {

          std::vector<float> filtered_x;
          std::vector<float> filtered_y;
          std::vector<float> filtered_z;
          for (size_t i = 0; i < defender_ids.size(); ++i) {
            if (std::find(claimed_units.begin(), claimed_units.end(),
                          defender_ids[i]) != claimed_units.end()) {
              filtered_x.push_back(target_x[i]);
              filtered_y.push_back(target_y[i]);
              filtered_z.push_back(target_z[i]);
            }
          }

          AICommand move;
          move.type = AICommandType::MoveUnits;
          move.units = std::move(claimed_units);
          move.moveTargetX = std::move(filtered_x);
          move.moveTargetY = std::move(filtered_y);
          move.moveTargetZ = std::move(filtered_z);
          outCommands.push_back(std::move(move));
          return;
        }
      }
    }
  }

  std::vector<const EntitySnapshot *> unclaimed_defenders;
  for (const auto *unit : ready_defenders) {
    auto it = context.assignedUnits.find(unit->id);
    if (it == context.assignedUnits.end()) {
      unclaimed_defenders.push_back(unit);
    }
  }

  if (unclaimed_defenders.empty()) {
    return;
  }

  const Nation *nation =
      NationRegistry::instance().getNationForPlayer(context.player_id);
  FormationType formation_type = FormationType::Roman;
  if (nation != nullptr) {
    formation_type = nation->formation_type;
  }

  QVector3D const defend_pos(defend_pos_x, defend_pos_y, defend_pos_z);
  auto targets = FormationSystem::instance().get_formation_positions(
      formation_type, static_cast<int>(unclaimed_defenders.size()), defend_pos,
      3.0F);

  std::vector<Engine::Core::EntityID> units_to_move;
  std::vector<float> target_x;
  std::vector<float> target_y;
  std::vector<float> target_z;
  units_to_move.reserve(unclaimed_defenders.size());
  target_x.reserve(unclaimed_defenders.size());
  target_y.reserve(unclaimed_defenders.size());
  target_z.reserve(unclaimed_defenders.size());

  for (size_t i = 0; i < unclaimed_defenders.size(); ++i) {
    const auto *entity = unclaimed_defenders[i];
    const auto &target = targets[i];

    float const dx = entity->posX - target.x();
    float const dz = entity->posZ - target.z();
    float const distance_sq = dx * dx + dz * dz;

    if (distance_sq < 1.0F * 1.0F) {
      continue;
    }

    units_to_move.push_back(entity->id);
    target_x.push_back(target.x());
    target_y.push_back(target.y());
    target_z.push_back(target.z());
  }

  if (units_to_move.empty()) {
    return;
  }

  auto claimed_for_move =
      claimUnits(units_to_move, BehaviorPriority::Low, "positioning", context,
                 m_defendTimer + delta_time, 1.5F);

  if (claimed_for_move.empty()) {
    return;
  }

  std::vector<float> filtered_x;
  std::vector<float> filtered_y;
  std::vector<float> filtered_z;
  for (size_t i = 0; i < units_to_move.size(); ++i) {
    if (std::find(claimed_for_move.begin(), claimed_for_move.end(),
                  units_to_move[i]) != claimed_for_move.end()) {
      filtered_x.push_back(target_x[i]);
      filtered_y.push_back(target_y[i]);
      filtered_z.push_back(target_z[i]);
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimed_for_move);
  command.moveTargetX = std::move(filtered_x);
  command.moveTargetY = std::move(filtered_y);
  command.moveTargetZ = std::move(filtered_z);
  outCommands.push_back(std::move(command));
}

auto DefendBehavior::should_execute(const AISnapshot &snapshot,
                                    const AIContext &context) const -> bool {
  (void)snapshot;

  if (context.primaryBarracks == 0) {
    return false;
  }

  if (context.barracksUnderThreat || !context.buildingsUnderAttack.empty()) {
    return true;
  }

  if (context.state == AIState::Defending && context.idleUnits > 0) {
    return true;
  }

  if (context.averageHealth < 0.6F && context.total_units > 0) {
    return true;
  }

  return false;
}

} // namespace Game::Systems::AI
