#include "camera_obstruction.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "building_collision_registry.h"
#include "building_line_of_sight.h"
#include "game/core/component_gameplay.h"
#include "game/core/wall_walk_geometry.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"

namespace Game::Systems {

namespace {

struct PropOccluder {
  float x{0.0F};
  float z{0.0F};
  float cosine{1.0F};
  float sine{0.0F};
  float half_x{0.0F};
  float half_z{0.0F};
  float top_y{0.0F};
  float bounding_radius{0.0F};
  bool stem{false};
};

constexpr float k_camera_stem_half_extent = 0.30F;

constexpr float k_prop_cell_size = 8.0F;

auto cell_key(int cell_x, int cell_z) -> std::int64_t {
  return (static_cast<std::int64_t>(cell_x) << 32) |
         static_cast<std::int64_t>(static_cast<std::uint32_t>(cell_z));
}

auto cell_coord(float world) -> int {
  return static_cast<int>(std::floor(world / k_prop_cell_size));
}

class PropOccluderIndex {
public:
  void rebuild(const Game::Map::TerrainService& terrain) {
    m_occluders.clear();
    m_cells.clear();
    m_max_bounding_radius = 0.0F;

    auto const& props = terrain.world_props();
    m_occluders.reserve(props.size());
    for (auto const& prop : props) {
      if (!Game::Map::is_solid_world_prop_type(prop.type)) {
        continue;
      }
      float const height = Game::Map::world_prop_occluder_height(prop.type, prop.scale);
      if (height <= 0.0F) {
        continue;
      }
      auto const [world_x, world_z] = terrain.world_prop_world_xz(prop);
      bool const stem = Game::Map::world_prop_blocks_only_its_stem(prop.type);
      auto extents = Game::Map::world_prop_ground_half_extents(prop.type, prop.scale);
      if (stem) {
        extents.x = std::min(extents.x, k_camera_stem_half_extent);
        extents.z = std::min(extents.z, k_camera_stem_half_extent);
      }
      PropOccluder occluder;
      occluder.x = world_x;
      occluder.z = world_z;
      occluder.cosine = std::cos(prop.rotation);
      occluder.sine = std::sin(prop.rotation);
      occluder.half_x = extents.x;
      occluder.half_z = extents.z;
      occluder.top_y =
          terrain.resolve_surface_world_y(world_x, world_z, 0.0F, 0.0F) + height;
      occluder.bounding_radius = std::hypot(extents.x, extents.z);
      occluder.stem = stem;
      m_max_bounding_radius = std::max(m_max_bounding_radius, occluder.bounding_radius);
      m_occluders.push_back(occluder);
    }

    for (std::uint32_t index = 0; index < m_occluders.size(); ++index) {
      PropOccluder const& occluder = m_occluders[index];
      int const min_x = cell_coord(occluder.x - occluder.bounding_radius);
      int const max_x = cell_coord(occluder.x + occluder.bounding_radius);
      int const min_z = cell_coord(occluder.z - occluder.bounding_radius);
      int const max_z = cell_coord(occluder.z + occluder.bounding_radius);
      for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
        for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
          m_cells[cell_key(cell_x, cell_z)].push_back(index);
        }
      }
    }
  }

  [[nodiscard]] auto empty() const -> bool { return m_occluders.empty(); }

  template <typename Fn>
  void
  for_each_near(float min_x, float max_x, float min_z, float max_z, Fn&& fn) const {
    if (m_occluders.empty()) {
      return;
    }
    float const reach = m_max_bounding_radius;
    int const first_x = cell_coord(min_x - reach);
    int const last_x = cell_coord(max_x + reach);
    int const first_z = cell_coord(min_z - reach);
    int const last_z = cell_coord(max_z + reach);
    for (int cell_z = first_z; cell_z <= last_z; ++cell_z) {
      for (int cell_x = first_x; cell_x <= last_x; ++cell_x) {
        auto const bucket = m_cells.find(cell_key(cell_x, cell_z));
        if (bucket == m_cells.end()) {
          continue;
        }
        for (std::uint32_t const index : bucket->second) {
          fn(m_occluders[index]);
        }
      }
    }
  }

private:
  std::vector<PropOccluder> m_occluders;
  std::unordered_map<std::int64_t, std::vector<std::uint32_t>> m_cells;
  float m_max_bounding_radius{0.0F};
};

auto prop_occluders(const Game::Map::TerrainService& terrain)
    -> const PropOccluderIndex& {
  static std::mutex mutex;
  static PropOccluderIndex index;
  static const Game::Map::TerrainService* cached_terrain = nullptr;
  static std::uint64_t cached_revision = 0;
  static bool built = false;

  std::lock_guard<std::mutex> const guard(mutex);
  std::uint64_t const revision = terrain.world_props_revision();
  if (!built || cached_terrain != &terrain || cached_revision != revision) {
    index.rebuild(terrain);
    cached_terrain = &terrain;
    cached_revision = revision;
    built = true;
  }
  return index;
}

struct LocalSegment {
  float start_x{0.0F};
  float start_z{0.0F};
  float delta_x{0.0F};
  float delta_z{0.0F};
};

auto to_local(const PropOccluder& occluder,
              const QVector3D& start,
              const QVector3D& delta) -> LocalSegment {
  float const offset_x = start.x() - occluder.x;
  float const offset_z = start.z() - occluder.z;
  return {.start_x = (occluder.cosine * offset_x) + (occluder.sine * offset_z),
          .start_z = (-occluder.sine * offset_x) + (occluder.cosine * offset_z),
          .delta_x = (occluder.cosine * delta.x()) + (occluder.sine * delta.z()),
          .delta_z = (-occluder.sine * delta.x()) + (occluder.cosine * delta.z())};
}

auto slab(float start, float delta, float half, float& t_enter, float& t_exit) -> bool {
  constexpr float k_epsilon = 1.0e-5F;
  if (std::abs(delta) <= k_epsilon) {
    return start >= -half && start <= half;
  }
  float const inverse = 1.0F / delta;
  float t0 = (-half - start) * inverse;
  float t1 = (half - start) * inverse;
  if (t0 > t1) {
    std::swap(t0, t1);
  }
  t_enter = std::max(t_enter, t0);
  t_exit = std::min(t_exit, t1);
  return t_enter <= t_exit;
}

auto prop_entry_fraction(const PropOccluder& occluder,
                         const QVector3D& pivot,
                         const QVector3D& eye,
                         float radius) -> float {
  QVector3D const delta = eye - pivot;
  LocalSegment const local = to_local(occluder, pivot, delta);
  float const half_x = occluder.half_x + radius;
  float const half_z = occluder.half_z + radius;

  float t_enter = 0.0F;
  float t_exit = 1.0F;
  if (!slab(local.start_x, local.delta_x, half_x, t_enter, t_exit) ||
      !slab(local.start_z, local.delta_z, half_z, t_enter, t_exit)) {
    return 1.0F;
  }
  if (t_enter > 1.0F || t_exit < 0.0F || t_enter < 0.0F) {
    return 1.0F;
  }

  constexpr float k_epsilon = 1.0e-5F;
  float const start_y = pivot.y();
  if (std::abs(delta.y()) > k_epsilon) {
    float const t_top = (occluder.top_y - start_y) / delta.y();
    if (delta.y() > 0.0F) {
      t_exit = std::min(t_exit, t_top);
    } else {
      t_enter = std::max(t_enter, t_top);
    }
  } else if (start_y > occluder.top_y) {
    return 1.0F;
  }
  if (t_enter > t_exit || t_enter > 1.0F || t_exit < 0.0F) {
    return 1.0F;
  }
  return std::clamp(t_enter, 0.0F, 1.0F);
}

struct LocalPoint {
  float x{0.0F};
  float z{0.0F};
};

auto to_local(const PropOccluder& occluder, const QVector3D& point) -> LocalPoint {
  float const offset_x = point.x() - occluder.x;
  float const offset_z = point.z() - occluder.z;
  return {.x = (occluder.cosine * offset_x) + (occluder.sine * offset_z),
          .z = (-occluder.sine * offset_x) + (occluder.cosine * offset_z)};
}

auto overlaps(const PropOccluder& occluder,
              const QVector3D& point,
              const LocalPoint& local,
              float radius) -> bool {
  return point.y() <= occluder.top_y && std::abs(local.x) < occluder.half_x + radius &&
         std::abs(local.z) < occluder.half_z + radius;
}

auto wall_crest_occluder(const BuildingFootprint& wall,
                         const Game::Map::TerrainService& terrain) -> PropOccluder {
  PropOccluder occluder;
  occluder.x = wall.body_center_x;
  occluder.z = wall.body_center_z;
  occluder.half_x = wall.body_width * 0.5F;
  occluder.half_z = wall.body_depth * 0.5F;
  occluder.top_y = terrain.resolve_surface_world_y(occluder.x, occluder.z, 0.0F, 0.0F) +
                   WallWalk::k_crest_height;
  occluder.bounding_radius = std::hypot(occluder.half_x, occluder.half_z);
  return occluder;
}

constexpr float k_shut_door_half_thickness = 0.3F;

auto gate_tower_occluders(const BuildingFootprint& gate,
                          const Game::Map::TerrainService& terrain)
    -> std::array<PropOccluder, 2> {
  std::array<PropOccluder, 2> towers{};
  auto const bodies = gate_tower_bodies(gate);
  for (std::size_t side = 0; side < bodies.size(); ++side) {
    PropOccluder& tower = towers[side];
    tower.x = bodies[side].center_x;
    tower.z = bodies[side].center_z;
    tower.half_x = bodies[side].half_x;
    tower.half_z = bodies[side].half_z;
    tower.top_y = terrain.resolve_surface_world_y(tower.x, tower.z, 0.0F, 0.0F) +
                  WallWalk::k_gate_crest_height;
    tower.bounding_radius = std::hypot(tower.half_x, tower.half_z);
  }
  return towers;
}

template <typename Fn>
void for_each_wall_crest(const CameraObstructionField& field,
                         float min_x,
                         float max_x,
                         float min_z,
                         float max_z,
                         Fn&& fn) {
  if (field.buildings == nullptr || field.terrain == nullptr ||
      !field.terrain->is_initialized()) {
    return;
  }
  for (auto const& building : field.buildings->get_all_buildings()) {
    if (!building.blocks_navigation || !building.wall_link) {
      continue;
    }
    float const reach =
        (building.wall_gate
             ? Engine::Core::GateComponent::k_structure_half_span
             : std::hypot(building.body_width, building.body_depth) * 0.5F) +
        field.radius;
    if (building.body_center_x + reach < min_x ||
        building.body_center_x - reach > max_x ||
        building.body_center_z + reach < min_z ||
        building.body_center_z - reach > max_z) {
      continue;
    }
    if (!building.wall_gate) {
      fn(wall_crest_occluder(building, *field.terrain));
      continue;
    }
    for (PropOccluder const& tower : gate_tower_occluders(building, *field.terrain)) {
      fn(tower);
    }
    for (GateBlocker const& shut : field.shut_gates) {
      if (shut.entity_id != building.entity_id) {
        continue;
      }
      bool const along_x = building.width >= building.depth;
      PropOccluder doors;
      doors.x = building.center_x;
      doors.z = building.center_z;
      doors.half_x = along_x ? Engine::Core::GateComponent::k_passage_half_width
                             : k_shut_door_half_thickness;
      doors.half_z = along_x ? k_shut_door_half_thickness
                             : Engine::Core::GateComponent::k_passage_half_width;
      doors.top_y =
          field.terrain->resolve_surface_world_y(doors.x, doors.z, 0.0F, 0.0F) +
          WallWalk::k_gate_crest_height;
      doors.bounding_radius = std::hypot(doors.half_x, doors.half_z);
      fn(doors);
    }
  }
}

auto walls_are_crest_limited(const CameraObstructionField& field) -> bool {
  return field.terrain != nullptr && field.terrain->is_initialized();
}

} // namespace

auto camera_boom_clear_fraction(const CameraObstructionField& field,
                                const QVector3D& pivot,
                                const QVector3D& eye) -> float {
  float clear = 1.0F;
  bool const crest_limited = walls_are_crest_limited(field);
  if (field.buildings != nullptr) {
    clear = first_building_body_intersection_fraction(
        *field.buildings, pivot, eye, field.radius, 0, crest_limited);
  }
  if (clear <= 0.0F) {
    return 0.0F;
  }
  if (!crest_limited) {
    return clear;
  }
  for_each_wall_crest(field,
                      std::min(pivot.x(), eye.x()),
                      std::max(pivot.x(), eye.x()),
                      std::min(pivot.z(), eye.z()),
                      std::max(pivot.z(), eye.z()),
                      [&](const PropOccluder& wall) {
                        clear = std::min(
                            clear, prop_entry_fraction(wall, pivot, eye, field.radius));
                      });

  auto const& index = prop_occluders(*field.terrain);
  index.for_each_near(
      std::min(pivot.x(), eye.x()),
      std::max(pivot.x(), eye.x()),
      std::min(pivot.z(), eye.z()),
      std::max(pivot.z(), eye.z()),
      [&](const PropOccluder& occluder) {
        if (occluder.stem) {
          return;
        }
        clear =
            std::min(clear, prop_entry_fraction(occluder, pivot, eye, field.radius));
      });
  return std::max(clear, 0.0F);
}

auto camera_body_clearance(const CameraObstructionField& field,
                           const QVector3D& point) -> float {
  float clearance = std::numeric_limits<float>::max();
  bool const crest_limited = walls_are_crest_limited(field);
  if (field.buildings != nullptr) {
    clearance = nearest_building_body_clearance(*field.buildings, point, crest_limited);
  }
  if (!crest_limited) {
    return clearance;
  }

  auto measure = [&](const PropOccluder& occluder) {
    if (point.y() > occluder.top_y) {
      return;
    }
    LocalPoint const local = to_local(occluder, point);
    float const gap_x = std::abs(local.x) - occluder.half_x;
    float const gap_z = std::abs(local.z) - occluder.half_z;
    float distance = 0.0F;
    if (gap_x > 0.0F || gap_z > 0.0F) {
      distance = std::hypot(std::max(gap_x, 0.0F), std::max(gap_z, 0.0F));
    } else {
      distance = std::max(gap_x, gap_z);
    }
    clearance = std::min(clearance, distance);
  };
  for_each_wall_crest(field, point.x(), point.x(), point.z(), point.z(), measure);
  auto const& index = prop_occluders(*field.terrain);
  index.for_each_near(point.x(), point.x(), point.z(), point.z(), measure);
  return clearance;
}

auto camera_depenetrated_point(const CameraObstructionField& field,
                               const QVector3D& point) -> QVector3D {
  QVector3D resolved = point;
  bool const crest_limited = walls_are_crest_limited(field);
  auto push_out = [&](const PropOccluder& occluder) {
    LocalPoint local = to_local(occluder, resolved);
    if (!overlaps(occluder, resolved, local, field.radius)) {
      return;
    }
    float const half_x = occluder.half_x + field.radius;
    float const half_z = occluder.half_z + field.radius;
    float const overlap_x = half_x - std::abs(local.x);
    float const overlap_z = half_z - std::abs(local.z);
    if (overlap_x <= overlap_z) {
      local.x = local.x >= 0.0F ? half_x : -half_x;
    } else {
      local.z = local.z >= 0.0F ? half_z : -half_z;
    }
    resolved.setX(occluder.x + (occluder.cosine * local.x) - (occluder.sine * local.z));
    resolved.setZ(occluder.z + (occluder.sine * local.x) + (occluder.cosine * local.z));
  };
  constexpr int k_passes = 4;
  for (int pass = 0; pass < k_passes; ++pass) {
    QVector3D const before = resolved;

    if (field.buildings != nullptr) {
      QVector3D const cleared = depenetrate_from_building_bodies(
          *field.buildings, resolved, field.radius, crest_limited);
      resolved.setX(cleared.x());
      resolved.setZ(cleared.z());
    }

    if (crest_limited) {
      QVector3D const probe = resolved;
      for_each_wall_crest(field, probe.x(), probe.x(), probe.z(), probe.z(), push_out);
      prop_occluders(*field.terrain)
          .for_each_near(probe.x(), probe.x(), probe.z(), probe.z(), push_out);
    }

    if ((resolved - before).lengthSquared() <= 1.0e-8F) {
      break;
    }
  }
  return resolved;
}

} // namespace Game::Systems
