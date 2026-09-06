#include "build_site.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_gameplay.h"
#include "../core/world.h"
#include "../map/terrain.h"
#include "../map/terrain_service.h"
#include "../units/spawn_type.h"
#include "builder_product_types.h"
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
    if (!Game::Map::is_solid_world_prop_type(prop.type)) {
      continue;
    }
    const float clearance =
        reach + Game::Map::world_prop_ground_radius(prop.type, prop.scale);
    const QVector3D at = terrain.world_prop_world_position(prop);
    const float dx = at.x() - x;
    const float dz = at.z() - z;
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

[[nodiscard]] auto cell_verdict(const Game::Map::TerrainHeightMap& terrain,
                                float x,
                                float z) -> GroundVerdict {
  const int grid_x = static_cast<int>(
      std::lround((x / terrain.get_tile_size()) + (terrain.get_width() * 0.5F - 0.5F)));
  const int grid_z = static_cast<int>(std::lround(
      (z / terrain.get_tile_size()) + (terrain.get_height() * 0.5F - 0.5F)));
  if (grid_x < 0 || grid_z < 0 || grid_x >= terrain.get_width() ||
      grid_z >= terrain.get_height()) {
    return GroundVerdict::OffMap;
  }
  if (terrain.is_walkable(grid_x, grid_z)) {
    return GroundVerdict::Clear;
  }
  const auto type = terrain.getTerrainType(grid_x, grid_z);
  if (Game::Map::is_water_terrain(type)) {
    return GroundVerdict::Water;
  }
  if (type == Game::Map::TerrainType::Hill) {

    return GroundVerdict::Uneven;
  }
  return GroundVerdict::Impassable;
}

constexpr float k_wall_link_min_spacing = 0.75F * k_wall_link_spacing;

[[nodiscard]] auto rect_overlaps(const BuildingFootprint& footprint,
                                 float x,
                                 float z,
                                 float half_width,
                                 float half_depth) -> bool {
  const float other_half_width = footprint.width * 0.5F;
  const float other_half_depth = footprint.depth * 0.5F;
  return footprint.center_x - other_half_width < x + half_width &&
         footprint.center_x + other_half_width > x - half_width &&
         footprint.center_z - other_half_depth < z + half_depth &&
         footprint.center_z + other_half_depth > z - half_depth;
}

[[nodiscard]] auto
reserved_footprint(const std::string& building_type,
                   float x,
                   float z,
                   float facing_degrees,
                   Engine::Core::EntityID holder) -> BuildingFootprint {
  const auto size = BuildingCollisionRegistry::axis_aligned_size(
      BuildingCollisionRegistry::get_building_size(building_type), facing_degrees);
  BuildingFootprint footprint(x, z, size.width, size.depth, 0, holder);
  footprint.wall_link = is_wall_link_building_type(building_type);
  footprint.wall_tower = is_wall_tower_building_type(building_type);
  return footprint;
}

[[nodiscard]] auto same_site(const BuildingFootprint& site,
                             const std::string& building_type,
                             float x,
                             float z) -> bool {
  constexpr float k_same_site_epsilon = 0.05F;
  return site.wall_link == is_wall_link_building_type(building_type) &&
         std::abs(site.center_x - x) < k_same_site_epsilon &&
         std::abs(site.center_z - z) < k_same_site_epsilon;
}

[[nodiscard]] auto reserves_ground(const std::string& product_type) -> bool {

  return !is_gather_builder_product(product_type) &&
         product_type != k_builder_product_repair &&
         product_type != k_builder_product_dismantle;
}

[[nodiscard]] auto pending_sites(const Engine::Core::World& world,
                                 std::span<const Engine::Core::EntityID> crew)
    -> std::vector<BuildingFootprint> {
  std::vector<BuildingFootprint> sites;
  const auto is_crew = [crew](Engine::Core::EntityID id) {
    return std::find(crew.begin(), crew.end(), id) != crew.end();
  };
  for (const auto id :
       world.entities_with<Engine::Core::BuilderProductionComponent>()) {
    const auto* builder = world.try_get<Engine::Core::BuilderProductionComponent>(id);
    if (builder == nullptr || !builder->has_construction_site ||
        builder->construction_site_entity_id != 0 || builder->product_type.empty() ||
        !reserves_ground(builder->product_type) || is_crew(id)) {
      continue;
    }
    sites.push_back(reserved_footprint(builder->product_type,
                                       builder->construction_site_x,
                                       builder->construction_site_z,
                                       builder->construction_site_rotation_y,
                                       id));
  }
  for (const auto id :
       world.entities_with<Engine::Core::WallConstructionSiteComponent>()) {
    const auto* site = world.try_get<Engine::Core::WallConstructionSiteComponent>(id);
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
    if (site == nullptr || transform == nullptr) {
      continue;
    }
    sites.push_back(
        reserved_footprint(Game::Units::spawn_typeToString(site->product_type),
                           transform->position.x,
                           transform->position.z,
                           transform->rotation.y,
                           id));
  }
  return sites;
}

[[nodiscard]] auto site_occupied(const BuildingCollisionRegistry& collision,
                                 std::span<const BuildingFootprint> reserved,
                                 const std::string& building_type,
                                 float x,
                                 float z,
                                 float half_width,
                                 float half_depth,
                                 Engine::Core::EntityID ignore_entity_id) -> bool {
  const bool wall_link = is_wall_link_building_type(building_type);
  const bool wall_tower = is_wall_tower_building_type(building_type);
  const auto blocked_by = [&](const BuildingFootprint& other) {
    if (ignore_entity_id != 0 && other.entity_id == ignore_entity_id) {
      return false;
    }

    const bool chained = (wall_link && (other.wall_link || other.wall_tower)) ||
                         (wall_tower && other.wall_link);
    if (chained) {

      const float dx = other.center_x - x;
      const float dz = other.center_z - z;
      return (dx * dx) + (dz * dz) < k_wall_link_min_spacing * k_wall_link_min_spacing;
    }
    return rect_overlaps(other, x, z, half_width, half_depth);
  };

  const float reach = std::max(half_width, half_depth);
  bool occupied = false;
  collision.for_each_building_in_region(
      x - reach, x + reach, z - reach, z + reach, [&](const BuildingFootprint& other) {
        if (!occupied && other.blocks_navigation) {
          occupied = blocked_by(other);
        }
      });
  if (occupied) {
    return true;
  }
  if (std::any_of(reserved.begin(), reserved.end(), [&](const BuildingFootprint& site) {
        return !same_site(site, building_type, x, z) && blocked_by(site);
      })) {
    return true;
  }
  return std::any_of(collision.authored_obstacles().begin(),
                     collision.authored_obstacles().end(),
                     [&](const BuildingFootprint& obstacle) {
                       return rect_overlaps(obstacle, x, z, half_width, half_depth);
                     });
}

[[nodiscard]] auto assess_ground(const Engine::Core::World& world,
                                 std::span<const BuildingFootprint> reserved,
                                 const std::string& building_type,
                                 float x,
                                 float z,
                                 Engine::Core::EntityID ignore_entity_id,
                                 float facing_degrees) -> GroundVerdict;

auto assess_ground(const Engine::Core::World& world,
                   std::span<const BuildingFootprint> reserved,
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
  if (site_occupied(collision,
                    reserved,
                    building_type,
                    x,
                    z,
                    half_width,
                    half_depth,
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
        if (const auto cell = cell_verdict(*terrain, sample_x, sample_z);
            cell != GroundVerdict::Clear) {
          return cell;
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

} // namespace

auto assess_ground(const Engine::Core::World& world,
                   const std::string& building_type,
                   float x,
                   float z,
                   Engine::Core::EntityID ignore_entity_id,
                   float facing_degrees,
                   std::span<const Engine::Core::EntityID> crew) -> GroundVerdict {
  const auto reserved = pending_sites(world, crew);
  return assess_ground(
      world, reserved, building_type, x, z, ignore_entity_id, facing_degrees);
}

auto find_clear_site(const Engine::Core::World& world,
                     const std::string& building_type,
                     const QVector3D& wanted,
                     float search_radius,
                     float facing_degrees,
                     std::span<const Engine::Core::EntityID> crew)
    -> std::optional<QVector3D> {
  const auto reserved = pending_sites(world, crew);
  if (assess_ground(
          world, reserved, building_type, wanted.x(), wanted.z(), 0, facing_degrees) ==
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
      if (assess_ground(world,
                        reserved,
                        building_type,
                        candidate.x(),
                        candidate.z(),
                        0,
                        facing_degrees) == GroundVerdict::Clear) {
        return candidate;
      }
    }
  }
  return std::nullopt;
}

auto wall_ground_probe(const Engine::Core::World& world) -> GroundProbe {
  return [&world](float world_x,
                  float world_z,
                  const std::string& building_type,
                  Engine::Core::EntityID ignore_entity_id) {
    return assess_ground(world, building_type, world_x, world_z, ignore_entity_id);
  };
}

} // namespace Game::Systems
