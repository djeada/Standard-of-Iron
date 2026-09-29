#include "builder_site_geometry.h"

#include <algorithm>

#include "../../building_collision_registry.h"
#include "../ai_utils.h"
#include "builder_catalog.h"

namespace Game::Systems::AI {

namespace {
constexpr float k_wall_slot_clearance = 1.45F;
constexpr float k_gate_half_span = 4.5F;
constexpr float MAP_EDGE_PADDING = 5.0F;
} // namespace

auto footprint_half_extent(const std::string& building_type) -> float {
  if (building_type == BUILDING_TYPE_WALL_GATE) {
    return k_gate_half_span;
  }
  const auto size = BuildingCollisionRegistry::get_building_size(building_type);
  return 0.5F * std::max(size.width, size.depth);
}

auto slot_clearance(const char* building_type,
                    Game::Units::SpawnType standing) -> float {
  if ((building_type == BUILDING_TYPE_WALL_SEGMENT ||
       building_type == BUILDING_TYPE_WALL_GATE) &&
      Game::Units::is_wall_network_spawn(standing)) {
    return k_wall_slot_clearance;
  }
  return footprint_half_extent(std::string(building_type)) +
         footprint_half_extent(Game::Units::spawn_typeToString(standing)) + k_slot_gap;
}

auto site_is_free(const AISnapshot& snapshot,
                  const char* building_type,
                  float world_x,
                  float world_z) -> bool {
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building) {
      continue;
    }
    const float clearance = slot_clearance(building_type, entity.spawn_type);
    if (distance_squared(entity.pos_x, 0.0F, entity.pos_z, world_x, 0.0F, world_z) <=
        clearance * clearance) {
      return false;
    }
  }
  return true;
}

void clamp_to_map_bounds(const AISnapshot& snapshot, float& x, float& z) {
  if (!snapshot.has_map_bounds) {
    return;
  }

  const float min_x = snapshot.map_min_x + MAP_EDGE_PADDING;
  const float max_x = snapshot.map_max_x - MAP_EDGE_PADDING;
  const float min_z = snapshot.map_min_z + MAP_EDGE_PADDING;
  const float max_z = snapshot.map_max_z - MAP_EDGE_PADDING;

  x = std::clamp(x, min_x, max_x);
  z = std::clamp(z, min_z, max_z);
}

} // namespace Game::Systems::AI
