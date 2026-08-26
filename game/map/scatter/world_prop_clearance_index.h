#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "game/map/map_definition.h"

namespace Render::Ground {

class WorldPropClearanceIndex {
public:
  void rebuild(const std::vector<Game::Map::WorldProp>& props, float cell_size);

  [[nodiscard]] auto overlaps(float world_x, float world_z, float radius) const -> bool;

  [[nodiscard]] auto empty() const -> bool { return m_discs.empty(); }

  [[nodiscard]] auto disc_count() const -> std::size_t { return m_discs.size(); }

  [[nodiscard]] auto max_radius() const -> float { return m_max_radius; }

private:
  struct Disc {
    float x = 0.0F;
    float z = 0.0F;
    float radius = 0.0F;
  };

  struct CellHash {
    [[nodiscard]] auto operator()(std::uint64_t key) const noexcept -> std::size_t {
      return static_cast<std::size_t>(key * 0x9E3779B97F4A7C15ULL);
    }
  };

  [[nodiscard]] static auto cell_key(int cell_x, int cell_z) -> std::uint64_t;

  std::vector<Disc> m_discs;
  std::unordered_map<std::uint64_t, std::vector<std::uint32_t>, CellHash> m_cells;
  float m_cell_size = 4.0F;
  float m_max_radius = 0.0F;
};

[[nodiscard]] auto
shared_world_prop_clearance_index() -> const WorldPropClearanceIndex&;

} // namespace Render::Ground
