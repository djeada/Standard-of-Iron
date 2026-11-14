#include "game/map/scatter/world_prop_clearance_index.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "game/map/terrain_service.h"
#include "game/util/planar_math.h"

namespace Game::Map {

namespace {
constexpr int k_push_out_passes = 4;
constexpr float k_side_step_band = 1.2F;
constexpr float k_way_round_skin = 0.02F;
constexpr float k_way_round_margin = 0.06F;
constexpr float k_district_size = 16.0F;

[[nodiscard]] auto district_of(float coordinate) -> int {
  return static_cast<int>(std::floor(coordinate / k_district_size));
}
} // namespace

auto WorldPropClearanceIndex::cell_key(int cell_x, int cell_z) -> std::uint64_t {
  const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x));
  const auto z = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_z));
  return (x << 32U) | z;
}

void WorldPropClearanceIndex::rebuild(const std::vector<WorldProp>& props,
                                      float cell_size) {
  m_bodies.clear();
  m_cells.clear();
  m_districts.clear();
  m_listed = false;
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
    const WorldPropHalfExtents extents =
        world_prop_ground_half_extents(prop.type, prop.scale);
    m_bodies.push_back({prop.type,
                        prop.x,
                        prop.z,
                        prop.scale,
                        prop.rotation,
                        bounding,
                        std::cos(prop.rotation),
                        std::sin(prop.rotation),
                        extents.x,
                        extents.z});
    m_max_radius = std::max(m_max_radius, bounding);
    for (int district_z = district_of(prop.z - bounding);
         district_z <= district_of(prop.z + bounding);
         ++district_z) {
      for (int district_x = district_of(prop.x - bounding);
           district_x <= district_of(prop.x + bounding);
           ++district_x) {
        m_districts.insert(cell_key(district_x, district_z));
      }
    }
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
  if (m_listed) {
    for (const Body& body : m_bodies) {
      const float dx = world_x - body.x;
      const float dz = world_z - body.z;
      const float limit = body.bounding_radius + reach;
      if ((dx * dx) + (dz * dz) < limit * limit && visit(body)) {
        return;
      }
    }
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

auto WorldPropClearanceIndex::anything_near(float world_x,
                                            float world_z,
                                            float reach) const -> bool {
  if (m_bodies.empty()) {
    return false;
  }
  for (int district_z = district_of(world_z - reach);
       district_z <= district_of(world_z + reach);
       ++district_z) {
    for (int district_x = district_of(world_x - reach);
         district_x <= district_of(world_x + reach);
         ++district_x) {
      if (m_districts.contains(cell_key(district_x, district_z))) {
        return true;
      }
    }
  }
  return false;
}

void WorldPropClearanceIndex::gather_near(float world_x,
                                          float world_z,
                                          float reach,
                                          WorldPropClearanceIndex& nearby) const {
  nearby.m_bodies.clear();
  nearby.m_listed = true;
  nearby.m_max_radius = m_max_radius;
  if (m_bodies.empty()) {
    return;
  }
  if (m_listed) {
    for_each_candidate(world_x, world_z, reach, [&nearby](const Body& body) {
      nearby.m_bodies.push_back(body);
      return false;
    });
    return;
  }
  thread_local std::vector<std::uint32_t> found;
  found.clear();
  const int min_x = static_cast<int>(std::floor((world_x - reach) / m_cell_size));
  const int max_x = static_cast<int>(std::floor((world_x + reach) / m_cell_size));
  const int min_z = static_cast<int>(std::floor((world_z - reach) / m_cell_size));
  const int max_z = static_cast<int>(std::floor((world_z + reach) / m_cell_size));
  for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
    for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
      const auto cell = m_cells.find(cell_key(cell_x, cell_z));
      if (cell == m_cells.end()) {
        continue;
      }
      for (const std::uint32_t index : cell->second) {
        const Body& body = m_bodies[index];
        const float dx = world_x - body.x;
        const float dz = world_z - body.z;
        const float limit = body.bounding_radius + reach;
        if ((dx * dx) + (dz * dz) < limit * limit) {
          found.push_back(index);
        }
      }
    }
  }
  std::sort(found.begin(), found.end());
  found.erase(std::unique(found.begin(), found.end()), found.end());
  for (const std::uint32_t index : found) {
    nearby.m_bodies.push_back(m_bodies[index]);
  }
}

auto WorldPropClearanceIndex::overlap_depth(float world_x,
                                            float world_z,
                                            float radius) const -> float {
  const float reach = std::max(radius, 0.0F);
  float deepest = 0.0F;
  for_each_candidate(world_x, world_z, reach, [&](const Body& body) {
    deepest = std::max(deepest,
                       world_prop_overlap_depth(body.type,
                                                body.scale,
                                                body.x,
                                                body.z,
                                                body.rotation,
                                                world_x,
                                                world_z,
                                                reach));
    return false;
  });
  return deepest;
}

auto WorldPropClearanceIndex::push_out(float& world_x,
                                       float& world_z,
                                       float radius) const -> bool {
  const float reach = std::max(radius, 0.0F);
  bool moved = false;
  bool pass_moved = true;
  for (int pass = 0; pass < k_push_out_passes && pass_moved; ++pass) {
    pass_moved = false;
    for_each_candidate(world_x, world_z, reach, [&](const Body& body) {
      const float cosine = body.cosine;
      const float sine = body.sine;
      const float dx = world_x - body.x;
      const float dz = world_z - body.z;
      const float local_x = (cosine * dx) + (sine * dz);
      const float local_z = (-sine * dx) + (cosine * dz);
      const float reach_x = body.half_x + reach;
      const float reach_z = body.half_z + reach;
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
      pass_moved = true;
      return false;
    });
  }
  return moved;
}

auto WorldPropClearanceIndex::side_step(float world_x,
                                        float world_z,
                                        float heading_x,
                                        float heading_z,
                                        float radius,
                                        float lookahead) const -> Point {
  float const heading = std::hypot(heading_x, heading_z);
  if (m_bodies.empty() || heading < 1.0e-4F || lookahead <= 0.0F) {
    return {};
  }
  float const along_x = heading_x / heading;
  float const along_z = heading_z / heading;
  float const across_x = along_z;
  float const across_z = -along_x;
  float const clearance = std::max(radius, 0.0F);
  float shift = 0.0F;
  for_each_candidate(
      world_x,
      world_z,
      clearance + lookahead + k_side_step_band,
      [&](const Body& body) {
        float const cosine = body.cosine;
        float const sine = body.sine;
        float const half_along =
            std::abs(body.half_x * (cosine * along_x + sine * along_z)) +
            std::abs(body.half_z * (-sine * along_x + cosine * along_z)) + clearance;
        float const half_across =
            std::abs(body.half_x * (cosine * across_x + sine * across_z)) +
            std::abs(body.half_z * (-sine * across_x + cosine * across_z)) + clearance;
        float const ahead = (body.x - world_x) * along_x + (body.z - world_z) * along_z;
        float const beside =
            (world_x - body.x) * across_x + (world_z - body.z) * across_z;
        float const gap = std::abs(ahead) - half_along;
        float const reach = half_across + k_side_step_band;
        if (gap >= lookahead || std::abs(beside) >= reach) {
          return false;
        }
        float const nearness = std::clamp(1.0F - gap / lookahead, 0.0F, 1.0F);
        float const ease = nearness * nearness * (3.0F - 2.0F * nearness);
        float const squeezed =
            half_across + std::abs(beside) * k_side_step_band / reach;
        float const wanted = (beside < 0.0F ? -squeezed : squeezed) - beside;
        if (std::abs(wanted * ease) > std::abs(shift)) {
          shift = wanted * ease;
        }
        return false;
      });
  return {across_x * shift, across_z * shift};
}

auto WorldPropClearanceIndex::way_round(float from_x,
                                        float from_z,
                                        float to_x,
                                        float to_z,
                                        float radius) const -> std::optional<Point> {
  using Game::Systems::PlanarPoint;
  if (m_bodies.empty()) {
    return std::nullopt;
  }
  float const clearance = std::max(radius, 0.0F);
  float const span = std::hypot(to_x - from_x, to_z - from_z);
  if (span < 1.0e-3F) {
    return std::nullopt;
  }
  struct Blocker {
    const Body* body{nullptr};
    PlanarPoint from;
    PlanarPoint to;
    float half_x{0.0F};
    float half_z{0.0F};
    float entry{2.0F};
  } blocker;
  for_each_candidate((from_x + to_x) * 0.5F,
                     (from_z + to_z) * 0.5F,
                     span * 0.5F + clearance,
                     [&](const Body& body) {
                       float const cosine = body.cosine;
                       float const sine = body.sine;
                       auto const to_local = [&](float x, float z) {
                         float const dx = x - body.x;
                         float const dz = z - body.z;
                         return PlanarPoint{(cosine * dx) + (sine * dz),
                                            (-sine * dx) + (cosine * dz)};
                       };
                       PlanarPoint const a = to_local(from_x, from_z);
                       PlanarPoint const b = to_local(to_x, to_z);
                       float const half_x = body.half_x + clearance;
                       float const half_z = body.half_z + clearance;
                       if (std::abs(a.x) < half_x - k_way_round_skin &&
                           std::abs(a.z) < half_z - k_way_round_skin) {
                         return false;
                       }
                       auto const entry = Game::Systems::segment_box_entry(
                           a, b, half_x - k_way_round_skin, half_z - k_way_round_skin);
                       if (entry.has_value() && *entry < blocker.entry) {
                         blocker = {&body, a, b, half_x, half_z, *entry};
                       }
                       return false;
                     });
  if (blocker.body == nullptr) {
    return std::nullopt;
  }
  auto const corner =
      Game::Systems::box_detour_corner(blocker.from,
                                       blocker.to,
                                       blocker.half_x,
                                       blocker.half_z,
                                       k_way_round_margin,
                                       [](PlanarPoint) { return true; });
  if (!corner.has_value()) {
    return std::nullopt;
  }
  float const cosine = blocker.body->cosine;
  float const sine = blocker.body->sine;
  return Point{blocker.body->x + (cosine * corner->x) - (sine * corner->z),
               blocker.body->z + (sine * corner->x) + (cosine * corner->z)};
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
