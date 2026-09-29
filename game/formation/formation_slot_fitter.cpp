#include "formation_slot_fitter.h"

#include "../systems/navigation/nav_grid.h"
#include "../systems/navigation/pathfinding.h"

namespace Game::Formation::planning {

SlotTerrainFitter::SlotTerrainFitter(float spacing,
                                     float gap,
                                     float facing,
                                     const QVector3D& anchor,
                                     bool enabled,
                                     std::size_t expected_slots)
    : m_claims(std::max(spacing, 0.5F), frame_axes(facing), gap)
    , m_axes(frame_axes(facing))
    , m_separation(spacing)
    , m_gap(gap)
    , m_anchor(anchor)
    , m_enabled(enabled)
    , m_pathfinder(enabled ? Game::Systems::NavGrid::get_pathfinder() : nullptr) {
  m_claims.reserve(expected_slots);
}

auto SlotTerrainFitter::fit(const QVector3D& ideal,
                            float half_width,
                            float half_depth,
                            bool heavy,
                            SlotStatus& status) -> QVector3D {
  if (!m_enabled || acceptable(ideal, half_width, half_depth, heavy)) {
    m_claims.claim(ideal, half_width, half_depth);
    status = SlotStatus::Valid;
    return ideal;
  }

  float const reach = max_displacement(half_width, half_depth);
  auto candidate = search_rings(ideal, half_width, half_depth, heavy, reach);
  if (!candidate) {
    candidate = snap_to_nearest_walkable(ideal, half_width, half_depth, heavy, reach);
  }
  if (!candidate) {
    status = SlotStatus::Blocked;
    return ideal;
  }
  m_claims.claim(*candidate, half_width, half_depth);
  status = SlotStatus::Adjusted;
  return *candidate;
}

auto SlotTerrainFitter::search_rings(const QVector3D& ideal,
                                     float half_width,
                                     float half_depth,
                                     bool heavy,
                                     float reach) -> std::optional<QVector3D> {
  constexpr int k_max_rings = 8;
  constexpr int k_samples = 12;
  float const step =
      std::max(0.75F, (std::min(half_width, half_depth) * 2.0F + m_gap) * 0.5F);
  int const rings =
      std::clamp(static_cast<int>(std::ceil(reach / step)), 1, k_max_rings);
  for (int ring = 1; ring <= rings; ++ring) {
    float const radius = reach * static_cast<float>(ring) / static_cast<float>(rings);
    for (int sample = 0; sample < k_samples; ++sample) {
      int const side = (sample % 2 == 0) ? 1 : -1;
      float const turn = static_cast<float>((sample + 1) / 2) * (2.0F * k_pi) /
                         static_cast<float>(k_samples);
      float const angle = k_pi + static_cast<float>(side) * turn;
      QVector3D const direction =
          m_axes.depth * std::cos(angle) + m_axes.lateral * std::sin(angle);
      QVector3D const candidate(ideal.x() + direction.x() * radius,
                                ideal.y(),
                                ideal.z() + direction.z() * radius);
      if (acceptable(candidate, half_width, half_depth, heavy)) {
        return candidate;
      }
    }
  }
  return std::nullopt;
}

auto SlotTerrainFitter::snap_to_nearest_walkable(
    const QVector3D& ideal, float half_width, float half_depth, bool heavy, float reach)
    -> std::optional<QVector3D> {
  auto const origin = Game::Systems::NavGrid::world_to_grid(ideal.x(), ideal.z());
  auto const nearest =
      Game::Systems::NavGrid::find_nearest_walkable_grid(origin, k_wide_cells);
  if (!nearest) {
    return std::nullopt;
  }
  QVector3D const grounded = Game::Systems::NavGrid::grid_to_world(*nearest);
  QVector3D const candidate(grounded.x(), ideal.y(), grounded.z());
  QVector3D const shift(candidate.x() - ideal.x(), 0.0F, candidate.z() - ideal.z());
  if (shift.length() <= reach && acceptable(candidate, half_width, half_depth, heavy)) {
    return candidate;
  }
  return std::nullopt;
}

auto SlotTerrainFitter::max_displacement(float half_width,
                                         float half_depth) const -> float {
  return std::max(m_separation, std::max(half_width, half_depth) * 2.0F + m_gap);
}

auto SlotTerrainFitter::acceptable(const QVector3D& centre,
                                   float half_width,
                                   float half_depth,
                                   bool heavy) -> bool {
  return m_claims.is_free(centre, half_width, half_depth) &&
         footprint_walkable(centre, half_width, half_depth, heavy) &&
         connected_to_anchor(centre, heavy);
}

auto SlotTerrainFitter::walkable(const QVector3D& point, bool heavy) const -> bool {
  if (m_pathfinder == nullptr) {
    return Game::Systems::NavGrid::is_world_position_walkable(point);
  }
  return m_pathfinder->is_world_position_walkable(
      point,
      heavy ? Game::Systems::Pathfinding::Passability::Heavy
            : Game::Systems::Pathfinding::Passability::Light);
}

auto SlotTerrainFitter::footprint_walkable(const QVector3D& centre,
                                           float half_width,
                                           float half_depth,
                                           bool heavy) const -> bool {
  if (!walkable(centre, heavy)) {
    return false;
  }
  float const sample_step =
      std::max(0.75F, m_pathfinder != nullptr ? m_pathfinder->grid_cell_size() : 1.0F);
  float const reach_x = half_width * k_footprint_inset;
  float const reach_z = half_depth * k_footprint_inset;
  int const across =
      std::clamp(static_cast<int>(std::ceil(reach_x * 2.0F / sample_step)) + 1,
                 2,
                 k_max_samples_per_axis);
  int const along =
      std::clamp(static_cast<int>(std::ceil(reach_z * 2.0F / sample_step)) + 1,
                 2,
                 k_max_samples_per_axis);
  for (int i = 0; i < across; ++i) {
    float const u = -reach_x + (2.0F * reach_x) * static_cast<float>(i) /
                                   static_cast<float>(across - 1);
    for (int j = 0; j < along; ++j) {
      float const v = -reach_z + (2.0F * reach_z) * static_cast<float>(j) /
                                     static_cast<float>(along - 1);
      QVector3D const point = centre + m_axes.lateral * u + m_axes.depth * v;
      if (!walkable(point, heavy)) {
        return false;
      }
    }
  }
  return true;
}

auto SlotTerrainFitter::connected_to_anchor(const QVector3D& centre,
                                            bool heavy) -> bool {
  if (m_pathfinder == nullptr) {
    return true;
  }
  auto const passability = heavy ? Game::Systems::Pathfinding::Passability::Heavy
                                 : Game::Systems::Pathfinding::Passability::Light;
  auto& anchor_region = heavy ? m_heavy_anchor_region : m_light_anchor_region;
  if (!anchor_region.has_value()) {
    anchor_region = m_pathfinder->region_of(
        m_pathfinder->world_to_grid(m_anchor.x(), m_anchor.z()), passability);
  }
  if (*anchor_region == Game::Systems::Pathfinding::k_unreachable_region) {
    return true;
  }
  return m_pathfinder->region_of(m_pathfinder->world_to_grid(centre.x(), centre.z()),
                                 passability) == *anchor_region;
}

} // namespace Game::Formation::planning
