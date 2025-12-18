#pragma once

#include "../core/entity.h"
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

/**
 * @brief Spatial hash grid for efficient range queries on entities.
 *
 * This class partitions 2D space into cells and maintains a mapping of
 * entities to cells for O(1) average case lookups of nearby entities.
 */
class SpatialGrid {
public:
  explicit SpatialGrid(float cell_size = 10.0F);

  /**
   * @brief Clear all entities from the grid.
   */
  void clear();

  /**
   * @brief Insert an entity at the given position.
   */
  void insert(Engine::Core::EntityID entity_id, float x, float z);

  /**
   * @brief Remove an entity from the grid.
   */
  void remove(Engine::Core::EntityID entity_id);

  /**
   * @brief Update an entity's position in the grid.
   */
  void update(Engine::Core::EntityID entity_id, float x, float z);

  /**
   * @brief Get all entities within range of the given position.
   * @param x Center X coordinate
   * @param z Center Z coordinate
   * @param range Search radius
   * @return Vector of entity IDs within range
   */
  [[nodiscard]] auto get_entities_in_range(float x, float z, float range) const
      -> std::vector<Engine::Core::EntityID>;

  /**
   * @brief Get all entities in the same cell or adjacent cells.
   * @param x Center X coordinate
   * @param z Center Z coordinate
   * @return Vector of entity IDs in nearby cells
   */
  [[nodiscard]] auto get_nearby_entities(float x, float z) const
      -> std::vector<Engine::Core::EntityID>;

private:
  struct CellKey {
    int x;
    int z;

    auto operator==(const CellKey &other) const -> bool {
      return x == other.x && z == other.z;
    }
  };

  struct CellKeyHash {
    auto operator()(const CellKey &key) const -> std::size_t {
      // Combine hashes using a common technique
      return std::hash<int>()(key.x) ^ (std::hash<int>()(key.z) << 16);
    }
  };

  [[nodiscard]] auto to_cell_key(float x, float z) const -> CellKey;

  float m_cell_size;
  float m_inv_cell_size;

  // Cell -> list of entities in that cell
  std::unordered_map<CellKey, std::vector<Engine::Core::EntityID>, CellKeyHash>
      m_cells;

  // Entity -> which cell it's in (for fast removal/update)
  std::unordered_map<Engine::Core::EntityID, CellKey> m_entity_cells;
};

} // namespace Game::Systems
