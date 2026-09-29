#include "formation_terrain_constraint.h"

#include <cmath>
#include <numbers>

namespace Game::Systems::Combat {

auto constrain_step_to_ground(const Pathfinding& pathfinder,
                              QVector3D origin,
                              float& step_x,
                              float& step_z,
                              Pathfinding::Passability passability) -> bool {
  if (std::hypot(step_x, step_z) <= 0.0001F ||
      !pathfinder.is_world_position_walkable(origin, passability)) {
    return false;
  }
  auto clear = [&](float x, float z) {
    return pathfinder.is_world_segment_walkable(
        origin, origin + QVector3D(x, 0.0F, z), passability);
  };
  if (clear(step_x, step_z)) {
    return false;
  }
  for (float const degrees : {35.0F, -35.0F, 70.0F, -70.0F}) {
    float const radians = degrees * std::numbers::pi_v<float> / 180.0F;
    float const keep = std::cos(radians);
    float const x = (step_x * std::cos(radians) - step_z * std::sin(radians)) * keep;
    float const z = (step_x * std::sin(radians) + step_z * std::cos(radians)) * keep;
    if (clear(x, z)) {
      step_x = x;
      step_z = z;
      return true;
    }
  }
  if (clear(step_x, 0.0F)) {
    step_z = 0.0F;
  } else if (clear(0.0F, step_z)) {
    step_x = 0.0F;
  } else {
    step_x = 0.0F;
    step_z = 0.0F;
  }
  return true;
}

auto terrain_walkable_at(const Pathfinding& pathfinder, float x, float z) -> bool {
  auto const cell = pathfinder.world_to_grid(x, z);
  return pathfinder.is_terrain_walkable(cell.x, cell.y);
}

void pull_onto_terrain(const Pathfinding& pathfinder,
                       float anchor_x,
                       float anchor_z,
                       QVector3D& destination) {
  if (terrain_walkable_at(pathfinder, destination.x(), destination.z()) ||
      !terrain_walkable_at(pathfinder, anchor_x, anchor_z)) {
    return;
  }
  float const dx = anchor_x - destination.x();
  float const dz = anchor_z - destination.z();
  float const distance = std::hypot(dx, dz);
  constexpr float k_pull_step = 0.25F;
  for (float pulled = k_pull_step; pulled < distance; pulled += k_pull_step) {
    float const x = destination.x() + dx / distance * pulled;
    float const z = destination.z() + dz / distance * pulled;
    if (terrain_walkable_at(pathfinder, x, z)) {
      destination.setX(x);
      destination.setZ(z);
      return;
    }
  }
  destination.setX(anchor_x);
  destination.setZ(anchor_z);
}

auto constrain_step_to_terrain(const Pathfinding& pathfinder,
                               float from_x,
                               float from_z,
                               float& step_x,
                               float& step_z) -> bool {
  if (!terrain_walkable_at(pathfinder, from_x, from_z) ||
      terrain_walkable_at(pathfinder, from_x + step_x, from_z + step_z)) {
    return false;
  }
  if (terrain_walkable_at(pathfinder, from_x + step_x, from_z)) {
    step_z = 0.0F;
  } else if (terrain_walkable_at(pathfinder, from_x, from_z + step_z)) {
    step_x = 0.0F;
  } else {
    step_x = 0.0F;
    step_z = 0.0F;
  }
  return true;
}

} // namespace Game::Systems::Combat
