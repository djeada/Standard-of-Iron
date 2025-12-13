#include "gather_behavior.h"
#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

#include <QVector3D>
#include <algorithm>
#include <cstddef>
#include <qvectornd.h>
#include <utility>
#include <vector>

namespace Game::Systems::AI {

void GatherBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float delta_time,
                             std::vector<AICommand> &outCommands) {
  m_gatherTimer += delta_time;

  if (m_gatherTimer < 1.0F) {
    return;
  }
  m_gatherTimer = 0.0F;

  if (context.primaryBarracks == 0) {
    return;
  }

  QVector3D const rally_point(context.rally_x, 0.0F, context.rally_z);

  std::vector<const EntitySnapshot *> units_to_gather;
  units_to_gather.reserve(snapshot.friendlies.size());

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (isEntityEngaged(entity, snapshot.visibleEnemies)) {
      continue;
    }

    const float dx = entity.posX - rally_point.x();
    const float dz = entity.posZ - rally_point.z();
    const float dist_sq = dx * dx + dz * dz;

    if (dist_sq > 2.0F * 2.0F) {
      units_to_gather.push_back(&entity);
    }
  }

  if (units_to_gather.empty()) {
    return;
  }

  const Nation *nation =
      NationRegistry::instance().getNationForPlayer(context.player_id);
  FormationType formation_type = FormationType::Roman;
  if (nation != nullptr) {
    formation_type = nation->formation_type;
  }

  auto formation_targets = FormationSystem::instance().get_formation_positions(
      formation_type, static_cast<int>(units_to_gather.size()), rally_point,
      1.4F);

  std::vector<Engine::Core::EntityID> units_to_move;
  std::vector<float> target_x;
  std::vector<float> target_y;
  std::vector<float> target_z;
  units_to_move.reserve(units_to_gather.size());
  target_x.reserve(units_to_gather.size());
  target_y.reserve(units_to_gather.size());
  target_z.reserve(units_to_gather.size());

  for (size_t i = 0; i < units_to_gather.size(); ++i) {
    const auto *entity = units_to_gather[i];
    const auto &target = formation_targets[i];

    units_to_move.push_back(entity->id);
    target_x.push_back(target.x());
    target_y.push_back(target.y());
    target_z.push_back(target.z());
  }

  if (units_to_move.empty()) {
    return;
  }

  auto claimed_units = claimUnits(units_to_move, getPriority(), "gathering",
                                  context, m_gatherTimer + delta_time, 2.0F);

  if (claimed_units.empty()) {
    return;
  }

  std::vector<float> filtered_x;
  std::vector<float> filtered_y;
  std::vector<float> filtered_z;
  for (size_t i = 0; i < units_to_move.size(); ++i) {
    if (std::find(claimed_units.begin(), claimed_units.end(),
                  units_to_move[i]) != claimed_units.end()) {
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

auto GatherBehavior::should_execute(const AISnapshot &snapshot,
                                    const AIContext &context) const -> bool {
  if (context.primaryBarracks == 0) {
    return false;
  }

  if (context.state == AIState::Retreating) {
    return false;
  }

  if (context.state == AIState::Attacking) {
    return false;
  }

  if (context.state == AIState::Defending) {

    QVector3D const rally_point(context.rally_x, 0.0F, context.rally_z);
    for (const auto &entity : snapshot.friendlies) {
      if (entity.isBuilding) {
        continue;
      }

      const float dx = entity.posX - rally_point.x();
      const float dz = entity.posZ - rally_point.z();
      const float dist_sq = dx * dx + dz * dz;

      if (dist_sq > 10.0F * 10.0F) {
        return true;
      }
    }
    return false;
  }

  if (context.state == AIState::Gathering || context.state == AIState::Idle) {
    return true;
  }

  return false;
}

} // namespace Game::Systems::AI
