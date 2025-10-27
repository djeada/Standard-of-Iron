#include "ai_command_filter.h"
#include "systems/ai_system/ai_types.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace Game::Systems::AI {

auto AICommandFilter::filter(const std::vector<AICommand> &commands,
                             float currentTime) -> std::vector<AICommand> {
  std::vector<AICommand> filtered;
  filtered.reserve(commands.size());

  for (const auto &cmd : commands) {

    if (cmd.type == AICommandType::StartProduction) {
      filtered.push_back(cmd);
      continue;
    }

    std::vector<Engine::Core::EntityID> valid_units;
    valid_units.reserve(cmd.units.size());
    int blocked_count = 0;

    for (size_t i = 0; i < cmd.units.size(); ++i) {
      Engine::Core::EntityID const unit_id = cmd.units[i];

      Engine::Core::EntityID target_id = 0;
      float move_x = 0.0F;
      float move_y = 0.0F;
      float move_z = 0.0F;

      if (cmd.type == AICommandType::AttackTarget) {
        target_id = cmd.target_id;
      } else if (cmd.type == AICommandType::MoveUnits) {

        if (i < cmd.moveTargetX.size()) {
          move_x = cmd.moveTargetX[i];
          move_y = cmd.moveTargetY[i];
          move_z = cmd.moveTargetZ[i];
        }
      }

      if (!isDuplicate(unit_id, cmd.type, target_id, move_x, move_y, move_z,
                       currentTime)) {
        valid_units.push_back(unit_id);
      } else {
        blocked_count++;
      }
    }

    if (blocked_count > 0) {
      continue;
    }

    if (!valid_units.empty()) {
      AICommand filtered_cmd = cmd;
      filtered_cmd.units = valid_units;

      if (cmd.type == AICommandType::MoveUnits) {
        std::vector<float> new_target_x;
        std::vector<float> new_target_y;
        std::vector<float> new_target_z;
        new_target_x.reserve(valid_units.size());
        new_target_y.reserve(valid_units.size());
        new_target_z.reserve(valid_units.size());

        for (size_t i = 0; i < cmd.units.size(); ++i) {

          if (std::find(valid_units.begin(), valid_units.end(), cmd.units[i]) !=
              valid_units.end()) {
            if (i < cmd.moveTargetX.size()) {
              new_target_x.push_back(cmd.moveTargetX[i]);
              new_target_y.push_back(cmd.moveTargetY[i]);
              new_target_z.push_back(cmd.moveTargetZ[i]);
            }
          }
        }

        filtered_cmd.moveTargetX = std::move(new_target_x);
        filtered_cmd.moveTargetY = std::move(new_target_y);
        filtered_cmd.moveTargetZ = std::move(new_target_z);
      }

      filtered.push_back(std::move(filtered_cmd));

      recordCommand(filtered.back(), currentTime);
    }
  }

  return filtered;
}

void AICommandFilter::update(float currentTime) { cleanupHistory(currentTime); }

void AICommandFilter::reset() { m_history.clear(); }

auto AICommandFilter::isDuplicate(Engine::Core::EntityID unit_id,
                                  AICommandType type,
                                  Engine::Core::EntityID target_id,
                                  float move_x, float move_y, float move_z,
                                  float currentTime) const -> bool {
  for (const auto &entry : m_history) {
    if (entry.isSimilarTo(type, unit_id, target_id, move_x, move_y, move_z,
                          currentTime, m_cooldownPeriod)) {
      return true;
    }
  }
  return false;
}

void AICommandFilter::recordCommand(const AICommand &cmd, float currentTime) {
  for (size_t i = 0; i < cmd.units.size(); ++i) {
    CommandHistory entry{};
    entry.unit_id = cmd.units[i];
    entry.type = cmd.type;
    entry.issuedTime = currentTime;

    if (cmd.type == AICommandType::AttackTarget) {
      entry.target_id = cmd.target_id;
      entry.moveTargetX = 0.0F;
      entry.moveTargetY = 0.0F;
      entry.moveTargetZ = 0.0F;
    } else if (cmd.type == AICommandType::MoveUnits) {
      entry.target_id = 0;
      if (i < cmd.moveTargetX.size()) {
        entry.moveTargetX = cmd.moveTargetX[i];
        entry.moveTargetY = cmd.moveTargetY[i];
        entry.moveTargetZ = cmd.moveTargetZ[i];
      }
    } else {

      entry.target_id = 0;
      entry.moveTargetX = 0.0F;
      entry.moveTargetY = 0.0F;
      entry.moveTargetZ = 0.0F;
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

auto AICommandFilter::CommandHistory::isSimilarTo(
    const AICommandType &cmdType, Engine::Core::EntityID unit,
    Engine::Core::EntityID target, float x, float y, float z, float currentTime,
    float cooldown) const -> bool {

  if (unit_id != unit) {
    return false;
  }

  if (type != cmdType) {
    return false;
  }

  if ((currentTime - issuedTime) > cooldown) {
    return false;
  }

  switch (cmdType) {
  case AICommandType::AttackTarget:

    return (target_id == target);

  case AICommandType::MoveUnits: {

    const float dx = moveTargetX - x;
    const float dy = moveTargetY - y;
    const float dz = moveTargetZ - z;
    const float dist_sq = dx * dx + dy * dy + dz * dz;
    const float threshold = 3.0F * 3.0F;
    return dist_sq < threshold;
  }

  case AICommandType::StartProduction:

    return true;

  default:
    return false;
  }
}

} // namespace Game::Systems::AI
