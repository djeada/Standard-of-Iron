#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

class AIBaseManager {
public:
  static constexpr float k_base_cluster_radius = 22.0F;
  static constexpr float k_base_identity_radius = 26.0F;
  static constexpr float k_base_defend_radius = 30.0F;
  static constexpr float k_forward_base_min_distance = 25.0F;
  static constexpr float k_abandoned_site_radius = 14.0F;
  static constexpr float k_abandoned_site_memory = 180.0F;
  static constexpr float k_outpost_attempt_timeout = 45.0F;
  static constexpr int k_max_outpost_failures = 3;
  static constexpr int k_migration_score_margin = 2;
  static constexpr int k_production_queue_per_base = 5;

  static void update(const AISnapshot& snapshot, AIContext& ctx);

  static void
  note_expansion_order(AIContext& ctx, float game_time, float site_x, float site_z);

  [[nodiscard]] static auto find_base(const AIContext& ctx,
                                      int base_id) -> const AIBase*;

  [[nodiscard]] static auto main_base(const AIContext& ctx) -> const AIBase*;

  [[nodiscard]] static auto forward_base(const AIContext& ctx) -> const AIBase*;

  [[nodiscard]] static auto
  base_for_position(const AIContext& ctx, float x, float z) -> const AIBase*;

  [[nodiscard]] static auto
  base_for_building(const AIContext& ctx,
                    Engine::Core::EntityID building_id) -> const AIBase*;

  [[nodiscard]] static auto
  site_is_abandoned(const AIContext& ctx, float x, float z, float game_time) -> bool;

  [[nodiscard]] static auto role_name(BaseRole role) -> const char*;
};

} // namespace Game::Systems::AI
