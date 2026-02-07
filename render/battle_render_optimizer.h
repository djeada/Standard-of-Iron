#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Render {

struct BattleRenderConfig {
  int temporal_culling_threshold = 15;
  int animation_throttle_threshold = 30;
  float animation_throttle_distance = 40.0F;
  int animation_skip_frames = 2;
  bool enabled = true;
};

class BattleRenderOptimizer {
public:
  static auto instance() noexcept -> BattleRenderOptimizer & {
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

  void set_config(const BattleRenderConfig &config) noexcept {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
  }

  [[nodiscard]] auto config() const noexcept -> BattleRenderConfig {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
  }

  [[nodiscard]] auto is_battle_mode() const noexcept -> bool {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config.enabled &&
           m_visible_unit_count.load(std::memory_order_relaxed) >=
               m_config.temporal_culling_threshold;
  }

  [[nodiscard]] auto
  should_render_unit(uint32_t entity_id, bool is_moving, bool is_selected,
                     bool is_hovered) const noexcept -> bool {
    if (!is_battle_mode()) {
      return true;
    }

    if (is_selected || is_hovered || is_moving) {
      return true;
    }

    uint32_t frame = m_frame_counter.load(std::memory_order_relaxed);
    bool render = ((entity_id + frame) % 2) == 0;

    if (!render) {
      m_units_skipped_temporal.fetch_add(1, std::memory_order_relaxed);
    } else {
      m_units_rendered_this_frame.fetch_add(1, std::memory_order_relaxed);
    }

    return render;
  }

  [[nodiscard]] auto
  should_update_animation(uint32_t entity_id, float distance_sq,
                          bool is_selected) const noexcept -> bool {
    BattleRenderConfig cfg;
    {
      std::lock_guard<std::mutex> lock(m_config_mutex);
      cfg = m_config;
    }

    if (!cfg.enabled) {
      return true;
    }

    int visible_count = m_visible_unit_count.load(std::memory_order_relaxed);
    if (visible_count < cfg.animation_throttle_threshold) {
      return true;
    }

    if (is_selected) {
      return true;
    }

    float const throttle_distance_sq =
        cfg.animation_throttle_distance * cfg.animation_throttle_distance;
    if (distance_sq < throttle_distance_sq) {
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
