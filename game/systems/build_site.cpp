#include "build_site.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/world.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"

namespace Game::Systems {

namespace {

constexpr float k_site_margin = 0.35F;

constexpr float k_level_ground_tolerance = 0.85F;

constexpr float k_wall_ground_tolerance = 2.0F;

[[nodiscard]] auto tolerance_for(const std::string& building_type) -> float {
  return building_type == "wall_segment" || building_type == "gate"
             ? k_wall_ground_tolerance
             : k_level_ground_tolerance;
}

[[nodiscard]] auto obstructed_by_a_prop(const Game::Map::TerrainService& terrain,
                                        float x,
                                        float z,
                                        float reach) -> bool {
  for (const auto& prop : terrain.world_props()) {
    if (!Game::Map::is_solid_world_prop_type(prop.type) ||
        Game::Map::is_tree_world_prop_type(prop.type)) {

      continue;
    }
    const float clearance =
        reach + Game::Map::world_prop_ground_radius(prop.type, prop.scale);
    const float dx = prop.x - x;
    const float dz = prop.z - z;
    if (std::abs(dx) > clearance || std::abs(dz) > clearance) {
      continue;
    }
    if ((dx * dx) + (dz * dz) <= clearance * clearance) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] auto
touches_water(const Game::Map::TerrainHeightMap& terrain, float x, float z) -> bool {
  for (const auto& river : terrain.get_river_segments()) {
    const QVector3D on_river = Game::Map::closest_point_on_segment(
        QVector3D(x, 0.0F, z), river.start, river.end);
    const float distance = std::hypot(on_river.x() - x, on_river.z() - z);
    if (distance <= Game::Map::river_bank_standing_half_width(river.width)) {
      return true;
    }
  }
  for (const auto& lake : terrain.get_lakes()) {
    if (Game::Map::point_in_lake(lake, x, z, Game::Map::k_water_bank_clearance)) {
      return true;
    }
  }
  return false;
}

constexpr float k_wall_link_min_spacing = 0.75F * k_wall_link_spacing;

[[nodiscard]] auto
wall_link_site_occupied(const BuildingCollisionRegistry& collision,
                        float x,
                        float z,
                        float half_width,
                        float half_depth,
                        Engine::Core::EntityID ignore_entity_id) -> bool {
  const float reach = std::max(half_width, half_depth);
  bool occupied = false;
  collision.for_each_building_in_region(
      x - reach, x + reach, z - reach, z + reach, [&](const BuildingFootprint& other) {
        if (occupied || !other.blocks_navigation ||
            (ignore_entity_id != 0 && other.entity_id == ignore_entity_id)) {
          return;
        }
        if (other.wall_link) {

          const float dx = other.center_x - x;
          const float dz = other.center_z - z;
          occupied =
              (dx * dx) + (dz * dz) < k_wall_link_min_spacing * k_wall_link_min_spacing;
          return;
        }
        const float other_half_width = other.width * 0.5F;
        const float other_half_depth = other.depth * 0.5F;
        occupied = other.center_x - other_half_width < x + half_width &&
                   other.center_x + other_half_width > x - half_width &&
                   other.center_z - other_half_depth < z + half_depth &&
                   other.center_z + other_half_depth > z - half_depth;
      });
  if (occupied) {
    return true;
  }
  return std::any_of(
      collision.authored_obstacles().begin(),
      collision.authored_obstacles().end(),
      [&](const BuildingFootprint& obstacle) {
        const float obstacle_half_width = obstacle.width * 0.5F;
        const float obstacle_half_depth = obstacle.depth * 0.5F;
        return obstacle.center_x - obstacle_half_width < x + half_width &&
               obstacle.center_x + obstacle_half_width > x - half_width &&
               obstacle.center_z - obstacle_half_depth < z + half_depth &&
               obstacle.center_z + obstacle_half_depth > z - half_depth;
      });
}

} // namespace

auto assess_ground(const Engine::Core::World& world,
                   const std::string& building_type,
                   float x,
                   float z,
                   Engine::Core::EntityID ignore_entity_id,
                   float facing_degrees) -> GroundVerdict {
  const auto size = BuildingCollisionRegistry::axis_aligned_size(
      BuildingCollisionRegistry::get_building_size(building_type), facing_degrees);
  const float half_width = (size.width * 0.5F) + k_site_margin;
  const float half_depth = (size.depth * 0.5F) + k_site_margin;

  const auto& collision = *Game::Session::services_for(world).building_collision;
  if (is_wall_link_building_type(building_type)) {

    if (wall_link_site_occupied(
            collision, x, z, half_width, half_depth, ignore_entity_id)) {
      return GroundVerdict::Occupied;
    }
  } else if (collision.is_rect_overlapping_blocking_building(x - half_width,
                                                             x + half_width,
                                                             z - half_depth,
                                                             z + half_depth,
                                                             ignore_entity_id)) {
    return GroundVerdict::Occupied;
  }

  const auto& terrain_service = *Game::Session::services_for(world).terrain;
  const auto* terrain = terrain_service.get_height_map();

  const float step =
      terrain != nullptr ? std::max(terrain->get_tile_size(), 0.5F) : 1.0F;
  const int steps_x = std::max(1, static_cast<int>(std::ceil(half_width / step)));
  const int steps_z = std::max(1, static_cast<int>(std::ceil(half_depth / step)));

  float lowest = std::numeric_limits<float>::max();
  float highest = std::numeric_limits<float>::lowest();

  for (int ix = -steps_x; ix <= steps_x; ++ix) {
    for (int iz = -steps_z; iz <= steps_z; ++iz) {
      const float sample_x =
          x + (half_width * static_cast<float>(ix) / static_cast<float>(steps_x));
      const float sample_z =
          z + (half_depth * static_cast<float>(iz) / static_cast<float>(steps_z));

      if (terrain != nullptr) {
        const float reach_x = terrain->get_width() * terrain->get_tile_size() * 0.5F;
        const float reach_z = terrain->get_height() * terrain->get_tile_size() * 0.5F;
        if (std::abs(sample_x) > reach_x || std::abs(sample_z) > reach_z) {
          return GroundVerdict::OffMap;
        }
        if (touches_water(*terrain, sample_x, sample_z)) {
          return GroundVerdict::Water;
        }
        const float height = terrain->get_height_at(sample_x, sample_z);
        lowest = std::min(lowest, height);
        highest = std::max(highest, height);
      }
    }
  }

  if (obstructed_by_a_prop(terrain_service, x, z, std::hypot(half_width, half_depth))) {
    return GroundVerdict::Impassable;
  }

  if (terrain != nullptr && highest - lowest > tolerance_for(building_type)) {
    return GroundVerdict::Uneven;
  }

  return GroundVerdict::Clear;
}

void clear_ground_for(Engine::Core::World& world,
                      const std::string& building_type,
                      const QVector3D& position) {
  auto& terrain = *Game::Session::services_for(world).terrain;
  const auto size = BuildingCollisionRegistry::get_building_size(building_type);
  const float reach = std::hypot((size.width * 0.5F) + k_site_margin,
                                 (size.depth * 0.5F) + k_site_margin);

  std::vector<std::uint64_t> felled;
  for (const auto& prop : terrain.world_props()) {
    if (!Game::Map::is_tree_world_prop_type(prop.type)) {
      continue;
    }
    const float clearance =
        reach + Game::Map::world_prop_ground_radius(prop.type, prop.scale);
    const float dx = prop.x - position.x();
    const float dz = prop.z - position.z();
    if ((dx * dx) + (dz * dz) <= clearance * clearance) {
      felled.push_back(prop.id);
    }
  }
  for (const auto id : felled) {
    terrain.harvest_world_prop(id);
  }
}

auto find_clear_site(const Engine::Core::World& world,
                     const std::string& building_type,
                     const QVector3D& wanted,
                     float search_radius,
                     float facing_degrees) -> std::optional<QVector3D> {
  if (assess_ground(world, building_type, wanted.x(), wanted.z(), 0, facing_degrees) ==
      GroundVerdict::Clear) {
    return wanted;
  }
  if (search_radius <= 0.0F) {
    return std::nullopt;
  }

  constexpr int k_rings = 6;
  constexpr int k_bearings = 16;
  for (int ring = 1; ring <= k_rings; ++ring) {
    const float radius =
        search_radius * static_cast<float>(ring) / static_cast<float>(k_rings);
    for (int bearing = 0; bearing < k_bearings; ++bearing) {
      const float angle =
          (6.2831853F * static_cast<float>(bearing) / static_cast<float>(k_bearings)) +
          (static_cast<float>(ring) * 0.26F);
      const QVector3D candidate(wanted.x() + (radius * std::cos(angle)),
                                wanted.y(),
                                wanted.z() + (radius * std::sin(angle)));
      if (assess_ground(
              world, building_type, candidate.x(), candidate.z(), 0, facing_degrees) ==
          GroundVerdict::Clear) {
        return candidate;
      }
    }
  }
  return std::nullopt;
}

} // namespace Game::Systems
