#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/core/nav_profile.h"
#include "gate_service.h"
#include "map/forest_outline.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "pathfinding.h"
#include "systems/building_collision_registry.h"

namespace Game::Systems {

auto Pathfinding::clearance_penalty(int x, int y) const -> int {
  auto const index = static_cast<std::size_t>(to_index(x, y));
  if (index >= m_clearance_penalty.size()) {
    return 0;
  }
  return m_clearance_penalty[index];
}

void Pathfinding::rebuild_elevation(int min_x, int max_x, int min_z, int max_z) {
  auto const total =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  if (m_cell_height.size() != total) {
    m_cell_height.assign(total, 0.0F);
  }

  auto& terrain_service = *m_terrain;
  const auto* height_map =
      terrain_service.is_initialized() ? terrain_service.get_height_map() : nullptr;
  if (height_map == nullptr) {
    return;
  }

  if (!clamp_to_grid(min_x, max_x, min_z, max_z)) {
    return;
  }

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      m_cell_height[static_cast<std::size_t>(to_index(x, z))] =
          height_map->get_height_at_grid(x, z);
    }
  }
}

auto Pathfinding::climb_penalty(int from_index, int to_index) const -> int {
  auto const from = static_cast<std::size_t>(from_index);
  auto const to = static_cast<std::size_t>(to_index);
  if (from >= m_cell_height.size() || to >= m_cell_height.size()) {
    return 0;
  }
  float const rise = std::abs(m_cell_height[to] - m_cell_height[from]);
  if (rise < k_climb_noise_floor_metres * m_grid_cell_size) {
    return 0;
  }
  return std::min(
      k_max_climb_penalty,
      static_cast<int>(std::lround(rise * static_cast<float>(k_climb_cost_per_metre))));
}

void Pathfinding::rebuild_clearance(int min_x, int max_x, int min_z, int max_z) {
  auto const total =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  if (m_clearance_penalty.size() != total) {
    m_clearance_penalty.assign(total, 0);
  }

  if (!clamp_to_grid(min_x, max_x, min_z, max_z)) {
    return;
  }

  for (int z = min_z; z <= max_z; ++z) {
    auto* row = &m_clearance_penalty[static_cast<std::size_t>(to_index(min_x, z))];
    std::fill(row, row + (max_x - min_x + 1), std::uint8_t{0});
  }

  int const pad_min_x = std::max(0, min_x - k_clearance_radius);
  int const pad_max_x = std::min(m_width - 1, max_x + k_clearance_radius);
  int const pad_min_z = std::max(0, min_z - k_clearance_radius);
  int const pad_max_z = std::min(m_height - 1, max_z + k_clearance_radius);
  int const span_x = pad_max_x - pad_min_x + 1;
  int const span_z = pad_max_z - pad_min_z + 1;
  auto const far = static_cast<std::uint8_t>(k_clearance_radius + 1);

  std::vector<std::uint8_t> distance(
      static_cast<std::size_t>(span_x) * static_cast<std::size_t>(span_z), far);
  auto at = [&](int x, int z) -> std::uint8_t& {
    return distance[(static_cast<std::size_t>(z - pad_min_z) *
                     static_cast<std::size_t>(span_x)) +
                    static_cast<std::size_t>(x - pad_min_x)];
  };

  for (int z = pad_min_z; z <= pad_max_z; ++z) {
    for (int x = pad_min_x; x <= pad_max_x; ++x) {
      if (!is_walkable(x, z)) {
        at(x, z) = 0;
      }
    }
  }

  auto relax = [&](std::uint8_t& cell, std::uint8_t neighbour) {
    if (neighbour < far && neighbour + 1 < cell) {
      cell = static_cast<std::uint8_t>(neighbour + 1);
    }
  };
  for (int z = pad_min_z; z <= pad_max_z; ++z) {
    for (int x = pad_min_x; x <= pad_max_x; ++x) {
      std::uint8_t& cell = at(x, z);
      if (z > pad_min_z) {
        relax(cell, at(x, z - 1));
        if (x > pad_min_x) {
          relax(cell, at(x - 1, z - 1));
        }
        if (x < pad_max_x) {
          relax(cell, at(x + 1, z - 1));
        }
      }
      if (x > pad_min_x) {
        relax(cell, at(x - 1, z));
      }
    }
  }
  for (int z = pad_max_z; z >= pad_min_z; --z) {
    for (int x = pad_max_x; x >= pad_min_x; --x) {
      std::uint8_t& cell = at(x, z);
      if (z < pad_max_z) {
        relax(cell, at(x, z + 1));
        if (x < pad_max_x) {
          relax(cell, at(x + 1, z + 1));
        }
        if (x > pad_min_x) {
          relax(cell, at(x - 1, z + 1));
        }
      }
      if (x < pad_max_x) {
        relax(cell, at(x + 1, z));
      }
    }
  }

  auto const& passages = buildings().navigation_passages();

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      int const reach = at(x, z);
      if (reach == 0 || reach > k_clearance_radius) {
        continue;
      }
      if (!passages.empty()) {
        QVector3D const world = grid_to_world({x, z});
        if (std::any_of(passages.begin(), passages.end(), [&](auto const& passage) {
              return passage.source_entity_id != 0U &&
                     std::abs(world.x() - passage.center_x) <= passage.width * 0.5F &&
                     std::abs(world.z() - passage.center_z) <= passage.depth * 0.5F;
            })) {
          continue;
        }
      }
      m_clearance_penalty[static_cast<std::size_t>(to_index(x, z))] =
          static_cast<std::uint8_t>(reach);
    }
  }
}

} // namespace Game::Systems
