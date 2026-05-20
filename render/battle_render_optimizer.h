#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

#include "../game/core/component.h"

namespace Render {

struct BattleRenderConfig {
  int temporal_culling_threshold = 15;
  int animation_throttle_threshold = 30;
  float animation_throttle_distance = 40.0F;
  float combat_render_priority_distance = 50.0F;
  float combat_animation_priority_distance = 36.0F;
  int animation_skip_frames = 2;
  bool enabled = true;
};

class BattleRenderOptimizer {
public:
  static auto instance() noexcept -> BattleRenderOptimizer& {
    static BattleRenderOptimizer inst;
    return inst;
  }

  void begin_frame() noexcept {
    m_frame_counter.fetch_add(1, std::memory_order_relaxed);
    m_units_rendered_this_frame.store(0, std::memory_order_relaxed);
    m_units_skipped_temporal.store(0, std::memory_order_relaxed);
    m_animations_throttled.store(0, std::memory_order_relaxed);
  }

  void set_visible_unit_count(int count) noexcept {
    m_visible_unit_count.store(count, std::memory_order_relaxed);
  }

  void set_config(const BattleRenderConfig& config) noexcept {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
  }

  [[nodiscard]] auto config() const noexcept -> BattleRenderConfig {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
  }

  [[nodiscard]] auto is_battle_mode() const noexcept -> bool {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config.enabled && m_visible_unit_count.load(std::memory_order_relaxed) >=
                                   m_config.temporal_culling_threshold;
  }

  [[nodiscard]] auto
  should_render_unit(uint32_t entity_id,
                     const Engine::Core::MotionPresentationComponent* motion,
                     bool is_selected,
                     bool is_hovered,
                     bool is_combat_active = false,
                     float distance_sq = 0.0F) const noexcept -> bool {
    (void)entity_id;
    (void)motion;
    (void)is_selected;
    (void)is_hovered;
    (void)is_combat_active;
    (void)distance_sq;
    m_units_rendered_this_frame.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  [[nodiscard]] auto should_update_animation(
      uint32_t entity_id,
      float distance_sq,
      bool is_selected,
      bool is_combat_active,
      const Engine::Core::MotionPresentationComponent* motion) const noexcept {
    BattleRenderConfig cfg;
    {
      std::lock_guard<std::mutex> lock(m_config_mutex);
      cfg = m_config;
    }

    if (!cfg.enabled) {
      return true;
    }

    if (is_selected) {
      return true;
    }

    // Use the finalized motion state directly. Same reasoning as should_render_unit:
    // snapshot_valid may be false during the render window but state retains its
    // last valid value.
    bool const is_moving = motion != nullptr && motion->has_locomotion();
    if (is_moving) {
      return true;
    }

    int visible_count = m_visible_unit_count.load(std::memory_order_relaxed);
    if (visible_count < cfg.animation_throttle_threshold) {
      return true;
    }

    if (is_combat_active) {
      return true;
    }

    float priority_distance = cfg.animation_throttle_distance;

    float const priority_distance_sq = priority_distance * priority_distance;
    if (distance_sq < priority_distance_sq) {
      return true;
    }

    uint32_t frame = m_frame_counter.load(std::memory_order_relaxed);
    bool update = ((entity_id + frame) %
                   static_cast<uint32_t>(cfg.animation_skip_frames + 1)) == 0;

    if (!update) {
      m_animations_throttled.fetch_add(1, std::memory_order_relaxed);
    }

    return update;
  }

  [[nodiscard]] auto get_batching_boost() const noexcept -> float {
    BattleRenderConfig cfg;
    {
      std::lock_guard<std::mutex> lock(m_config_mutex);
      cfg = m_config;
    }

    if (!cfg.enabled) {
      return 1.0F;
    }

    int visible_count = m_visible_unit_count.load(std::memory_order_relaxed);
    if (visible_count < cfg.temporal_culling_threshold) {
      return 1.0F;
    }

    float excess_ratio =
        static_cast<float>(visible_count - cfg.temporal_culling_threshold) /
        static_cast<float>(cfg.temporal_culling_threshold);
    return 1.0F + excess_ratio * 0.5F;
  }

  [[nodiscard]] auto frame_counter() const noexcept -> uint32_t {
    return m_frame_counter.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto units_rendered_this_frame() const noexcept -> int {
    return m_units_rendered_this_frame.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto units_skipped_temporal() const noexcept -> int {
    return m_units_skipped_temporal.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto animations_throttled() const noexcept -> int {
    return m_animations_throttled.load(std::memory_order_relaxed);
  }

  [[nodiscard]] auto visible_unit_count() const noexcept -> int {
    return m_visible_unit_count.load(std::memory_order_relaxed);
  }

private:
  BattleRenderOptimizer() = default;

  mutable std::mutex m_config_mutex;
  BattleRenderConfig m_config;
  std::atomic<uint32_t> m_frame_counter{0};
  std::atomic<int> m_visible_unit_count{0};
  mutable std::atomic<int> m_units_rendered_this_frame{0};
  mutable std::atomic<int> m_units_skipped_temporal{0};
  mutable std::atomic<int> m_animations_throttled{0};
};

} // namespace Render
