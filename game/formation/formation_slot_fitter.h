#pragma once

#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <optional>

#include "army_formation_types.h"
#include "formation_footprint_claims.h"
#include "formation_frame.h"

namespace Game::Systems {
class Pathfinding;
}

namespace Game::Formation::planning {

class SlotTerrainFitter {
public:
  SlotTerrainFitter(float spacing,
                    float gap,
                    float facing,
                    const QVector3D& anchor,
                    bool enabled,
                    std::size_t expected_slots);

  auto fit(const QVector3D& ideal,
           float half_width,
           float half_depth,
           bool heavy,
           SlotStatus& status) -> QVector3D;

private:
  static constexpr int k_wide_cells = 12;
  static constexpr float k_footprint_inset = 0.5F;
  static constexpr int k_max_samples_per_axis = 6;

  [[nodiscard]] auto search_rings(const QVector3D& ideal,
                                  float half_width,
                                  float half_depth,
                                  bool heavy,
                                  float reach) -> std::optional<QVector3D>;

  [[nodiscard]] auto snap_to_nearest_walkable(const QVector3D& ideal,
                                              float half_width,
                                              float half_depth,
                                              bool heavy,
                                              float reach) -> std::optional<QVector3D>;

  [[nodiscard]] auto max_displacement(float half_width,
                                      float half_depth) const -> float;

  [[nodiscard]] auto acceptable(const QVector3D& centre,
                                float half_width,
                                float half_depth,
                                bool heavy) -> bool;

  [[nodiscard]] auto walkable(const QVector3D& point, bool heavy) const -> bool;

  [[nodiscard]] auto footprint_walkable(const QVector3D& centre,
                                        float half_width,
                                        float half_depth,
                                        bool heavy) const -> bool;

  [[nodiscard]] auto connected_to_anchor(const QVector3D& centre, bool heavy) -> bool;

  FootprintClaims m_claims;
  FrameAxes m_axes;
  float m_separation{1.0F};
  float m_gap{0.0F};
  QVector3D m_anchor;
  bool m_enabled{true};
  Game::Systems::Pathfinding* m_pathfinder{nullptr};
  std::optional<std::uint32_t> m_light_anchor_region;
  std::optional<std::uint32_t> m_heavy_anchor_region;
};

} // namespace Game::Formation::planning
