#include "spatial_grid.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {

SpatialGrid::SpatialGrid(float cell_size)
    : m_cell_size(cell_size), m_inv_cell_size(1.0F / cell_size) {}

void SpatialGrid::clear() {
  m_cells.clear();
  m_entity_cells.clear();
}

void SpatialGrid::insert(Engine::Core::EntityID entity_id, float x, float z) {
  CellKey const key = to_cell_key(x, z);
  m_cells[key].push_back(entity_id);
  m_entity_cells[entity_id] = key;
}

void SpatialGrid::remove(Engine::Core::EntityID entity_id) {
  auto it = m_entity_cells.find(entity_id);
  if (it == m_entity_cells.end()) {
    return;
  }

  CellKey const key = it->second;
  m_entity_cells.erase(it);

  auto cell_it = m_cells.find(key);
  if (cell_it != m_cells.end()) {
    auto &entities = cell_it->second;
    entities.erase(std::remove(entities.begin(), entities.end(), entity_id),
                   entities.end());
    if (entities.empty()) {
      m_cells.erase(cell_it);
    }
  }
}

void SpatialGrid::update(Engine::Core::EntityID entity_id, float x, float z) {
  CellKey const new_key = to_cell_key(x, z);

  auto it = m_entity_cells.find(entity_id);
  if (it == m_entity_cells.end()) {
    // Entity not in grid, insert it
    insert(entity_id, x, z);
    return;
  }

  if (it->second == new_key) {
    // Same cell, no update needed
    return;
  }

  // Remove from old cell
  CellKey const old_key = it->second;
  auto cell_it = m_cells.find(old_key);
  if (cell_it != m_cells.end()) {
    auto &entities = cell_it->second;
    entities.erase(std::remove(entities.begin(), entities.end(), entity_id),
                   entities.end());
    if (entities.empty()) {
      m_cells.erase(cell_it);
    }
  }

  // Insert into new cell
  m_cells[new_key].push_back(entity_id);
  it->second = new_key;
}

auto SpatialGrid::get_entities_in_range(float x, float z, float range) const
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> result;

  // Calculate the cell range to check
  int const cells_to_check =
      static_cast<int>(std::ceil(range * m_inv_cell_size));
  CellKey const center = to_cell_key(x, z);

  float const range_sq = range * range;

  for (int dx = -cells_to_check; dx <= cells_to_check; ++dx) {
    for (int dz = -cells_to_check; dz <= cells_to_check; ++dz) {
      CellKey const key{center.x + dx, center.z + dz};
      auto it = m_cells.find(key);
      if (it == m_cells.end()) {
        continue;
      }

      // For cells that are not adjacent, we need to verify distance
      // Adjacent cells (within 1 cell) are always included
      if (std::abs(dx) <= 1 && std::abs(dz) <= 1) {
        result.insert(result.end(), it->second.begin(), it->second.end());
      } else {
        // Check cell center distance (rough filter)
        float const cell_center_x =
            (static_cast<float>(key.x) + 0.5F) * m_cell_size;
        float const cell_center_z =
            (static_cast<float>(key.z) + 0.5F) * m_cell_size;
        float const cell_dx = cell_center_x - x;
        float const cell_dz = cell_center_z - z;
        float const cell_dist_sq = cell_dx * cell_dx + cell_dz * cell_dz;

        // Cell diagonal is sqrt(2) * cell_size, add as buffer
        float const cell_buffer = m_cell_size * 1.5F;
        if (cell_dist_sq <= (range + cell_buffer) * (range + cell_buffer)) {
          result.insert(result.end(), it->second.begin(), it->second.end());
        }
      }
    }
  }

  return result;
}

auto SpatialGrid::get_nearby_entities(float x, float z) const
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> result;
  CellKey const center = to_cell_key(x, z);

  // Check the 3x3 grid of cells around the center
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dz = -1; dz <= 1; ++dz) {
      CellKey const key{center.x + dx, center.z + dz};
      auto it = m_cells.find(key);
      if (it != m_cells.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
      }
    }
  }

  return result;
}

auto SpatialGrid::to_cell_key(float x, float z) const -> CellKey {
  return CellKey{static_cast<int>(std::floor(x * m_inv_cell_size)),
                 static_cast<int>(std::floor(z * m_inv_cell_size))};
}

} // namespace Game::Systems
