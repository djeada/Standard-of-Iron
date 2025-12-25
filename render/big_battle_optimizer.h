#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Render {

/**
 * @brief Big Battle Optimizer
 *
 * This module provides rendering optimization techniques specifically designed
 * for scenes with more than 15 visible units, without relying on LOD (Level of
 * Detail) techniques. These tricks help maintain smooth frame rates during
 * large-scale battles.
 */

// ============================================================================
// Frame Budget Management
// ============================================================================

/**
 * @brief Manages frame time budgets to ensure consistent render performance.
 *
 * During big battles, tracks the time spent on rendering operations and
 * provides hints for when to skip non-essential updates.
 */
class FrameBudgetManager {
public:
  static constexpr float kTargetFrameTimeMs = 16.67F; // ~60 FPS
  static constexpr int kBigBattleThreshold = 15;

  static auto instance() noexcept -> FrameBudgetManager & {
    static FrameBudgetManager inst;
    return inst;
  }

  void begin_frame() noexcept {
    m_frame_start = std::chrono::steady_clock::now();
    ++m_frame_counter;
  }

  [[nodiscard]] auto elapsed_ms() const noexcept -> float {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float, std::milli>(now - m_frame_start)
        .count();
  }

  [[nodiscard]] auto remaining_budget_ms() const noexcept -> float {
    return kTargetFrameTimeMs - elapsed_ms();
  }

  [[nodiscard]] auto is_over_budget() const noexcept -> bool {
    return remaining_budget_ms() < 0.0F;
  }

  void set_visible_unit_count(int count) noexcept { m_visible_units = count; }

  [[nodiscard]] auto is_big_battle() const noexcept -> bool {
    return m_visible_units > kBigBattleThreshold;
  }

  [[nodiscard]] auto frame_counter() const noexcept -> std::uint64_t {
    return m_frame_counter;
  }

private:
  FrameBudgetManager() = default;

  std::chrono::steady_clock::time_point m_frame_start;
  int m_visible_units{0};
  std::uint64_t m_frame_counter{0};
};

// ============================================================================
// Animation Update Throttling
// ============================================================================

/**
 * @brief Throttles animation updates for distant or non-essential units.
 *
 * Instead of updating every unit's animation every frame, this allows
 * staggering updates based on unit ID and distance, reducing CPU load
 * without affecting visual quality for nearby units.
 */
class AnimationThrottler {
public:
  static auto instance() noexcept -> AnimationThrottler & {
    static AnimationThrottler inst;
    return inst;
  }

  /**
   * @brief Determines if a unit should update its animation this frame.
   *
   * @param entity_id The unique entity identifier
   * @param distance_sq Squared distance from camera
   * @param frame_counter Current frame number
   * @return true if the unit should update this frame
   */
  [[nodiscard]] auto should_update_animation(std::uint32_t entity_id,
                                              float distance_sq,
                                              std::uint64_t frame_counter)
      const noexcept -> bool {
    if (!m_enabled) {
      return true;
    }

    // Always update very close units
    if (distance_sq < m_close_distance_sq) {
      return true;
    }

    // For medium distance, update every other frame based on ID
    if (distance_sq < m_medium_distance_sq) {
      return (frame_counter + entity_id) % 2 == 0;
    }

    // For far distance, update every 3rd frame
    if (distance_sq < m_far_distance_sq) {
      return (frame_counter + entity_id) % 3 == 0;
    }

    // Very far units update every 4th frame
    return (frame_counter + entity_id) % 4 == 0;
  }

  void set_enabled(bool enabled) noexcept { m_enabled = enabled; }
  [[nodiscard]] auto is_enabled() const noexcept -> bool { return m_enabled; }

  void set_distance_thresholds(float close, float medium, float far) noexcept {
    m_close_distance_sq = close * close;
    m_medium_distance_sq = medium * medium;
    m_far_distance_sq = far * far;
  }

private:
  AnimationThrottler() = default;

  bool m_enabled{true};
  float m_close_distance_sq{100.0F};  // 10 units
  float m_medium_distance_sq{400.0F}; // 20 units
  float m_far_distance_sq{900.0F};    // 30 units
};

// ============================================================================
// Staggered Rendering Updates
// ============================================================================

/**
 * @brief Provides staggered update scheduling for non-critical visual elements.
 *
 * Elements like particle effects, cloth simulation, and secondary animations
 * can be updated in a staggered manner across multiple frames to distribute
 * the computational load.
 */
class StaggeredUpdateScheduler {
public:
  enum class UpdatePriority : std::uint8_t {
    Critical = 0,   // Selection rings, combat indicators
    High = 1,       // Main unit animations
    Medium = 2,     // Equipment animations, dust effects
    Low = 3,        // Cloth physics, ambient particles
    Background = 4  // Distant vegetation movement
  };

  static auto instance() noexcept -> StaggeredUpdateScheduler & {
    static StaggeredUpdateScheduler inst;
    return inst;
  }

  /**
   * @brief Checks if an update should occur this frame based on priority.
   *
   * @param priority The update priority level
   * @param entity_id Used for staggering within the same priority
   * @param frame_counter Current frame number
   * @return true if the update should be performed
   */
  [[nodiscard]] auto should_update(UpdatePriority priority,
                                   std::uint32_t entity_id,
                                   std::uint64_t frame_counter)
      const noexcept -> bool {
    if (!m_enabled) {
      return true;
    }

    switch (priority) {
    case UpdatePriority::Critical:
      return true;
    case UpdatePriority::High:
      return true;
    case UpdatePriority::Medium:
      return (frame_counter + entity_id) % 2 == 0;
    case UpdatePriority::Low:
      return (frame_counter + entity_id) % 3 == 0;
    case UpdatePriority::Background:
      return (frame_counter + entity_id) % 4 == 0;
    default:
      return true;
    }
  }

  void set_enabled(bool enabled) noexcept { m_enabled = enabled; }

private:
  StaggeredUpdateScheduler() = default;

  bool m_enabled{true};
};

// ============================================================================
// Spatial Coherence Optimizer
// ============================================================================

/**
 * @brief Groups units by spatial proximity for batch rendering.
 *
 * Units that are close together can share certain rendering state,
 * reducing state changes and improving GPU efficiency.
 */
class SpatialCoherenceOptimizer {
public:
  struct UnitCluster {
    float center_x{0.0F};
    float center_z{0.0F};
    float radius{0.0F};
    int unit_count{0};
    std::uint32_t cluster_id{0};
  };

  static constexpr int kMaxClusters = 64;
  static constexpr float kClusterRadius = 10.0F;

  static auto instance() noexcept -> SpatialCoherenceOptimizer & {
    static SpatialCoherenceOptimizer inst;
    return inst;
  }

  void begin_frame() noexcept {
    m_clusters.clear();
    m_cluster_id_counter = 0;
  }

  /**
   * @brief Assigns a unit to a spatial cluster or creates a new one.
   *
   * @param x World X position
   * @param z World Z position
   * @return Cluster ID for the unit
   */
  [[nodiscard]] auto assign_to_cluster(float x, float z) noexcept
      -> std::uint32_t {
    // Find existing nearby cluster
    for (auto &cluster : m_clusters) {
      float const dx = cluster.center_x - x;
      float const dz = cluster.center_z - z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq < kClusterRadius * kClusterRadius) {
        // Update cluster center (running average)
        float const n = static_cast<float>(cluster.unit_count);
        cluster.center_x = (cluster.center_x * n + x) / (n + 1.0F);
        cluster.center_z = (cluster.center_z * n + z) / (n + 1.0F);
        ++cluster.unit_count;
        return cluster.cluster_id;
      }
    }

    // Create new cluster if under limit
    if (m_clusters.size() < kMaxClusters) {
      UnitCluster new_cluster;
      new_cluster.center_x = x;
      new_cluster.center_z = z;
      new_cluster.radius = kClusterRadius;
      new_cluster.unit_count = 1;
      new_cluster.cluster_id = m_cluster_id_counter++;
      m_clusters.push_back(new_cluster);
      return new_cluster.cluster_id;
    }

    // Fallback: assign to nearest cluster if any exist
    if (m_clusters.empty()) {
      return 0;
    }

    std::uint32_t nearest_id = m_clusters[0].cluster_id;
    float min_dist_sq = std::numeric_limits<float>::max();
    for (const auto &cluster : m_clusters) {
      float const dx = cluster.center_x - x;
      float const dz = cluster.center_z - z;
      float const dist_sq = dx * dx + dz * dz;
      if (dist_sq < min_dist_sq) {
        min_dist_sq = dist_sq;
        nearest_id = cluster.cluster_id;
      }
    }
    return nearest_id;
  }

  [[nodiscard]] auto get_clusters() const noexcept
      -> const std::vector<UnitCluster> & {
    return m_clusters;
  }

private:
  SpatialCoherenceOptimizer() { m_clusters.reserve(kMaxClusters); }

  std::vector<UnitCluster> m_clusters;
  std::uint32_t m_cluster_id_counter{0};
};

// ============================================================================
// Draw Call Batcher
// ============================================================================

/**
 * @brief Hints for draw call batching priority.
 *
 * During big battles, prioritizes which units should be rendered with
 * individual draw calls vs batched together.
 */
class DrawCallPrioritizer {
public:
  static auto instance() noexcept -> DrawCallPrioritizer & {
    static DrawCallPrioritizer inst;
    return inst;
  }

  /**
   * @brief Determines render priority for a unit.
   *
   * @param is_selected Whether the unit is selected
   * @param is_hovered Whether the unit is hovered
   * @param is_in_combat Whether the unit is in active combat
   * @param distance_sq Squared distance from camera
   * @return Priority score (higher = render with more detail)
   */
  [[nodiscard]] auto calculate_priority(bool is_selected, bool is_hovered,
                                        bool is_in_combat,
                                        float distance_sq) const noexcept
      -> float {
    float priority = 0.0F;

    if (is_selected) {
      priority += 100.0F;
    }
    if (is_hovered) {
      priority += 50.0F;
    }
    if (is_in_combat) {
      priority += 25.0F;
    }

    // Distance penalty (closer = higher priority)
    float const distance_factor =
        1.0F / (1.0F + distance_sq * 0.001F);
    priority += distance_factor * 20.0F;

    return priority;
  }

  /**
   * @brief Checks if unit should use individual rendering or batching.
   *
   * @param priority Priority score from calculate_priority
   * @return true if unit should be rendered individually
   */
  [[nodiscard]] auto should_render_individually(float priority) const noexcept
      -> bool {
    return priority >= m_individual_render_threshold;
  }

  void set_individual_render_threshold(float threshold) noexcept {
    m_individual_render_threshold = threshold;
  }

private:
  DrawCallPrioritizer() = default;

  float m_individual_render_threshold{40.0F};
};

// ============================================================================
// Temporal Coherence Cache
// ============================================================================

/**
 * @brief Caches frame-to-frame coherent data to avoid redundant calculations.
 *
 * During big battles, many units maintain similar states between frames.
 * This cache helps identify when recalculation is unnecessary.
 */
class TemporalCoherenceCache {
public:
  struct UnitState {
    float position_x{0.0F};
    float position_z{0.0F};
    float facing_angle{0.0F};
    std::uint8_t animation_state{0};
    std::uint64_t last_update_frame{0};
  };

  static constexpr float kPositionEpsilon = 0.01F;
  static constexpr float kAngleEpsilon = 0.1F;
  static constexpr float kHalfCircleDegrees = 180.0F;
  static constexpr float kFullCircleDegrees = 360.0F;

  static auto instance() noexcept -> TemporalCoherenceCache & {
    static TemporalCoherenceCache inst;
    return inst;
  }

  /**
   * @brief Checks if unit state has changed significantly.
   *
   * @param entity_id The entity to check
   * @param x Current X position
   * @param z Current Z position
   * @param angle Current facing angle
   * @param anim_state Current animation state
   * @return true if state has changed and needs update
   */
  [[nodiscard]] auto has_state_changed(std::uint32_t entity_id, float x,
                                        float z, float angle,
                                        std::uint8_t anim_state) noexcept
      -> bool {
    auto it = m_states.find(entity_id);
    if (it == m_states.end()) {
      // New entity, always needs update
      UnitState state;
      state.position_x = x;
      state.position_z = z;
      state.facing_angle = angle;
      state.animation_state = anim_state;
      state.last_update_frame =
          FrameBudgetManager::instance().frame_counter();
      m_states[entity_id] = state;
      return true;
    }

    UnitState &cached = it->second;

    // Check position change
    float const dx = cached.position_x - x;
    float const dz = cached.position_z - z;
    if (dx * dx + dz * dz > kPositionEpsilon * kPositionEpsilon) {
      cached.position_x = x;
      cached.position_z = z;
      cached.last_update_frame =
          FrameBudgetManager::instance().frame_counter();
      return true;
    }

    // Check angle change
    float angle_diff = std::abs(cached.facing_angle - angle);
    if (angle_diff > kHalfCircleDegrees) {
      angle_diff = kFullCircleDegrees - angle_diff;
    }
    if (angle_diff > kAngleEpsilon) {
      cached.facing_angle = angle;
      cached.last_update_frame =
          FrameBudgetManager::instance().frame_counter();
      return true;
    }

    // Check animation state change
    if (cached.animation_state != anim_state) {
      cached.animation_state = anim_state;
      cached.last_update_frame =
          FrameBudgetManager::instance().frame_counter();
      return true;
    }

    return false;
  }

  void remove_entity(std::uint32_t entity_id) noexcept {
    m_states.erase(entity_id);
  }

  void clear() noexcept { m_states.clear(); }

private:
  TemporalCoherenceCache() = default;

  std::unordered_map<std::uint32_t, UnitState> m_states;
};

// ============================================================================
// Render Skip Hints
// ============================================================================

/**
 * @brief Provides hints for when rendering can be safely skipped.
 *
 * Certain units or visual elements can be skipped when frame budget is tight,
 * without significantly impacting visual quality.
 */
class RenderSkipHints {
public:
  static constexpr float kEffectSkipDistanceSq = 2500.0F;   // 50 units squared
  static constexpr float kDustSkipDistanceSq = 900.0F;      // 30 units squared
  static constexpr float kCriticalBudgetMs = 4.0F;
  static constexpr float kComfortableBudgetMs = 8.0F;

  static auto instance() noexcept -> RenderSkipHints & {
    static RenderSkipHints inst;
    return inst;
  }

  /**
   * @brief Checks if a visual element can be skipped this frame.
   *
   * @param element_type Type of visual element (0=unit, 1=effect, 2=dust, etc)
   * @param distance_sq Squared distance from camera
   * @param is_critical Whether element is critical (selected, in combat)
   * @return true if element can be skipped to save frame budget
   */
  [[nodiscard]] auto can_skip(int element_type, float distance_sq,
                               bool is_critical) const noexcept -> bool {
    if (is_critical) {
      return false;
    }

    auto &budget = FrameBudgetManager::instance();

    // Never skip if plenty of budget remains
    if (budget.remaining_budget_ms() > kComfortableBudgetMs) {
      return false;
    }

    // Skip distant effects when low on budget
    if (element_type >= 1 && distance_sq > kEffectSkipDistanceSq) {
      return true;
    }

    // Skip very distant dust when very low on budget
    if (element_type == 2 && distance_sq > kDustSkipDistanceSq &&
        budget.remaining_budget_ms() < kCriticalBudgetMs) {
      return true;
    }

    return false;
  }

private:
  RenderSkipHints() = default;
};

// ============================================================================
// Big Battle Statistics
// ============================================================================

/**
 * @brief Collects rendering statistics during big battles for optimization.
 */
struct BigBattleStats {
  std::uint32_t total_units{0};
  std::uint32_t visible_units{0};
  std::uint32_t culled_units{0};
  std::uint32_t throttled_animations{0};
  std::uint32_t skipped_effects{0};
  std::uint32_t clusters_formed{0};
  float avg_frame_time_ms{0.0F};

  void reset() noexcept {
    total_units = 0;
    visible_units = 0;
    culled_units = 0;
    throttled_animations = 0;
    skipped_effects = 0;
    clusters_formed = 0;
    avg_frame_time_ms = 0.0F;
  }
};

inline auto get_big_battle_stats() noexcept -> BigBattleStats & {
  static BigBattleStats stats;
  return stats;
}

} // namespace Render
