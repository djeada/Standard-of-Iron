#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "game/map/map_definition.h"

namespace Game::Map {

class WorldPropClearanceIndex {
public:
  void rebuild(const std::vector<WorldProp>& props, float cell_size);

  [[nodiscard]] auto overlaps(float world_x, float world_z, float radius) const -> bool;

  [[nodiscard]] auto
  anything_near(float world_x, float world_z, float reach) const -> bool;

  void gather_near(float world_x,
                   float world_z,
                   float reach,
                   WorldPropClearanceIndex& nearby) const;

  [[nodiscard]] auto
  overlap_depth(float world_x, float world_z, float radius) const -> float;

  auto push_out(float& world_x, float& world_z, float radius) const -> bool;

  struct Point {
    float x{0.0F};
    float z{0.0F};
  };

  [[nodiscard]] auto side_step(float world_x,
                               float world_z,
                               float heading_x,
                               float heading_z,
                               float radius,
                               float lookahead) const -> Point;

  [[nodiscard]] auto way_round(float from_x,
                               float from_z,
                               float to_x,
                               float to_z,
                               float radius) const -> std::optional<Point>;

  [[nodiscard]] auto empty() const -> bool { return m_bodies.empty(); }

  [[nodiscard]] auto body_count() const -> std::size_t { return m_bodies.size(); }

  [[nodiscard]] auto max_radius() const -> float { return m_max_radius; }

private:
  struct Body {
    WorldProp::Type type{};
    float x = 0.0F;
    float z = 0.0F;
    float scale = 1.0F;
    float rotation = 0.0F;
    float bounding_radius = 0.0F;
    float cosine = 1.0F;
    float sine = 0.0F;
    float half_x = 0.0F;
    float half_z = 0.0F;
  };

  struct CellHash {
    [[nodiscard]] auto operator()(std::uint64_t key) const noexcept -> std::size_t {
      return static_cast<std::size_t>(key * 0x9E3779B97F4A7C15ULL);
    }
  };

  [[nodiscard]] static auto cell_key(int cell_x, int cell_z) -> std::uint64_t;

  template <typename Visit>
  void
  for_each_candidate(float world_x, float world_z, float reach, Visit&& visit) const;

  std::vector<Body> m_bodies;
  std::unordered_map<std::uint64_t, std::vector<std::uint32_t>, CellHash> m_cells;
  std::unordered_set<std::uint64_t, CellHash> m_districts;
  float m_cell_size = 4.0F;
  float m_max_radius = 0.0F;
  bool m_listed = false;
};

class TerrainService;

[[nodiscard]] auto build_world_prop_clearance_index(const TerrainService& terrain)
    -> std::shared_ptr<const WorldPropClearanceIndex>;

[[nodiscard]] auto
shared_world_prop_clearance_index() -> std::shared_ptr<const WorldPropClearanceIndex>;

} // namespace Game::Map
