#include "terrain_service.h"

#include <QDebug>
#include <QVector3D>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "../core/ambient_session.h"
#include "../systems/building_collision_registry.h"
#include "../units/spawn_type.h"
#include "map_definition.h"
#include "terrain.h"
#include "terrain_world_props.h"

namespace Game::Map {

auto TerrainService::instance() -> TerrainService& {
  return *Game::Session::ambient_services().terrain;
}

namespace {

auto sample_grid_clamped(
    const std::vector<float>& values, int width, int height, int x, int z) -> float {
  if (values.empty() || width <= 0 || height <= 0) {
    return 0.0F;
  }

  x = std::clamp(x, 0, width - 1);
  z = std::clamp(z, 0, height - 1);
  return values[static_cast<size_t>(z * width + x)];
}

void register_authored_building_obstacles(const MapDefinition& map_def) {
  using Game::Systems::BuildingCollisionRegistry;

  std::vector<Game::Systems::BuildingFootprint> obstacles;
  obstacles.reserve(map_def.structures.size() + map_def.spawns.size());

  const auto add = [&obstacles](Game::Units::SpawnType type,
                                float center_x,
                                float center_z,
                                float min_width,
                                float min_depth) {
    if (!Game::Units::is_building_spawn(type)) {
      return;
    }
    const auto size = BuildingCollisionRegistry::get_building_size(
        Game::Units::spawn_typeToString(type));
    obstacles.emplace_back(center_x,
                           center_z,
                           std::max(size.width, min_width),
                           std::max(size.depth, min_depth),
                           0,
                           0U);
  };

  for (const auto& structure : map_def.structures) {
    if (const auto* point = std::get_if<PointStructureGeometry>(&structure.geometry)) {
      add(structure.type, point->position.x(), point->position.z(), 0.0F, 0.0F);
      continue;
    }
    if (const auto* line = std::get_if<LineStructureGeometry>(&structure.geometry)) {

      const QVector3D delta = line->end - line->start;
      const float length = delta.length();
      const int steps = std::max(1, static_cast<int>(std::ceil(length / 2.0F)));
      for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const QVector3D point = line->start + (delta * t);
        add(structure.type, point.x(), point.z(), line->width, line->width);
      }
      continue;
    }
  }

  for (const auto& spawn : map_def.spawns) {
    add(spawn.type, spawn.x, spawn.z, 0.0F, 0.0F);
  }

  BuildingCollisionRegistry::instance().set_authored_obstacles(std::move(obstacles));
}

} // namespace

void TerrainService::initialize(const MapDefinition& map_def) {
  m_sealed = false;
  m_world_props_from_save = false;
  m_prop_surface_cache.clear();
  m_prop_surface_cache_valid = false;
  m_height_map = std::make_unique<TerrainHeightMap>(
      map_def.grid.width, map_def.grid.height, map_def.grid.tile_size);

  m_height_map->apply_biome_variation(map_def.biome);
  m_height_map->build_from_features(map_def.terrain);
  m_height_map->add_lakes(map_def.lakes);
  m_height_map->add_river_segments(map_def.rivers);
  m_height_map->add_fords(map_def.fords);
  m_height_map->add_bridges(map_def.bridges);
  m_biome_settings = map_def.biome;

  set_supernatural_presence(map_def.undead_zones.empty() ? 0.0F : 1.0F);
  m_coord_system = map_def.coordSystem;

  m_road_segments = map_def.roads;
  m_forests = map_def.forests;
  rebuild_road_spatial_index();

  m_reserved_world_prop_ids.clear();
  m_next_world_prop_id = 1;
  m_authored_world_props = map_def.world_props;
  normalize_world_props(m_authored_world_props);
  register_authored_building_obstacles(map_def);

  m_world_props = m_authored_world_props;
  bump_world_props_revision();
  m_world_props = build_runtime_world_props(
      *m_height_map, m_biome_settings, m_coord_system, m_authored_world_props);
  normalize_world_props(m_world_props);
  sync_world_prop_identity_state();
  rebuild_terrain_field();
  bump_authored_world_props_revision();
  bump_world_props_revision();
  bump_navigation_topology_revision();
}

void TerrainService::initialize_keeping_world_props(const MapDefinition& map_def) {
  if (!is_initialized() || !m_world_props_from_save) {
    initialize(map_def);
    return;
  }

  std::vector<WorldProp> saved_world_props = m_world_props;
  std::vector<WorldProp> saved_authored_world_props = m_authored_world_props;

  initialize(map_def);

  m_world_props = std::move(saved_world_props);
  m_authored_world_props = std::move(saved_authored_world_props);
  normalize_world_props(m_authored_world_props);
  normalize_world_props(m_world_props);
  m_prop_surface_cache.clear();
  m_prop_surface_cache_valid = false;
  sync_world_prop_identity_state();
  rebuild_terrain_field();
  m_world_props_from_save = true;
  bump_authored_world_props_revision();
  bump_world_props_revision();
  bump_navigation_topology_revision();
}

void TerrainService::clear() {
  bump_world_props_revision();
  bump_authored_world_props_revision();
  bump_navigation_topology_revision();
  m_sealed = false;
  m_world_props_from_save = false;
  m_height_map.reset();
  m_prop_surface_cache.clear();
  m_prop_surface_cache_valid = false;
  m_terrain_field.clear();
  m_biome_settings = BiomeSettings();
  m_coord_system = CoordSystem::Grid;
  m_authored_world_props.clear();
  m_world_props.clear();
  m_road_segments.clear();
  m_forests.clear();
  m_road_index.clear();
  m_reserved_world_prop_ids.clear();
  m_next_world_prop_id = 1;
  bump_authored_world_props_revision();
  bump_world_props_revision();
  bump_navigation_topology_revision();
}

void TerrainService::seal() {
  m_sealed = true;
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
    const std::vector<WorldProp>& authored_world_props,
    const std::vector<Lake>& lakes,
    const HillNavigation& hills,
    const std::vector<FordCrossing>& fords) {
  m_sealed = false;
  m_prop_surface_cache.clear();
  m_prop_surface_cache_valid = false;
  m_height_map = std::make_unique<TerrainHeightMap>(width, height, tile_size);
  m_height_map->restore_from_data(
      heights, terrain_types, rivers, bridges, lakes, hills, fords);
  m_biome_settings = biome;
  m_coord_system = CoordSystem::Grid;

  m_road_segments = roads;
  rebuild_road_spatial_index();
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
  m_world_props_from_save = true;
  sync_world_prop_identity_state();
  rebuild_terrain_field();
  bump_authored_world_props_revision();
  bump_world_props_revision();
  bump_navigation_topology_revision();
}

auto TerrainService::is_point_in_registered_building(float world_x,
                                                     float world_z) const -> bool {
  return Game::Systems::BuildingCollisionRegistry::instance().is_point_in_building(
      world_x, world_z);
}

void TerrainService::rebuild_road_spatial_index() {
  if (m_height_map == nullptr) {
    m_road_index.clear();
    return;
  }
  m_road_index.rebuild(m_road_segments,
                       m_height_map->get_tile_size(),
                       m_height_map->get_width(),
                       m_height_map->get_height());
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

auto TerrainService::ford_profile_at(float world_x,
                                     float world_z) const -> const FordProfile* {
  return m_height_map != nullptr ? m_height_map->ford_profile_at(world_x, world_z)
                                 : nullptr;
}

auto TerrainService::ford_water_depth_at(float world_x, float world_z) const -> float {
  return m_height_map != nullptr ? m_height_map->ford_water_depth_at(world_x, world_z)
                                 : 0.0F;
}

auto TerrainService::ford_water_level_at(float world_x, float world_z) const
    -> std::optional<float> {
  return m_height_map != nullptr ? m_height_map->ford_water_level_at(world_x, world_z)
                                 : std::nullopt;
}

auto TerrainService::has_fords() const -> bool {
  return m_height_map != nullptr && m_height_map->has_fords();
}

auto TerrainService::next_props_revision() -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{0};
  return ++counter;
}

void TerrainService::bump_world_props_revision() {
  m_world_props_revision = next_props_revision();
}

void TerrainService::bump_authored_world_props_revision() {
  m_authored_world_props_revision = next_props_revision();
}

void TerrainService::bump_navigation_topology_revision() {
  m_navigation_topology_revision = next_props_revision();
}

} // namespace Game::Map
