#pragma once

#include "ai_types.h"
#include <vector>

namespace Game::Systems::AI {

class TacticalUtils {
public:
  struct EngagementAssessment {
    bool should_engage = false;
    float force_ratio = 0.0F;
    float confidence_level = 0.0F;
    int friendly_count = 0;
    int enemy_count = 0;
    float avg_friendly_health = 1.0F;
    float avg_enemy_health = 1.0F;
  };

  struct TargetScore {
    Engine::Core::EntityID target_id = 0;
    float score = 0.0F;
    float distance_to_group = 0.0F;
    bool is_low_health = false;
    bool is_isolated = false;
  };

  static auto
  assess_engagement(const std::vector<const EntitySnapshot *> &friendlies,
                   const std::vector<const ContactSnapshot *> &enemies,
                   float min_force_ratio = 0.8F) -> EngagementAssessment;

  static auto select_focus_fire_target(
      const std::vector<const EntitySnapshot *> &attackers,
      const std::vector<const ContactSnapshot *> &enemies, float group_center_x,
      float group_center_y, float group_center_z, const AIContext &context,
      Engine::Core::EntityID currentTarget = 0) -> TargetScore;

  static auto calculate_force_strength(
      const std::vector<const EntitySnapshot *> &units) -> float;

  static auto calculate_force_strength(
      const std::vector<const ContactSnapshot *> &units) -> float;

  static auto
  is_target_isolated(const ContactSnapshot &target,
                   const std::vector<const ContactSnapshot *> &allEnemies,
                   float isolation_radius = 8.0F) -> bool;

  static auto
  get_unit_type_priority(const std::string &unit_type,
                      const Game::Systems::Nation *nation = nullptr) -> float;
};

} // namespace Game::Systems::AI
