#include "undead_shrine_placement.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"

namespace Game::Map {

namespace {

constexpr float k_grid_to_world_epsilon = 0.0001F;
constexpr float k_shrine_surface_offset = 0.05F;
constexpr float k_road_clearance = 1.2F;
constexpr float k_search_step = 0.75F;
constexpr int k_min_samples_per_ring = 8;

auto authored_to_world(float coord, int grid_size, float tile_size) -> float {
  float const safe_tile = std::max(tile_size, k_grid_to_world_epsilon);
  return (coord - (static_cast<float>(grid_size) * 0.5F - 0.5F)) * safe_tile;
}

auto prop_block_radius(const WorldProp& prop) -> float {
  return std::max(0.5F, world_prop_render_scale(prop.type) * prop.scale * 0.35F);
}

auto is_inside_playable_bounds(const TerrainService& terrain,
                               float world_x,
                               float world_z) -> bool {
  const auto* height_map = terrain.get_height_map();
  if (height_map == nullptr) {
    return true;
  }
  float const tile = std::max(height_map->get_tile_size(), k_grid_to_world_epsilon);
  float const half_width = static_cast<float>(height_map->get_width()) * 0.5F * tile;
  float const half_height = static_cast<float>(height_map->get_height()) * 0.5F * tile;
  return std::abs(world_x) <= half_width - k_undead_shrine_clearance &&
         std::abs(world_z) <= half_height - k_undead_shrine_clearance;
}

auto is_ground_clear(const TerrainService& terrain,
                     float world_x,
                     float world_z) -> bool {
  if (!terrain.is_initialized()) {
    return true;
  }
  if (terrain.is_forbidden_world(world_x, world_z)) {
    return false;
  }

  constexpr std::array<float, 4> k_corner_angles = {0.25F * std::numbers::pi_v<float>,
                                                    0.75F * std::numbers::pi_v<float>,
                                                    1.25F * std::numbers::pi_v<float>,
                                                    1.75F * std::numbers::pi_v<float>};
  for (float const angle : k_corner_angles) {
    float const corner_x = world_x + std::cos(angle) * k_undead_shrine_clearance;
    float const corner_z = world_z + std::sin(angle) * k_undead_shrine_clearance;
    if (terrain.is_forbidden_world(corner_x, corner_z)) {
      return false;
    }
  }

  return !terrain.is_point_near_water(world_x, world_z, k_undead_shrine_clearance) &&
         !terrain.is_point_near_bridge(world_x, world_z, k_undead_shrine_clearance) &&
         !terrain.is_point_near_road(world_x, world_z, k_road_clearance);
}

auto is_prop_clear(const TerrainService& terrain,
                   float world_x,
                   float world_z,
                   const UndeadShrineExclusions& exclusions) -> bool {
  for (const auto& prop : terrain.world_props()) {
    if (!world_prop_blocks_shrine(prop.type)) {
      continue;
    }

    if (prop.type == WorldProp::Type::MagicShrine &&
        !exclusions.claimed_prop_ids.contains(prop.id)) {
      continue;
    }
    QVector3D const prop_position = terrain.world_prop_world_position(prop);
    float const dx = prop_position.x() - world_x;
    float const dz = prop_position.z() - world_z;
    float const clearance = k_undead_shrine_clearance + prop_block_radius(prop);
    if (dx * dx + dz * dz < clearance * clearance) {
      return false;
    }
  }
  return true;
}

auto is_reserved_site_clear(float world_x,
                            float world_z,
                            const UndeadShrineExclusions& exclusions) -> bool {
  float const clearance = k_undead_shrine_clearance * 2.0F;
  return std::none_of(exclusions.reserved_sites.begin(),
                      exclusions.reserved_sites.end(),
                      [world_x, world_z, clearance](const QVector3D& site) {
                        float const dx = site.x() - world_x;
                        float const dz = site.z() - world_z;
                        return dx * dx + dz * dz < clearance * clearance;
                      });
}

auto find_existing_shrine(const TerrainService& terrain,
                          const QVector3D& center,
                          float adopt_radius,
                          const UndeadShrineExclusions& exclusions)
    -> const WorldProp* {
  const WorldProp* best = nullptr;
  float best_distance_sq = adopt_radius * adopt_radius;

  for (const auto& prop : terrain.world_props()) {
    if (prop.type != WorldProp::Type::MagicShrine ||
        exclusions.claimed_prop_ids.contains(prop.id)) {
      continue;
    }
    QVector3D const prop_position = terrain.world_prop_world_position(prop);
    float const dx = prop_position.x() - center.x();
    float const dz = prop_position.z() - center.z();
    float const distance_sq = dx * dx + dz * dz;
    if (distance_sq > best_distance_sq) {
      continue;
    }
    best = &prop;
    best_distance_sq = distance_sq;
  }

  return best;
}

} // namespace

auto undead_zone_center_world(const MapDefinition& map_definition,
                              const UndeadZone& zone) -> QVector3D {
  if (map_definition.coordSystem == CoordSystem::World) {
    return {zone.x, 0.0F, zone.z};
  }
  return {authored_to_world(
              zone.x, map_definition.grid.width, map_definition.grid.tile_size),
          0.0F,
          authored_to_world(
              zone.z, map_definition.grid.height, map_definition.grid.tile_size)};
}

auto is_undead_shrine_site_clear(const TerrainService& terrain,
                                 float world_x,
                                 float world_z,
                                 const UndeadShrineExclusions& exclusions) -> bool {
  if (!is_inside_playable_bounds(terrain, world_x, world_z)) {
    return false;
  }
  if (!is_ground_clear(terrain, world_x, world_z)) {
    return false;
  }
  if (!is_reserved_site_clear(world_x, world_z, exclusions)) {
    return false;
  }
  if (Game::Systems::BuildingCollisionRegistry::instance()
          .is_circle_overlapping_building(
              world_x, world_z, k_undead_shrine_clearance)) {
    return false;
  }
  return is_prop_clear(terrain, world_x, world_z, exclusions);
}

auto plan_undead_zone_shrine(const TerrainService& terrain,
                             const MapDefinition& map_definition,
                             const UndeadZone& zone,
                             const UndeadShrineExclusions& exclusions)
    -> UndeadShrinePlacement {
  UndeadShrinePlacement placement;
  placement.zone_id = zone.id;

  QVector3D center = undead_zone_center_world(map_definition, zone);
  center.setY(
      terrain.resolve_surface_world_y(center.x(), center.z(), k_shrine_surface_offset));

  float const adopt_radius = std::max(k_undead_shrine_adopt_distance, zone.radius);
  if (const WorldProp* existing =
          find_existing_shrine(terrain, center, adopt_radius, exclusions);
      existing != nullptr) {
    placement.placed = true;
    placement.adopted_existing_prop = true;
    placement.prop_id = existing->id;
    placement.world_position =
        terrain.world_prop_world_position(*existing, k_shrine_surface_offset);
    return placement;
  }

  if (is_undead_shrine_site_clear(terrain, center.x(), center.z(), exclusions)) {
    placement.placed = true;
    placement.world_position = center;
    return placement;
  }

  float const search_radius = std::max(k_undead_shrine_min_search_radius, zone.radius);
  for (float radius = k_search_step; radius <= search_radius; radius += k_search_step) {
    int const samples =
        std::max(k_min_samples_per_ring,
                 static_cast<int>(std::ceil(2.0F * std::numbers::pi_v<float> * radius /
                                            k_search_step)));
    for (int sample = 0; sample < samples; ++sample) {
      float const angle = 2.0F * std::numbers::pi_v<float> *
                          static_cast<float>(sample) / static_cast<float>(samples);
      float const world_x = center.x() + std::cos(angle) * radius;
      float const world_z = center.z() + std::sin(angle) * radius;
      if (!is_undead_shrine_site_clear(terrain, world_x, world_z, exclusions)) {
        continue;
      }
      placement.placed = true;
      placement.moved_off_center = true;
      placement.world_position = terrain.resolve_surface_world_position(
          world_x, world_z, k_shrine_surface_offset, center.y());
      return placement;
    }
  }

  placement.world_position = center;
  return placement;
}

auto plan_undead_zone_shrines(const TerrainService& terrain,
                              const MapDefinition& map_definition)
    -> std::vector<UndeadShrinePlacement> {
  std::vector<UndeadShrinePlacement> placements;
  placements.reserve(map_definition.undead_zones.size());

  UndeadShrineExclusions exclusions;
  for (const auto& zone : map_definition.undead_zones) {
    auto placement = plan_undead_zone_shrine(terrain, map_definition, zone, exclusions);
    if (placement.prop_id != 0) {
      exclusions.claimed_prop_ids.insert(placement.prop_id);
    }
    if (placement.placed) {
      exclusions.reserved_sites.push_back(placement.world_position);
    }
    placements.push_back(std::move(placement));
  }

  return placements;
}

} // namespace Game::Map
