#include "game/map/scatter/world_prop_clearance_index.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <vector>

#include "game/map/terrain_service.h"

namespace Game::Map {

auto WorldPropClearanceIndex::cell_key(int cell_x, int cell_z) -> std::uint64_t {
  const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x));
  const auto z = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_z));
  return (x << 32U) | z;
}

void WorldPropClearanceIndex::rebuild(const std::vector<WorldProp>& props,
                                      float cell_size) {
  m_bodies.clear();
  m_cells.clear();
  m_max_radius = 0.0F;
  m_cell_size = std::max(cell_size, 0.5F);

  m_bodies.reserve(props.size());
  for (const auto& prop : props) {
    if (!is_solid_world_prop_type(prop.type)) {
      continue;
    }
    const float bounding = world_prop_ground_bounding_radius(prop.type, prop.scale);
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

template <typename Visit>
void WorldPropClearanceIndex::for_each_candidate(float world_x,
                                                 float world_z,
                                                 float reach,
                                                 Visit&& visit) const {
  if (m_bodies.empty()) {
    return;
  }
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
        if ((dx * dx) + (dz * dz) < limit * limit && visit(body)) {
          return;
        }
      }
    }
  }
}

auto WorldPropClearanceIndex::overlaps(float world_x,
                                       float world_z,
                                       float radius) const -> bool {
  const float reach = std::max(radius, 0.0F);
  bool hit = false;
  for_each_candidate(world_x, world_z, reach, [&](const Body& body) {
    hit = world_prop_overlap_depth(body.type,
                                   body.scale,
                                   body.x,
                                   body.z,
                                   body.rotation,
                                   world_x,
                                   world_z,
                                   reach) > 0.0F;
    return hit;
  });
  return hit;
}

auto WorldPropClearanceIndex::push_out(float& world_x,
                                       float& world_z,
                                       float radius) const -> bool {
  const float reach = std::max(radius, 0.0F);
  bool moved = false;
  for_each_candidate(world_x, world_z, reach, [&](const Body& body) {
    const WorldPropHalfExtents extents =
        world_prop_ground_half_extents(body.type, body.scale);
    const float cosine = std::cos(body.rotation);
    const float sine = std::sin(body.rotation);
    const float dx = world_x - body.x;
    const float dz = world_z - body.z;
    const float local_x = (cosine * dx) + (sine * dz);
    const float local_z = (-sine * dx) + (cosine * dz);
    const float reach_x = extents.x + reach;
    const float reach_z = extents.z + reach;
    const float gap_x = reach_x - std::abs(local_x);
    const float gap_z = reach_z - std::abs(local_z);
    if (gap_x <= 0.0F || gap_z <= 0.0F) {
      return false;
    }
    float out_x = local_x;
    float out_z = local_z;
    if (gap_x < gap_z) {
      out_x = std::copysign(reach_x, local_x == 0.0F ? 1.0F : local_x);
    } else {
      out_z = std::copysign(reach_z, local_z == 0.0F ? 1.0F : local_z);
    }
    world_x = body.x + (cosine * out_x) - (sine * out_z);
    world_z = body.z + (sine * out_x) + (cosine * out_z);
    moved = true;
    return false;
  });
  return moved;
}

auto shared_world_prop_clearance_index()
    -> std::shared_ptr<const WorldPropClearanceIndex> {
  static std::mutex mutex;
  static std::shared_ptr<const WorldPropClearanceIndex> index;
  static std::uint64_t cached_revision = 0;

  auto& terrain_service = TerrainService::instance();
  const std::uint64_t revision = terrain_service.world_props_revision();

  const std::lock_guard<std::mutex> guard(mutex);
  if (index == nullptr || revision != cached_revision) {
    const auto* height_map = terrain_service.get_height_map();
    const float tile_size = height_map != nullptr ? height_map->get_tile_size() : 1.0F;

    std::vector<WorldProp> in_world_space = terrain_service.world_props();
    for (auto& prop : in_world_space) {
      const auto [world_x, world_z] = terrain_service.world_prop_world_xz(prop);
      prop.x = world_x;
      prop.z = world_z;
    }
    auto rebuilt = std::make_shared<WorldPropClearanceIndex>();
    rebuilt->rebuild(in_world_space, std::max(4.0F * tile_size, 4.0F));
    index = std::move(rebuilt);
    cached_revision = revision;
  }
  return index;
}

} // namespace Game::Map
