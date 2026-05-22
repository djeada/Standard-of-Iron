#include "terrain_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "../systems/building_collision_registry.h"
#include "map_definition.h"
#include "procedural_tree_generation.h"
#include "terrain.h"

namespace Game::Map {

namespace {

constexpr float k_min_tile_size = 0.0001F;
constexpr float k_harvest_grid_snap_distance = 3.0F;

struct SurfaceBaseSample {
  float world_y{0.0F};
  SurfaceHeightKind kind{SurfaceHeightKind::Fallback};
};

auto authored_grid_to_world(float grid_coord, int grid_size, float tile_size) -> float {
  float const safe_tile_size = std::max(tile_size, k_min_tile_size);
  return (grid_coord - (static_cast<float>(grid_size) * 0.5F - 0.5F)) * safe_tile_size;
}

auto world_prop_world_xz(const TerrainHeightMap* height_map,
                         CoordSystem coord_system,
                         const WorldProp& prop) -> std::pair<float, float> {
  if (coord_system == CoordSystem::World || height_map == nullptr) {
    return {prop.x, prop.z};
  }
  return {authored_grid_to_world(
              prop.x, height_map->get_width(), height_map->get_tile_size()),
          authored_grid_to_world(
              prop.z, height_map->get_height(), height_map->get_tile_size())};
}

auto world_prop_grid_position(const TerrainHeightMap* height_map,
                              CoordSystem coord_system,
                              const WorldProp& prop) -> std::pair<float, float> {
  if (coord_system == CoordSystem::Grid || height_map == nullptr) {
    return {prop.x, prop.z};
  }

  float const safe_tile_size = std::max(height_map->get_tile_size(), k_min_tile_size);
  float const half_w = static_cast<float>(height_map->get_width()) * 0.5F - 0.5F;
  float const half_h = static_cast<float>(height_map->get_height()) * 0.5F - 0.5F;
  return {prop.x / safe_tile_size + half_w, prop.z / safe_tile_size + half_h};
}

auto make_world_prop_target(const TerrainHeightMap* height_map,
                            CoordSystem coord_system,
                            const WorldProp& prop) -> WorldPropTarget {
  auto const [world_x, world_z] = world_prop_world_xz(height_map, coord_system, prop);
  return WorldPropTarget{.id = prop.id, .type = prop.type, .x = world_x, .z = world_z};
}

template <typename Predicate>
auto find_world_prop_near_world(const TerrainHeightMap* height_map,
                                CoordSystem coord_system,
                                const std::vector<WorldProp>& world_props,
                                const std::unordered_set<std::uint64_t>& reserved_ids,
                                float world_x,
                                float world_z,
                                float max_world_distance,
                                Predicate&& matches) -> std::optional<WorldPropTarget> {
  float const max_distance_sq =
      std::max(max_world_distance, 0.0F) * std::max(max_world_distance, 0.0F);
  float const safe_tile_size =
      (height_map != nullptr) ? std::max(height_map->get_tile_size(), k_min_tile_size)
                              : 1.0F;
  float const query_grid_x =
      world_x / safe_tile_size +
      ((height_map != nullptr)
           ? (static_cast<float>(height_map->get_width()) * 0.5F - 0.5F)
           : 0.0F);
  float const query_grid_z =
      world_z / safe_tile_size +
      ((height_map != nullptr)
           ? (static_cast<float>(height_map->get_height()) * 0.5F - 0.5F)
           : 0.0F);
  float const max_grid_distance_sq =
      k_harvest_grid_snap_distance * k_harvest_grid_snap_distance;
  const WorldProp* best = nullptr;
  float best_distance_sq = max_distance_sq;
  float best_grid_distance_sq = max_grid_distance_sq;
  for (const auto& prop : world_props) {
    if (!matches(prop.type) || reserved_ids.contains(prop.id)) {
      continue;
    }
    auto const [prop_world_x, prop_world_z] =
        world_prop_world_xz(height_map, coord_system, prop);
    auto const [prop_grid_x, prop_grid_z] =
        world_prop_grid_position(height_map, coord_system, prop);
    float const dx = prop_world_x - world_x;
    float const dz = prop_world_z - world_z;
    float const distance_sq = dx * dx + dz * dz;
    float const grid_dx = prop_grid_x - query_grid_x;
    float const grid_dz = prop_grid_z - query_grid_z;
    float const grid_distance_sq = grid_dx * grid_dx + grid_dz * grid_dz;
    bool const within_world_distance = distance_sq <= max_distance_sq;
    bool const within_grid_distance = grid_distance_sq <= max_grid_distance_sq;
    if (!within_world_distance && !within_grid_distance) {
      continue;
    }
    if (within_world_distance) {
      if (best == nullptr || distance_sq < best_distance_sq ||
          (distance_sq == best_distance_sq &&
           grid_distance_sq < best_grid_distance_sq)) {
        best = &prop;
        best_distance_sq = distance_sq;
        best_grid_distance_sq = grid_distance_sq;
      }
      continue;
    }
    if (best != nullptr && best_distance_sq < max_distance_sq) {
      continue;
    }
    if (grid_distance_sq > best_grid_distance_sq ||
        (grid_distance_sq == best_grid_distance_sq &&
         distance_sq >= best_distance_sq)) {
      continue;
    }
    best = &prop;
    best_distance_sq = distance_sq;
    best_grid_distance_sq = grid_distance_sq;
  }
  if (best == nullptr) {
    return std::nullopt;
  }
  return make_world_prop_target(height_map, coord_system, *best);
}

template <typename Predicate>
auto find_world_prop_near_grid(const TerrainHeightMap* height_map,
                               CoordSystem coord_system,
                               const std::vector<WorldProp>& world_props,
                               const std::unordered_set<std::uint64_t>& reserved_ids,
                               float grid_x,
                               float grid_z,
                               float max_grid_distance,
                               Predicate&& matches) -> std::optional<WorldPropTarget> {
  float const max_distance_sq =
      std::max(max_grid_distance, 0.0F) * std::max(max_grid_distance, 0.0F);
  const WorldProp* best = nullptr;
  float best_distance_sq = max_distance_sq;
  for (const auto& prop : world_props) {
    if (!matches(prop.type) || reserved_ids.contains(prop.id)) {
      continue;
    }
    auto const [prop_grid_x, prop_grid_z] =
        world_prop_grid_position(height_map, coord_system, prop);
    float const dx = prop_grid_x - grid_x;
    float const dz = prop_grid_z - grid_z;
    float const distance_sq = dx * dx + dz * dz;
    if (distance_sq > best_distance_sq) {
      continue;
    }
    best = &prop;
    best_distance_sq = distance_sq;
  }
  if (best == nullptr) {
    return std::nullopt;
  }
  return make_world_prop_target(height_map, coord_system, *best);
}

auto build_runtime_world_props(const TerrainHeightMap& height_map,
                               const BiomeSettings& biome_settings,
                               CoordSystem coord_system,
                               const std::vector<WorldProp>& authored_world_props)
    -> std::vector<WorldProp> {
  std::vector<WorldProp> runtime_world_props = authored_world_props;
  auto generated_world_props = generate_procedural_world_props(
      height_map, biome_settings, coord_system, authored_world_props);
  for (auto& prop : generated_world_props) {
    prop.persistent = false;
  }
  runtime_world_props.insert(runtime_world_props.end(),
                             generated_world_props.begin(),
                             generated_world_props.end());
  return runtime_world_props;
}

auto world_props_match(const std::vector<WorldProp>& lhs,
                       const std::vector<WorldProp>& rhs) -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  return std::equal(
      lhs.begin(), lhs.end(), rhs.begin(), [](const WorldProp& a, const WorldProp& b) {
        return a.id == b.id && a.type == b.type && a.x == b.x && a.z == b.z &&
               a.scale == b.scale && a.rotation == b.rotation &&
               a.intensity == b.intensity && a.radius == b.radius &&
               a.persistent == b.persistent;
      });
}

auto has_runtime_harvest_props(const std::vector<WorldProp>& world_props) -> bool {
  return std::any_of(world_props.begin(), world_props.end(), [](const WorldProp& prop) {
    return !prop.persistent && is_harvestable_world_prop_type(prop.type);
  });
}

template <typename Predicate>
auto find_world_prop_by_id(const TerrainHeightMap* height_map,
                           CoordSystem coord_system,
                           const std::vector<WorldProp>& world_props,
                           std::uint64_t world_prop_id,
                           Predicate&& matches) -> std::optional<WorldPropTarget> {
  if (world_prop_id == 0) {
    return std::nullopt;
  }
  const auto it = std::find_if(world_props.begin(),
                               world_props.end(),
                               [world_prop_id, &matches](const WorldProp& prop) {
                                 return prop.id == world_prop_id && matches(prop.type);
                               });
  if (it == world_props.end()) {
    return std::nullopt;
  }
  return make_world_prop_target(height_map, coord_system, *it);
}

auto sample_grid_clamped(
    const std::vector<float>& values, int width, int height, int x, int z) -> float {
  if (values.empty() || width <= 0 || height <= 0) {
    return 0.0F;
  }

  x = std::clamp(x, 0, width - 1);
  z = std::clamp(z, 0, height - 1);
  return values[static_cast<size_t>(z * width + x)];
}

auto is_point_within_linear_feature(float world_x,
                                    float world_z,
                                    const QVector3D& start,
                                    const QVector3D& end,
                                    float width,
                                    float clearance) -> bool {
  const float effective_half_width =
      std::max(0.0F, width * 0.5F + std::max(clearance, 0.0F));
  const float dx = end.x() - start.x();
  const float dz = end.z() - start.z();
  const float segment_length_sq = dx * dx + dz * dz;

  if (segment_length_sq < 0.0001F) {
    const float dist_x = world_x - start.x();
    const float dist_z = world_z - start.z();
    return dist_x * dist_x + dist_z * dist_z <=
           effective_half_width * effective_half_width;
  }

  const float px = world_x - start.x();
  const float pz = world_z - start.z();
  float t = (px * dx + pz * dz) / segment_length_sq;
  t = std::clamp(t, 0.0F, 1.0F);

  const float closest_x = start.x() + t * dx;
  const float closest_z = start.z() + t * dz;
  const float dist_x = world_x - closest_x;
  const float dist_z = world_z - closest_z;
  return dist_x * dist_x + dist_z * dist_z <=
         effective_half_width * effective_half_width;
}

auto sample_surface_base_height(const TerrainHeightMap* height_map,
                                const std::vector<RoadSegment>& road_segments,
                                float world_x,
                                float world_z,
                                float fallback_y) -> SurfaceBaseSample {
  if (height_map == nullptr) {
    return {.world_y = fallback_y, .kind = SurfaceHeightKind::Fallback};
  }

  if (height_map->isOnBridge(world_x, world_z)) {
    if (auto const bridge_height = height_map->getBridgeDeckHeight(world_x, world_z);
        bridge_height.has_value()) {
      return {.world_y = *bridge_height, .kind = SurfaceHeightKind::Bridge};
    }
  }

  float const terrain_height = height_map->get_height_at(world_x, world_z);
  for (const auto& segment : road_segments) {
    const float dx = segment.end.x() - segment.start.x();
    const float dz = segment.end.z() - segment.start.z();
    const float segment_length_sq = dx * dx + dz * dz;

    if (segment_length_sq < 0.0001F) {
      const float dist_x = world_x - segment.start.x();
      const float dist_z = world_z - segment.start.z();
      const float dist_sq = dist_x * dist_x + dist_z * dist_z;
      const float half_width = segment.width * 0.5F;
      if (dist_sq <= half_width * half_width) {
        return {.world_y = terrain_height, .kind = SurfaceHeightKind::Road};
      }
      continue;
    }

    const float px = world_x - segment.start.x();
    const float pz = world_z - segment.start.z();
    float t = (px * dx + pz * dz) / segment_length_sq;
    t = std::clamp(t, 0.0F, 1.0F);

    const float closest_x = segment.start.x() + t * dx;
    const float closest_z = segment.start.z() + t * dz;

    const float dist_x = world_x - closest_x;
    const float dist_z = world_z - closest_z;
    const float dist_sq = dist_x * dist_x + dist_z * dist_z;

    const float half_width = segment.width * 0.5F;
    if (dist_sq <= half_width * half_width) {
      return {.world_y = terrain_height, .kind = SurfaceHeightKind::Road};
    }
  }

  return {.world_y = terrain_height, .kind = SurfaceHeightKind::Terrain};
}

} // namespace

auto TerrainService::instance() -> TerrainService& {
  static TerrainService s_instance;
  return s_instance;
}

auto TerrainService::world_prop_world_position(const WorldProp& prop,
                                               float world_y_offset,
                                               float fallback_y) const -> QVector3D {
  auto const [world_x, world_z] =
      world_prop_world_xz(m_height_map.get(), m_coord_system, prop);
  return resolve_surface_world_position(world_x, world_z, world_y_offset, fallback_y);
}

void TerrainService::initialize(const MapDefinition& map_def) {
  m_height_map = std::make_unique<TerrainHeightMap>(
      map_def.grid.width, map_def.grid.height, map_def.grid.tile_size);

  m_height_map->build_from_features(map_def.terrain);
  m_height_map->add_river_segments(map_def.rivers);
  m_height_map->add_bridges(map_def.bridges);
  m_biome_settings = map_def.biome;
  m_coord_system = map_def.coordSystem;
  m_height_map->apply_biome_variation(m_biome_settings);
  // Procedural scatter generation queries road proximity through TerrainService.
  m_road_segments = map_def.roads;
  m_authored_world_props = map_def.world_props;
  normalize_world_props(m_authored_world_props);
  m_world_props = build_runtime_world_props(
      *m_height_map, m_biome_settings, m_coord_system, m_authored_world_props);
  normalize_world_props(m_world_props);
  sync_world_prop_identity_state();
  rebuild_terrain_field();
  bump_authored_world_props_revision();
  bump_world_props_revision();
}

void TerrainService::clear() {
  m_height_map.reset();
  m_terrain_field.clear();
  m_biome_settings = BiomeSettings();
  m_coord_system = CoordSystem::Grid;
  m_authored_world_props.clear();
  m_world_props.clear();
  m_road_segments.clear();
  m_reserved_world_prop_ids.clear();
  m_next_world_prop_id = 1;
  bump_authored_world_props_revision();
  bump_world_props_revision();
}

void TerrainService::remove_non_persistent_props() {
  m_authored_world_props.erase(
      std::remove_if(m_authored_world_props.begin(),
                     m_authored_world_props.end(),
                     [](const WorldProp& p) { return !p.persistent; }),
      m_authored_world_props.end());
  m_world_props.erase(std::remove_if(m_world_props.begin(),
                                     m_world_props.end(),
                                     [](const WorldProp& p) { return !p.persistent; }),
                      m_world_props.end());
  sync_world_prop_identity_state();
  bump_authored_world_props_revision();
  bump_world_props_revision();
}

auto TerrainService::find_tree_near_world(float world_x,
                                          float world_z,
                                          float max_world_distance) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_near_world(m_height_map.get(),
                                    m_coord_system,
                                    m_world_props,
                                    m_reserved_world_prop_ids,
                                    world_x,
                                    world_z,
                                    max_world_distance,
                                    is_tree_world_prop_type);
}

auto TerrainService::find_tree_near_grid(float grid_x,
                                         float grid_z,
                                         float max_grid_distance) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_near_grid(m_height_map.get(),
                                   m_coord_system,
                                   m_world_props,
                                   m_reserved_world_prop_ids,
                                   grid_x,
                                   grid_z,
                                   max_grid_distance,
                                   is_tree_world_prop_type);
}

auto TerrainService::find_tree_by_id(std::uint64_t tree_id) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_by_id(m_height_map.get(),
                               m_coord_system,
                               m_world_props,
                               tree_id,
                               is_tree_world_prop_type);
}

auto TerrainService::find_boulder_near_world(float world_x,
                                             float world_z,
                                             float max_world_distance) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_near_world(m_height_map.get(),
                                    m_coord_system,
                                    m_world_props,
                                    m_reserved_world_prop_ids,
                                    world_x,
                                    world_z,
                                    max_world_distance,
                                    is_boulder_world_prop_type);
}

auto TerrainService::find_boulder_near_grid(float grid_x,
                                            float grid_z,
                                            float max_grid_distance) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_near_grid(m_height_map.get(),
                                   m_coord_system,
                                   m_world_props,
                                   m_reserved_world_prop_ids,
                                   grid_x,
                                   grid_z,
                                   max_grid_distance,
                                   is_boulder_world_prop_type);
}

auto TerrainService::find_boulder_by_id(std::uint64_t boulder_id) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_by_id(m_height_map.get(),
                               m_coord_system,
                               m_world_props,
                               boulder_id,
                               is_boulder_world_prop_type);
}

auto TerrainService::find_iron_ore_near_world(float world_x,
                                              float world_z,
                                              float max_world_distance) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_near_world(m_height_map.get(),
                                    m_coord_system,
                                    m_world_props,
                                    m_reserved_world_prop_ids,
                                    world_x,
                                    world_z,
                                    max_world_distance,
                                    is_iron_ore_world_prop_type);
}

auto TerrainService::find_iron_ore_near_grid(float grid_x,
                                             float grid_z,
                                             float max_grid_distance) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_near_grid(m_height_map.get(),
                                   m_coord_system,
                                   m_world_props,
                                   m_reserved_world_prop_ids,
                                   grid_x,
                                   grid_z,
                                   max_grid_distance,
                                   is_iron_ore_world_prop_type);
}

auto TerrainService::find_iron_ore_by_id(std::uint64_t iron_ore_id) const
    -> std::optional<WorldPropTarget> {
  return find_world_prop_by_id(m_height_map.get(),
                               m_coord_system,
                               m_world_props,
                               iron_ore_id,
                               is_iron_ore_world_prop_type);
}

auto TerrainService::find_harvestable_world_prop_by_id(
    std::uint64_t world_prop_id) const -> std::optional<WorldPropTarget> {
  return find_world_prop_by_id(m_height_map.get(),
                               m_coord_system,
                               m_world_props,
                               world_prop_id,
                               is_harvestable_world_prop_type);
}

auto TerrainService::is_world_prop_reserved(std::uint64_t world_prop_id) const -> bool {
  return world_prop_id != 0 && m_reserved_world_prop_ids.contains(world_prop_id);
}

auto TerrainService::reserve_world_prop(std::uint64_t world_prop_id) -> bool {
  if (!find_world_prop_by_id(m_height_map.get(),
                             m_coord_system,
                             m_world_props,
                             world_prop_id,
                             is_harvestable_world_prop_type)
           .has_value() ||
      m_reserved_world_prop_ids.contains(world_prop_id)) {
    return false;
  }
  m_reserved_world_prop_ids.insert(world_prop_id);
  return true;
}

void TerrainService::release_world_prop(std::uint64_t world_prop_id) {
  if (world_prop_id == 0) {
    return;
  }
  m_reserved_world_prop_ids.erase(world_prop_id);
}

auto TerrainService::harvest_world_prop(std::uint64_t world_prop_id) -> bool {
  const auto it = std::find_if(m_world_props.begin(),
                               m_world_props.end(),
                               [world_prop_id](const WorldProp& prop) {
                                 return prop.id == world_prop_id &&
                                        is_harvestable_world_prop_type(prop.type);
                               });
  if (it == m_world_props.end()) {
    release_world_prop(world_prop_id);
    return false;
  }
  m_world_props.erase(it);
  release_world_prop(world_prop_id);
  bump_world_props_revision();
  return true;
}

auto TerrainService::get_terrain_height(float world_x, float world_z) const -> float {
  return sample_surface_height(world_x, world_z).world_y;
}

auto TerrainService::sample_surface_height(
    float world_x, float world_z, float fallback_y) const -> SurfaceHeightSample {
  auto const base_sample = sample_surface_base_height(
      m_height_map.get(), m_road_segments, world_x, world_z, fallback_y);
  if (base_sample.kind == SurfaceHeightKind::Road) {
    return {.world_y = road_surface_world_y(base_sample.world_y),
            .kind = base_sample.kind};
  }
  return {.world_y = base_sample.world_y, .kind = base_sample.kind};
}

auto TerrainService::resolve_surface_world_y(float world_x,
                                             float world_z,
                                             float world_y_offset,
                                             float fallback_y) const -> float {
  auto const base_sample = sample_surface_base_height(
      m_height_map.get(), m_road_segments, world_x, world_z, fallback_y);

  float const surface_world_y = base_sample.kind == SurfaceHeightKind::Road
                                    ? road_surface_world_y(base_sample.world_y)
                                    : base_sample.world_y;
  double const resolved_world_y =
      static_cast<double>(surface_world_y) + static_cast<double>(world_y_offset);

  return static_cast<float>(resolved_world_y);
}

auto TerrainService::resolve_surface_world_position(float world_x,
                                                    float world_z,
                                                    float world_y_offset,
                                                    float fallback_y) const
    -> QVector3D {
  return {world_x,
          resolve_surface_world_y(world_x, world_z, world_y_offset, fallback_y),
          world_z};
}

auto TerrainService::get_terrain_height_grid(int grid_x, int grid_z) const -> float {
  if (!m_height_map) {
    return 0.0F;
  }
  return m_height_map->get_height_at_grid(grid_x, grid_z);
}

auto TerrainService::is_walkable(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return true;
  }
  return m_height_map->is_walkable(grid_x, grid_z);
}

auto TerrainService::is_forbidden(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return false;
  }

  if (!m_height_map->is_walkable(grid_x, grid_z)) {
    return true;
  }

  constexpr float k_half_cell_offset = 0.5F;

  const float half_width =
      static_cast<float>(m_height_map->get_width()) * k_half_cell_offset -
      k_half_cell_offset;
  const float half_height =
      static_cast<float>(m_height_map->get_height()) * k_half_cell_offset -
      k_half_cell_offset;
  const float tile_size = m_height_map->get_tile_size();

  const float world_x = (static_cast<float>(grid_x) - half_width) * tile_size;
  const float world_z = (static_cast<float>(grid_z) - half_height) * tile_size;

  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  return registry.is_point_in_building(world_x, world_z);
}

auto TerrainService::is_forbidden_world(float world_x, float world_z) const -> bool {
  if (!m_height_map) {
    return false;
  }

  constexpr float k_half_cell_offset = 0.5F;

  const float grid_half_width =
      static_cast<float>(m_height_map->get_width()) * k_half_cell_offset -
      k_half_cell_offset;
  const float grid_half_height =
      static_cast<float>(m_height_map->get_height()) * k_half_cell_offset -
      k_half_cell_offset;

  const float grid_x = world_x / m_height_map->get_tile_size() + grid_half_width;
  const float grid_z = world_z / m_height_map->get_tile_size() + grid_half_height;

  const int grid_x_int = static_cast<int>(std::round(grid_x));
  const int grid_z_int = static_cast<int>(std::round(grid_z));

  return is_forbidden(grid_x_int, grid_z_int);
}

auto TerrainService::is_hill_entrance(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return false;
  }
  return m_height_map->isHillEntrance(grid_x, grid_z);
}

auto TerrainService::get_terrain_type(int grid_x, int grid_z) const -> TerrainType {
  if (!m_height_map) {
    return TerrainType::Flat;
  }
  return m_height_map->getTerrainType(grid_x, grid_z);
}

void TerrainService::restore_from_serialized(
    int width,
    int height,
    float tile_size,
    const std::vector<float>& heights,
    const std::vector<TerrainType>& terrain_types,
    const std::vector<RiverSegment>& rivers,
    const std::vector<RoadSegment>& roads,
    const std::vector<Bridge>& bridges,
    const BiomeSettings& biome,
    const std::vector<WorldProp>& world_props,
    const std::vector<WorldProp>& authored_world_props) {
  m_height_map = std::make_unique<TerrainHeightMap>(width, height, tile_size);
  m_height_map->restore_from_data(heights, terrain_types, rivers, bridges);
  m_biome_settings = biome;
  m_coord_system = CoordSystem::Grid;
  // Procedural scatter generation queries road proximity through TerrainService.
  m_road_segments = roads;
  if (authored_world_props.empty()) {
    m_authored_world_props = world_props;
    normalize_world_props(m_authored_world_props);
    m_world_props = m_authored_world_props;
  } else {
    m_authored_world_props = authored_world_props;
    m_world_props = world_props;
    normalize_world_props(m_authored_world_props);
    normalize_world_props(m_world_props);
  }
  if (!has_runtime_harvest_props(m_world_props) &&
      world_props_match(m_authored_world_props, m_world_props)) {
    m_world_props = build_runtime_world_props(
        *m_height_map, m_biome_settings, m_coord_system, m_authored_world_props);
    normalize_world_props(m_world_props);
  }
  sync_world_prop_identity_state();
  rebuild_terrain_field();
  bump_authored_world_props_revision();
  bump_world_props_revision();
}

auto TerrainService::is_point_on_road(float world_x, float world_z) const -> bool {
  return sample_surface_base_height(
             m_height_map.get(), m_road_segments, world_x, world_z, 0.0F)
             .kind == SurfaceHeightKind::Road;
}

auto TerrainService::is_point_near_road(float world_x,
                                        float world_z,
                                        float clearance) const -> bool {
  for (const auto& segment : m_road_segments) {
    if (is_point_within_linear_feature(
            world_x, world_z, segment.start, segment.end, segment.width, clearance)) {
      return true;
    }
  }
  return false;
}

auto TerrainService::is_on_bridge(float world_x, float world_z) const -> bool {
  if (!m_height_map) {
    return false;
  }
  return m_height_map->isOnBridge(world_x, world_z);
}

auto TerrainService::is_point_near_bridge(float world_x,
                                          float world_z,
                                          float clearance) const -> bool {
  if (!m_height_map) {
    return false;
  }
  for (const auto& bridge : m_height_map->get_bridges()) {
    if (is_point_within_linear_feature(
            world_x, world_z, bridge.start, bridge.end, bridge.width, clearance)) {
      return true;
    }
  }
  return false;
}

auto TerrainService::is_point_near_river(float world_x,
                                         float world_z,
                                         float clearance) const -> bool {
  if (!m_height_map) {
    return false;
  }
  for (const auto& river : m_height_map->get_river_segments()) {
    if (is_point_within_linear_feature(
            world_x, world_z, river.start, river.end, river.width, clearance)) {
      return true;
    }
  }
  return false;
}

auto TerrainService::get_bridge_center_position(float world_x, float world_z) const
    -> std::optional<QVector3D> {
  if (!m_height_map) {
    return std::nullopt;
  }
  return m_height_map->getBridgeCenterPosition(world_x, world_z);
}

auto TerrainService::get_bridge_traversal_position(float world_x, float world_z) const
    -> std::optional<QVector3D> {
  if (!m_height_map) {
    return std::nullopt;
  }
  return m_height_map->getBridgeTraversalPosition(world_x, world_z);
}

void TerrainService::rebuild_terrain_field() {
  m_terrain_field.clear();

  if (!m_height_map) {
    return;
  }

  m_terrain_field.width = m_height_map->get_width();
  m_terrain_field.height = m_height_map->get_height();
  m_terrain_field.tile_size = m_height_map->get_tile_size();
  m_terrain_field.heights = m_height_map->get_height_data();

  const int width = m_terrain_field.width;
  const int height = m_terrain_field.height;
  const float tile = std::max(0.001F, m_terrain_field.tile_size);
  const auto count = static_cast<size_t>(width * height);
  m_terrain_field.slopes.assign(count, 0.0F);
  m_terrain_field.curvature.assign(count, 0.0F);

  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      const float h_c =
          sample_grid_clamped(m_terrain_field.heights, width, height, x, z);
      const float h_l =
          sample_grid_clamped(m_terrain_field.heights, width, height, x - 1, z);
      const float h_r =
          sample_grid_clamped(m_terrain_field.heights, width, height, x + 1, z);
      const float h_d =
          sample_grid_clamped(m_terrain_field.heights, width, height, x, z - 1);
      const float h_u =
          sample_grid_clamped(m_terrain_field.heights, width, height, x, z + 1);
      const float h_dl =
          sample_grid_clamped(m_terrain_field.heights, width, height, x - 1, z - 1);
      const float h_dr =
          sample_grid_clamped(m_terrain_field.heights, width, height, x + 1, z - 1);
      const float h_ul =
          sample_grid_clamped(m_terrain_field.heights, width, height, x - 1, z + 1);
      const float h_ur =
          sample_grid_clamped(m_terrain_field.heights, width, height, x + 1, z + 1);

      const float dx = (h_r - h_l) / (2.0F * tile);
      const float dz = (h_u - h_d) / (2.0F * tile);
      const float normal_y = 1.0F / std::sqrt(1.0F + dx * dx + dz * dz);
      const auto index = static_cast<size_t>(z * width + x);
      const float dxx = h_r - 2.0F * h_c + h_l;
      const float dzz = h_u - 2.0F * h_c + h_d;
      const float dxz = 0.25F * (h_ur - h_ul - h_dr + h_dl);
      const float curvature_scale = std::max(tile * tile, 0.0001F);

      m_terrain_field.slopes[index] = 1.0F - std::clamp(normal_y, 0.0F, 1.0F);
      m_terrain_field.curvature[index] =
          std::sqrt(dxx * dxx + dzz * dzz + 2.0F * dxz * dxz) / curvature_scale;
    }
  }
}

void TerrainService::normalize_world_props(std::vector<WorldProp>& world_props) {
  for (auto& prop : world_props) {
    if (prop.id == 0) {
      prop.id = m_next_world_prop_id++;
    }
  }
}

void TerrainService::sync_world_prop_identity_state() {
  m_reserved_world_prop_ids.clear();

  std::uint64_t max_id = 0;
  for (const auto& prop : m_authored_world_props) {
    max_id = std::max(max_id, prop.id);
  }
  for (const auto& prop : m_world_props) {
    max_id = std::max(max_id, prop.id);
  }
  m_next_world_prop_id = std::max(m_next_world_prop_id, max_id + 1);
}

void TerrainService::bump_world_props_revision() {
  ++m_world_props_revision;
}

void TerrainService::bump_authored_world_props_revision() {
  ++m_authored_world_props_revision;
}

} // namespace Game::Map
