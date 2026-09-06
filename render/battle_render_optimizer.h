#pragma once

#include <cstdint>
#include <mutex>

#include "game/core/component_core.h"

namespace Render {

struct BattleRenderConfig {

  int battle_mode_unit_threshold = 15;
  int animation_throttle_threshold = 30;
  float animation_throttle_distance = 40.0F;
  float combat_render_priority_distance = 50.0F;
  float combat_animation_priority_distance = 36.0F;
  int animation_skip_frames = 2;
  bool enabled = true;
};

class BattleRenderOptimizer {
public:
  struct FrameStats {
    int animations_updated = 0;
    int animations_throttled = 0;
  };

  struct FrameSnapshot {
    BattleRenderConfig config{};
    std::uint32_t frame = 0;
    int visible_unit_count = 0;

    [[nodiscard]] auto battle_mode() const noexcept -> bool {
      return config.enabled && visible_unit_count >= config.battle_mode_unit_threshold;
    }

    [[nodiscard]] auto batching_boost() const noexcept -> float {
      if (!config.enabled || visible_unit_count < config.battle_mode_unit_threshold) {
        return 1.0F;
      }
      const float excess_ratio =
          static_cast<float>(visible_unit_count - config.battle_mode_unit_threshold) /
          static_cast<float>(config.battle_mode_unit_threshold);
      return 1.0F + excess_ratio * 0.5F;
    }

    [[nodiscard]] auto
    should_update_animation(std::uint32_t entity_id,
                            float distance_sq,
                            bool is_selected,
                            bool is_combat_active,
                            const Engine::Core::MotionPresentationComponent* motion,
                            FrameStats& stats) const noexcept -> bool {
      const bool update = evaluate_animation_update(
          entity_id, distance_sq, is_selected, is_combat_active, motion);
      if (update) {
        ++stats.animations_updated;
      } else {
        ++stats.animations_throttled;
      }
      return update;
    }

  private:
    [[nodiscard]] auto
    evaluate_animation_update(std::uint32_t entity_id,
                              float distance_sq,
                              bool is_selected,
                              bool is_combat_active,
                              const Engine::Core::MotionPresentationComponent* motion)
        const noexcept -> bool {
      if (!config.enabled || is_selected) {
        return true;
      }
      if (motion != nullptr && motion->has_locomotion()) {
        return true;
      }
      if (visible_unit_count < config.animation_throttle_threshold) {
        return true;
      }
      if (is_combat_active) {
        return true;
      }
      const float priority_distance = config.animation_throttle_distance;
      if (distance_sq < priority_distance * priority_distance) {
        return true;
      }
      return ((entity_id + frame) %
              static_cast<std::uint32_t>(config.animation_skip_frames + 1)) == 0;
    }
  };

  void set_config(const BattleRenderConfig& config) {
    const std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
  }

  [[nodiscard]] auto config() const -> BattleRenderConfig {
    const std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
  }

  void begin_frame() {
    BattleRenderConfig snapshot;
    {
      const std::lock_guard<std::mutex> lock(m_config_mutex);
      snapshot = m_config;
    }

    m_frame = FrameSnapshot{.config = snapshot,
                            .frame = m_frame.frame + 1U,
                            .visible_unit_count = m_frame.visible_unit_count};
    m_stats = {};
  }

  void set_visible_unit_count(int count) noexcept {
    m_frame.visible_unit_count = count;
  }

  [[nodiscard]] auto frame() const noexcept -> const FrameSnapshot& { return m_frame; }

  void commit_frame_stats(const FrameStats& stats) noexcept {
    m_stats.animations_updated += stats.animations_updated;
    m_stats.animations_throttled += stats.animations_throttled;
  }

  [[nodiscard]] auto frame_counter() const noexcept -> std::uint32_t {
    return m_frame.frame;
  }
  [[nodiscard]] auto visible_unit_count() const noexcept -> int {
    return m_frame.visible_unit_count;
  }
  [[nodiscard]] auto is_battle_mode() const noexcept -> bool {
    return m_frame.battle_mode();
  }
  [[nodiscard]] auto get_batching_boost() const noexcept -> float {
    return m_frame.batching_boost();
  }
  [[nodiscard]] auto animations_throttled() const noexcept -> int {
    return m_stats.animations_throttled;
  }
  [[nodiscard]] auto animations_updated() const noexcept -> int {
    return m_stats.animations_updated;
  }

private:
  mutable std::mutex m_config_mutex;
  BattleRenderConfig m_config;

  FrameSnapshot m_frame;
  FrameStats m_stats;
};

} // namespace Render
