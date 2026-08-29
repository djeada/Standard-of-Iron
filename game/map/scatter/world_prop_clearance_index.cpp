#include "game/map/scatter/world_prop_clearance_index.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "game/map/terrain_service.h"

namespace Render::Ground {

auto WorldPropClearanceIndex::cell_key(int cell_x, int cell_z) -> std::uint64_t {
  const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x));
  const auto z = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_z));
  return (x << 32U) | z;
}

void WorldPropClearanceIndex::rebuild(const std::vector<Game::Map::WorldProp>& props,
                                      float cell_size) {
  m_bodies.clear();
  m_cells.clear();
  m_max_radius = 0.0F;
  m_cell_size = std::max(cell_size, 0.5F);

  m_bodies.reserve(props.size());
  for (const auto& prop : props) {
    if (!Game::Map::is_solid_world_prop_type(prop.type)) {
      continue;
    }
    const float bounding =
        Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale);
    if (bounding <= 0.0F) {
      continue;
    }
    m_bodies.push_back(
        {prop.type, prop.x, prop.z, prop.scale, prop.rotation, bounding});
    m_max_radius = std::max(m_max_radius, bounding);
  }

  for (std::uint32_t index = 0; index < m_bodies.size(); ++index) {
    const Body& body = m_bodies[index];
    const int min_x =
        static_cast<int>(std::floor((body.x - body.bounding_radius) / m_cell_size));
    const int max_x =
        static_cast<int>(std::floor((body.x + body.bounding_radius) / m_cell_size));
    const int min_z =
        static_cast<int>(std::floor((body.z - body.bounding_radius) / m_cell_size));
    const int max_z =
        static_cast<int>(std::floor((body.z + body.bounding_radius) / m_cell_size));
    for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
      for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
        m_cells[cell_key(cell_x, cell_z)].push_back(index);
      }
    }
  }
}

auto WorldPropClearanceIndex::overlaps(float world_x,
                                       float world_z,
                                       float radius) const -> bool {
  if (m_bodies.empty()) {
    return false;
  }

  const float reach = std::max(radius, 0.0F);
  const int min_x = static_cast<int>(std::floor((world_x - reach) / m_cell_size));
  const int max_x = static_cast<int>(std::floor((world_x + reach) / m_cell_size));
  const int min_z = static_cast<int>(std::floor((world_z - reach) / m_cell_size));
  const int max_z = static_cast<int>(std::floor((world_z + reach) / m_cell_size));

  for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
    for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
      const auto found = m_cells.find(cell_key(cell_x, cell_z));
      if (found == m_cells.end()) {
        continue;
      }
      for (const std::uint32_t index : found->second) {
        const Body& body = m_bodies[index];
        const float dx = world_x - body.x;
        const float dz = world_z - body.z;
        const float limit = body.bounding_radius + reach;
        if ((dx * dx) + (dz * dz) >= limit * limit) {
          continue;
        }
        if (Game::Map::world_prop_overlap_depth(body.type,
                                                body.scale,
                                                body.x,
                                                body.z,
                                                body.rotation,
                                                world_x,
                                                world_z,
                                                reach) > 0.0F) {
          return true;
        }
      }
    }
  }
  return false;
}

auto shared_world_prop_clearance_index() -> const WorldPropClearanceIndex& {
  static std::mutex mutex;
  static WorldPropClearanceIndex index;
  static std::uint64_t cached_revision = 0;
  static bool built = false;

  auto& terrain_service = Game::Map::TerrainService::instance();
  const std::uint64_t revision = terrain_service.world_props_revision();

  const std::lock_guard<std::mutex> guard(mutex);
  if (!built || revision != cached_revision) {
    const auto* height_map = terrain_service.get_height_map();
    const float tile_size = height_map != nullptr ? height_map->get_tile_size() : 1.0F;
    index.rebuild(terrain_service.world_props(), std::max(4.0F * tile_size, 4.0F));
    cached_revision = revision;
    built = true;
  }
  return index;
}

} // namespace Render::Ground
