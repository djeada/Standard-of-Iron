#include "ai_command_filter.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems::AI {

std::vector<AICommand>
AICommandFilter::filter(const std::vector<AICommand> &commands,
                        float currentTime) {
  std::vector<AICommand> filtered;
  filtered.reserve(commands.size());

  for (const auto &cmd : commands) {

    if (cmd.type == AICommandType::StartProduction) {
      filtered.push_back(cmd);
      continue;
    }

    std::vector<Engine::Core::EntityID> validUnits;
    validUnits.reserve(cmd.units.size());
    int blockedCount = 0;

    for (size_t i = 0; i < cmd.units.size(); ++i) {
      Engine::Core::EntityID unitId = cmd.units[i];

      Engine::Core::EntityID targetId = 0;
      float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;

      if (cmd.type == AICommandType::AttackTarget) {
        targetId = cmd.targetId;
      } else if (cmd.type == AICommandType::MoveUnits) {

        if (i < cmd.moveTargetX.size()) {
          moveX = cmd.moveTargetX[i];
          moveY = cmd.moveTargetY[i];
          moveZ = cmd.moveTargetZ[i];
        }
      }

      if (!isDuplicate(unitId, cmd.type, targetId, moveX, moveY, moveZ,
                       currentTime)) {
        validUnits.push_back(unitId);
      } else {
        blockedCount++;
      }
    }

    if (blockedCount > 0) {
      continue;
    }

    if (!validUnits.empty()) {
      AICommand filteredCmd = cmd;
      filteredCmd.units = validUnits;

      if (cmd.type == AICommandType::MoveUnits) {
        std::vector<float> newTargetX, newTargetY, newTargetZ;
        newTargetX.reserve(validUnits.size());
        newTargetY.reserve(validUnits.size());
        newTargetZ.reserve(validUnits.size());

        for (size_t i = 0; i < cmd.units.size(); ++i) {

          if (std::find(validUnits.begin(), validUnits.end(), cmd.units[i]) !=
              validUnits.end()) {
            if (i < cmd.moveTargetX.size()) {
              newTargetX.push_back(cmd.moveTargetX[i]);
              newTargetY.push_back(cmd.moveTargetY[i]);
              newTargetZ.push_back(cmd.moveTargetZ[i]);
            }
          }
        }

        filteredCmd.moveTargetX = std::move(newTargetX);
        filteredCmd.moveTargetY = std::move(newTargetY);
        filteredCmd.moveTargetZ = std::move(newTargetZ);
      }

      filtered.push_back(std::move(filteredCmd));

      recordCommand(filtered.back(), currentTime);
    }
  }

  return filtered;
}

void AICommandFilter::update(float currentTime) { cleanupHistory(currentTime); }

void AICommandFilter::reset() { m_history.clear(); }

bool AICommandFilter::isDuplicate(Engine::Core::EntityID unitId,
                                  AICommandType type,
                                  Engine::Core::EntityID targetId, float moveX,
                                  float moveY, float moveZ,
                                  float currentTime) const {
  for (const auto &entry : m_history) {
    if (entry.isSimilarTo(type, unitId, targetId, moveX, moveY, moveZ,
                          currentTime, m_cooldownPeriod)) {
      return true;
    }
  }
  return false;
}

void AICommandFilter::recordCommand(const AICommand &cmd, float currentTime) {
  for (size_t i = 0; i < cmd.units.size(); ++i) {
    CommandHistory entry;
    entry.unitId = cmd.units[i];
    entry.type = cmd.type;
    entry.issuedTime = currentTime;

    if (cmd.type == AICommandType::AttackTarget) {
      entry.targetId = cmd.targetId;
      entry.moveTargetX = 0.0f;
      entry.moveTargetY = 0.0f;
      entry.moveTargetZ = 0.0f;
    } else if (cmd.type == AICommandType::MoveUnits) {
      entry.targetId = 0;
      if (i < cmd.moveTargetX.size()) {
        entry.moveTargetX = cmd.moveTargetX[i];
        entry.moveTargetY = cmd.moveTargetY[i];
        entry.moveTargetZ = cmd.moveTargetZ[i];
      }
    } else {

      entry.targetId = 0;
      entry.moveTargetX = 0.0f;
      entry.moveTargetY = 0.0f;
      entry.moveTargetZ = 0.0f;
    }

    m_history.push_back(entry);
  }
}

void AICommandFilter::cleanupHistory(float currentTime) {

  m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
                                 [&](const CommandHistory &entry) {
                                   return (currentTime - entry.issuedTime) >
                                          m_cooldownPeriod;
                                 }),
                  m_history.end());
}

bool AICommandFilter::CommandHistory::isSimilarTo(const AICommandType &cmdType,
                                                  Engine::Core::EntityID unit,
                                                  Engine::Core::EntityID target,
                                                  float x, float y, float z,
                                                  float currentTime,
                                                  float cooldown) const {

  if (unitId != unit)
    return false;

  if (type != cmdType)
    return false;

  if ((currentTime - issuedTime) > cooldown)
    return false;

  switch (cmdType) {
  case AICommandType::AttackTarget:

    return (targetId == target);

  case AICommandType::MoveUnits: {

    const float dx = moveTargetX - x;
    const float dy = moveTargetY - y;
    const float dz = moveTargetZ - z;
    const float distSq = dx * dx + dy * dy + dz * dz;
    const float threshold = 3.0f * 3.0f;
    return distSq < threshold;
  }

  case AICommandType::StartProduction:

    return true;

  default:
    return false;
  }
}

} // namespace Game::Systems::AI
