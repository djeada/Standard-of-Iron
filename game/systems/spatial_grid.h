#pragma once

#include "../core/entity.h"
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

class SpatialGrid {
public:
  explicit SpatialGrid(float cell_size = 10.0F);

  void clear();

  void insert(Engine::Core::EntityID entity_id, float x, float z);

  void remove(Engine::Core::EntityID entity_id);

  void update(Engine::Core::EntityID entity_id, float x, float z);

  [[nodiscard]] auto get_entities_in_range(float x, float z, float range) const
      -> std::vector<Engine::Core::EntityID>;

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

      return std::hash<int>()(key.x) ^ (std::hash<int>()(key.z) << 16);
    }
  };

  [[nodiscard]] auto to_cell_key(float x, float z) const -> CellKey;

  float m_cell_size;
  float m_inv_cell_size;

  std::unordered_map<CellKey, std::vector<Engine::Core::EntityID>, CellKeyHash>
      m_cells;

  std::unordered_map<Engine::Core::EntityID, CellKey> m_entity_cells;
};

} // namespace Game::Systems
