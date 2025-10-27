#include "retreat_behavior.h"
#include "../../formation_planner.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

#include <QVector3D>
#include <algorithm>
#include <cstddef>
#include <qvectornd.h>
#include <utility>
#include <vector>

namespace Game::Systems::AI {

void RetreatBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                              float deltaTime,
                              std::vector<AICommand> &outCommands) {
  m_retreatTimer += deltaTime;
  if (m_retreatTimer < 1.0F) {
    return;
  }
  m_retreatTimer = 0.0F;

  if (context.primaryBarracks == 0) {
    return;
  }

  std::vector<const EntitySnapshot *> retreating_units;
  retreating_units.reserve(snapshot.friendlies.size());

  constexpr float critical_health = 0.35F;
  constexpr float low_health = 0.50F;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (entity.max_health <= 0) {
      continue;
    }

    float const health_ratio = static_cast<float>(entity.health) /
                               static_cast<float>(entity.max_health);

    if (health_ratio < critical_health) {
      retreating_units.push_back(&entity);
    }

    else if (health_ratio < low_health &&
             isEntityEngaged(entity, snapshot.visibleEnemies)) {
      retreating_units.push_back(&entity);
    }
  }

  if (retreating_units.empty()) {
    return;
  }

  QVector3D retreat_pos(context.basePosX, context.basePosY, context.basePosZ);

  retreat_pos.setX(retreat_pos.x() - 8.0F);

  auto retreat_targets = FormationPlanner::spreadFormation(
      static_cast<int>(retreating_units.size()), retreat_pos, 2.0F);

  std::vector<Engine::Core::EntityID> unit_ids;
  std::vector<float> target_x;
  std::vector<float> target_y;
  std::vector<float> target_z;
  unit_ids.reserve(retreating_units.size());
  target_x.reserve(retreating_units.size());
  target_y.reserve(retreating_units.size());
  target_z.reserve(retreating_units.size());

  for (size_t i = 0; i < retreating_units.size(); ++i) {
    unit_ids.push_back(retreating_units[i]->id);
    target_x.push_back(retreat_targets[i].x());
    target_y.push_back(retreat_targets[i].y());
    target_z.push_back(retreat_targets[i].z());
  }

  auto claimed_units = claimUnits(unit_ids, getPriority(), "retreating",
                                  context, m_retreatTimer + deltaTime, 1.0F);

  if (claimed_units.empty()) {
    return;
  }

  std::vector<float> filtered_x;
  std::vector<float> filtered_y;
  std::vector<float> filtered_z;
  for (size_t i = 0; i < unit_ids.size(); ++i) {
    if (std::find(claimed_units.begin(), claimed_units.end(), unit_ids[i]) !=
        claimed_units.end()) {
      filtered_x.push_back(target_x[i]);
      filtered_y.push_back(target_y[i]);
      filtered_z.push_back(target_z[i]);
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimed_units);
  command.moveTargetX = std::move(filtered_x);
  command.moveTargetY = std::move(filtered_y);
  command.moveTargetZ = std::move(filtered_z);
  outCommands.push_back(std::move(command));
}

auto RetreatBehavior::should_execute(const AISnapshot &snapshot,
                                     const AIContext &context) const -> bool {
  if (context.primaryBarracks == 0) {
    return false;
  }

  if (context.state == AIState::Retreating) {
    return true;
  }

  constexpr float critical_health = 0.35F;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (entity.max_health > 0) {
      float const health_ratio = static_cast<float>(entity.health) /
                                 static_cast<float>(entity.max_health);
      if (health_ratio < critical_health) {
        return true;
      }
    }
  }

  return false;
}

} // namespace Game::Systems::AI
