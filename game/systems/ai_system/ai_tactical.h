#pragma once

#include "ai_types.h"
#include <vector>

namespace Game::Systems::AI {

class TacticalUtils {
public:
  struct EngagementAssessment {
    bool shouldEngage = false;
    float forceRatio = 0.0F;
    float confidenceLevel = 0.0F;
    int friendlyCount = 0;
    int enemyCount = 0;
    float avgFriendlyHealth = 1.0F;
    float avgEnemyHealth = 1.0F;
  };

  struct TargetScore {
    Engine::Core::EntityID target_id = 0;
    float score = 0.0F;
    float distanceToGroup = 0.0F;
    bool isLowHealth = false;
    bool isIsolated = false;
  };

  static auto
  assessEngagement(const std::vector<const EntitySnapshot *> &friendlies,
                   const std::vector<const ContactSnapshot *> &enemies,
                   float minForceRatio = 0.8F) -> EngagementAssessment;

  static auto selectFocusFireTarget(
      const std::vector<const EntitySnapshot *> &attackers,
      const std::vector<const ContactSnapshot *> &enemies, float group_center_x,
      float group_center_y, float group_center_z, const AIContext &context,
      Engine::Core::EntityID currentTarget = 0) -> TargetScore;

  static auto calculateForceStrength(
      const std::vector<const EntitySnapshot *> &units) -> float;

  static auto calculateForceStrength(
      const std::vector<const ContactSnapshot *> &units) -> float;

  static auto
  isTargetIsolated(const ContactSnapshot &target,
                   const std::vector<const ContactSnapshot *> &allEnemies,
                   float isolationRadius = 8.0F) -> bool;

  static auto
  getUnitTypePriority(const std::string &unit_type,
                      const Game::Systems::Nation *nation = nullptr) -> float;
};

} // namespace Game::Systems::AI
