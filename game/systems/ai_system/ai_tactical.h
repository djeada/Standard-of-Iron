#pragma once

#include "ai_types.h"
#include <vector>

namespace Game::Systems::AI {

class TacticalUtils {
public:
  struct EngagementAssessment {
    bool shouldEngage = false;
    float forceRatio = 0.0f;
    float confidenceLevel = 0.0f;
    int friendlyCount = 0;
    int enemyCount = 0;
    float avgFriendlyHealth = 1.0f;
    float avgEnemyHealth = 1.0f;
  };

  struct TargetScore {
    Engine::Core::EntityID targetId = 0;
    float score = 0.0f;
    float distanceToGroup = 0.0f;
    bool isLowHealth = false;
    bool isIsolated = false;
  };

  static EngagementAssessment
  assessEngagement(const std::vector<const EntitySnapshot *> &friendlies,
                   const std::vector<const ContactSnapshot *> &enemies,
                   float minForceRatio = 0.8f);

  static TargetScore
  selectFocusFireTarget(const std::vector<const EntitySnapshot *> &attackers,
                        const std::vector<const ContactSnapshot *> &enemies,
                        float groupCenterX, float groupCenterY,
                        float groupCenterZ, const AIContext &context,
                        Engine::Core::EntityID currentTarget = 0);

  static float
  calculateForceStrength(const std::vector<const EntitySnapshot *> &units);

  static float
  calculateForceStrength(const std::vector<const ContactSnapshot *> &units);

  static bool
  isTargetIsolated(const ContactSnapshot &target,
                   const std::vector<const ContactSnapshot *> &allEnemies,
                   float isolationRadius = 8.0f);

  static float
  getUnitTypePriority(const std::string &unitType,
                      const Game::Systems::Nation *nation = nullptr);
};

} // namespace Game::Systems::AI
