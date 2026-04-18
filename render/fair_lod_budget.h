#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Render {

struct LODEntity {
  std::uint32_t entity_id{0};
  float distance_sq{0.0F};
  bool is_selected{false};
  bool is_moving{false};
  bool is_attacking{false};
  std::uint8_t current_lod{0}; // 0=Full, 1=Reduced, 2=Minimal
  std::uint8_t assigned_lod{2}; // Output: assigned LOD for this frame
  std::uint8_t stable_frames{0}; // Frames at current LOD (for hysteresis)
};

struct FairLODConfig {
  std::uint32_t full_lod_budget{200};
  std::uint32_t reduced_lod_budget{500};
  float selection_priority_scale{0.1F};
  float moving_priority_scale{0.5F};
  float attacking_priority_scale{0.3F};
  std::uint8_t hysteresis_frames{8};
};

// Distance-weighted fair LOD budget allocator.
// Replaces greedy first-come-first-served with scored priority system.
class FairLODBudget {
public:
  void configure(const FairLODConfig &config) { m_config = config; }

  // Score each entity and assign LOD based on fair budget allocation.
  // Lower score = higher priority for Full LOD.
  void allocate(std::vector<LODEntity> &entities) {
    if (entities.empty()) return;

    // Score entities
    for (auto &e : entities) {
      e.assigned_lod = 2; // Default to Minimal
    }

    // Build scored index list
    m_scored.resize(entities.size());
    for (std::size_t i = 0; i < entities.size(); ++i) {
      float score = entities[i].distance_sq;
      if (entities[i].is_selected)  score *= m_config.selection_priority_scale;
      if (entities[i].is_moving)    score *= m_config.moving_priority_scale;
      if (entities[i].is_attacking) score *= m_config.attacking_priority_scale;
      m_scored[i] = {static_cast<std::uint32_t>(i), score};
    }

    // Sort by score ascending (best priority first)
    std::sort(m_scored.begin(), m_scored.end(),
              [](const ScoredEntity &a, const ScoredEntity &b) {
                return a.score < b.score;
              });

    // Assign Full LOD to top N
    std::uint32_t full_count = 0;
    std::uint32_t reduced_count = 0;

    for (const auto &se : m_scored) {
      auto &entity = entities[se.index];

      // Hysteresis: keep current LOD if within stable window
      if (entity.stable_frames < m_config.hysteresis_frames &&
          entity.current_lod <= 1) {
        entity.assigned_lod = entity.current_lod;
        if (entity.current_lod == 0) ++full_count;
        else ++reduced_count;
        continue;
      }

      if (full_count < m_config.full_lod_budget) {
        entity.assigned_lod = 0;
        ++full_count;
      } else if (reduced_count < m_config.reduced_lod_budget) {
        entity.assigned_lod = 1;
        ++reduced_count;
      } else {
        entity.assigned_lod = 2;
      }
    }

    // Update stability counters
    for (auto &e : entities) {
      if (e.assigned_lod == e.current_lod) {
        if (e.stable_frames < 255) ++e.stable_frames;
      } else {
        e.stable_frames = 0;
      }
    }
  }

  [[nodiscard]] auto config() const -> const FairLODConfig & { return m_config; }

private:
  struct ScoredEntity {
    std::uint32_t index;
    float score;
  };

  FairLODConfig m_config;
  std::vector<ScoredEntity> m_scored;
};

} // namespace Render
