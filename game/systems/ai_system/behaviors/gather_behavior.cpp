#include "gather_behavior.h"
#include "../../formation_planner.h"
#include "../ai_utils.h"

#include <QDebug>
#include <QVector3D>

namespace Game::Systems::AI {

void GatherBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_gatherTimer += deltaTime;

  if (m_gatherTimer < 1.0f)
    return;
  m_gatherTimer = 0.0f;

  if (context.primaryBarracks == 0)
    return;

  QVector3D rallyPoint(context.rallyX, 0.0f, context.rallyZ);

  std::vector<const EntitySnapshot *> unitsToGather;
  unitsToGather.reserve(snapshot.friendlies.size());

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;

    if (isEntityEngaged(entity, snapshot.visibleEnemies))
      continue;

    const float dx = entity.posX - rallyPoint.x();
    const float dz = entity.posZ - rallyPoint.z();
    const float distSq = dx * dx + dz * dz;

    if (distSq > 2.0f * 2.0f) {
      unitsToGather.push_back(&entity);
    }
  }

  if (unitsToGather.empty())
    return;

  auto formationTargets = FormationPlanner::spreadFormation(
      static_cast<int>(unitsToGather.size()), rallyPoint, 1.4f);

  std::vector<Engine::Core::EntityID> unitsToMove;
  std::vector<float> targetX, targetY, targetZ;
  unitsToMove.reserve(unitsToGather.size());
  targetX.reserve(unitsToGather.size());
  targetY.reserve(unitsToGather.size());
  targetZ.reserve(unitsToGather.size());

  for (size_t i = 0; i < unitsToGather.size(); ++i) {
    const auto *entity = unitsToGather[i];
    const auto &target = formationTargets[i];

    unitsToMove.push_back(entity->id);
    targetX.push_back(target.x());
    targetY.push_back(target.y());
    targetZ.push_back(target.z());
  }

  if (unitsToMove.empty())
    return;

  auto claimedUnits = claimUnits(unitsToMove, getPriority(), "gathering",
                                 context, m_gatherTimer + deltaTime, 2.0f);

  if (claimedUnits.empty())
    return;

  std::vector<float> filteredX, filteredY, filteredZ;
  for (size_t i = 0; i < unitsToMove.size(); ++i) {
    if (std::find(claimedUnits.begin(), claimedUnits.end(), unitsToMove[i]) !=
        claimedUnits.end()) {
      filteredX.push_back(targetX[i]);
      filteredY.push_back(targetY[i]);
      filteredZ.push_back(targetZ[i]);
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimedUnits);
  command.moveTargetX = std::move(filteredX);
  command.moveTargetY = std::move(filteredY);
  command.moveTargetZ = std::move(filteredZ);

  qDebug() << "[GatherBehavior] Issuing MoveUnits for" << command.units.size()
           << "units to rally point (" << context.rallyX << ","
           << context.rallyZ << ")"
           << "in state:" << static_cast<int>(context.state);

  outCommands.push_back(std::move(command));
}

bool GatherBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {
  if (context.primaryBarracks == 0)
    return false;

  if (context.state == AIState::Retreating)
    return false;

  if (context.state == AIState::Attacking)
    return false;

  if (context.state == AIState::Defending) {

    QVector3D rallyPoint(context.rallyX, 0.0f, context.rallyZ);
    for (const auto &entity : snapshot.friendlies) {
      if (entity.isBuilding)
        continue;

      const float dx = entity.posX - rallyPoint.x();
      const float dz = entity.posZ - rallyPoint.z();
      const float distSq = dx * dx + dz * dz;

      if (distSq > 10.0f * 10.0f) {
        return true;
      }
    }
    return false;
  }

  if (context.state == AIState::Gathering || context.state == AIState::Idle)
    return true;

  return false;
}

} // namespace Game::Systems::AI
