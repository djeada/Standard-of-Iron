#include "terrain_world_props.h"

#include <QDebug>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "procedural_tree_generation.h"
#include "terrain.h"
#include "terrain_service.h"

namespace Game::Map {

namespace {

constexpr float k_min_tile_size = 0.0001F;
constexpr float k_harvest_grid_snap_distance = 3.0F;

auto authored_grid_to_world(float grid_coord, int grid_size, float tile_size) -> float {
  float const safe_tile_size = std::max(tile_size, k_min_tile_size);
  return (grid_coord - (static_cast<float>(grid_size) * 0.5F - 0.5F)) * safe_tile_size;
}

auto authored_prop_world_xz(const TerrainHeightMap* height_map,
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
  auto const [world_x, world_z] =
      authored_prop_world_xz(height_map, coord_system, prop);
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
        authored_prop_world_xz(height_map, coord_system, prop);
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

auto lies_in_fields(const TerrainHeightMap& height_map,
                    float world_x,
                    float world_z,
                    float reach) -> bool {
  const float tile = std::max(height_map.get_tile_size(), 0.0001F);
  const float grid_x = (world_x / tile) + (height_map.get_width() * 0.5F - 0.5F);
  const float grid_z = (world_z / tile) + (height_map.get_height() * 0.5F - 0.5F);
  const float step = std::ceil(reach / tile);
  for (const float dz : {-step, 0.0F, step}) {
    for (const float dx : {-step, 0.0F, step}) {
      if (height_map.is_fields(static_cast<int>(std::lround(grid_x + dx)),
                               static_cast<int>(std::lround(grid_z + dz)))) {
        return true;
      }
    }
  }
  return false;
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

} // namespace

auto build_runtime_world_props(const TerrainHeightMap& height_map,
                               const BiomeSettings& biome_settings,
                               CoordSystem coord_system,
                               const std::vector<WorldProp>& authored_world_props)
    -> std::vector<WorldProp> {
  std::vector<WorldProp> runtime_world_props = authored_world_props;
  auto generated_world_props = generate_procedural_world_props(
      height_map, biome_settings, coord_system, authored_world_props);
  std::erase_if(generated_world_props, [&](const WorldProp& prop) {
    const auto [world_x, world_z] =
        authored_prop_world_xz(&height_map, coord_system, prop);
    return lies_in_fields(
        height_map, world_x, world_z, world_prop_ground_radius(prop.type, prop.scale));
  });
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

auto TerrainService::world_prop_world_position(const WorldProp& prop,
                                               float world_y_offset,
                                               float fallback_y) const -> QVector3D {
  auto const [world_x, world_z] =
      authored_prop_world_xz(m_height_map.get(), m_coord_system, prop);

  if (m_height_map == nullptr || prop.id == 0) {
    return resolve_surface_world_position(world_x, world_z, world_y_offset, fallback_y);
  }

  if (!m_prop_surface_cache_valid ||
      m_prop_surface_cache_revision != m_world_props_revision) {
    m_prop_surface_cache.clear();
    m_prop_surface_cache_revision = m_world_props_revision;
    m_prop_surface_cache_valid = true;
  }

  auto entry = m_prop_surface_cache.find(prop.id);
  if (entry == m_prop_surface_cache.end()) {
    entry =
        m_prop_surface_cache
            .emplace(prop.id,
                     resolve_surface_world_position(world_x, world_z, 0.0F, fallback_y))
            .first;
  }

  return {entry->second.x(), entry->second.y() + world_y_offset, entry->second.z()};
}

void TerrainService::remove_non_persistent_props() {
  if (m_sealed) {
    qWarning()
        << "TerrainService: remove_non_persistent_props ignored; terrain is sealed";
    return;
  }
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

auto TerrainService::add_world_prop_at_world(WorldProp prop,
                                             float world_x,
                                             float world_z) -> std::uint64_t {
  if (m_sealed) {
    qWarning() << "TerrainService: add_world_prop_at_world ignored; terrain is sealed";
    return 0;
  }
  if (m_coord_system == CoordSystem::World || m_height_map == nullptr) {
    prop.x = world_x;
    prop.z = world_z;
  } else {
    float const safe_tile_size =
        std::max(m_height_map->get_tile_size(), k_min_tile_size);
    prop.x = world_x / safe_tile_size +
             (static_cast<float>(m_height_map->get_width()) * 0.5F - 0.5F);
    prop.z = world_z / safe_tile_size +
             (static_cast<float>(m_height_map->get_height()) * 0.5F - 0.5F);
  }

  prop.id = m_next_world_prop_id++;
  m_authored_world_props.push_back(prop);
  m_world_props.push_back(prop);
  bump_authored_world_props_revision();
  bump_world_props_revision();
  return prop.id;
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

auto TerrainService::world_prop_world_xz(const WorldProp& prop) const
    -> std::pair<float, float> {
  return authored_prop_world_xz(m_height_map.get(), m_coord_system, prop);
}

auto TerrainService::world_prop_footprint_world_position(const WorldProp& prop,
                                                         float footprint_radius,
                                                         float world_y_offset,
                                                         float fallback_y) const
    -> QVector3D {
  auto const [world_x, world_z] =
      authored_prop_world_xz(m_height_map.get(), m_coord_system, prop);
  return resolve_footprint_world_position(
      world_x, world_z, footprint_radius, world_y_offset, fallback_y);
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

} // namespace Game::Map
