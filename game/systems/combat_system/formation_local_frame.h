#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

#include "../../core/component.h"
#include "../formation_combat_geometry.h"
#include "combat_random.h"

namespace Game::Systems::Combat {

struct LocalContactVector {
  float x{0.0F};
  float z{0.0F};
  float distance{0.0F};
  float yaw{0.0F};
};

[[nodiscard]] inline auto hash_unit_float(std::uint32_t seed,
                                          std::uint32_t salt) noexcept -> float {
  return hash_to_unit_open(seed ^ salt);
}

[[nodiscard]] inline auto
find_live_slot(const FormationCombat::FormationLayout& layout,
               std::uint16_t stable_index) -> const FormationCombat::SoldierSlot* {
  auto const found = std::lower_bound(
      layout.live_slots.begin(),
      layout.live_slots.end(),
      stable_index,
      [](auto const& slot, std::uint16_t index) { return slot.index < index; });
  return found != layout.live_slots.end() && found->index == stable_index ? &*found
                                                                          : nullptr;
}

[[nodiscard]] inline auto local_to_world(const Engine::Core::TransformComponent& actor,
                                         float local_x,
                                         float local_z) -> QVector3D {
  float const yaw = actor.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {actor.position.x + cos_yaw * local_x + sin_yaw * local_z,
          actor.position.y,
          actor.position.z - sin_yaw * local_x + cos_yaw * local_z};
}

[[nodiscard]] inline auto
local_contact_vector(const Engine::Core::TransformComponent& actor,
                     float source_local_x,
                     float source_local_z,
                     float target_world_x,
                     float target_world_z) -> LocalContactVector {
  float const actor_yaw = actor.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(actor_yaw);
  float const cos_yaw = std::cos(actor_yaw);
  float const source_world_x =
      actor.position.x + cos_yaw * source_local_x + sin_yaw * source_local_z;
  float const source_world_z =
      actor.position.z - sin_yaw * source_local_x + cos_yaw * source_local_z;
  float const world_x = target_world_x - source_world_x;
  float const world_z = target_world_z - source_world_z;

  LocalContactVector result;
  result.x = cos_yaw * world_x - sin_yaw * world_z;
  result.z = sin_yaw * world_x + cos_yaw * world_z;
  result.distance = std::hypot(result.x, result.z);
  if (result.distance > 0.0001F) {
    result.yaw = std::atan2(result.x, result.z) * 180.0F / std::numbers::pi_v<float>;
  }
  return result;
}

[[nodiscard]] inline auto
world_vector_to_local(const Engine::Core::TransformComponent& actor,
                      float world_x,
                      float world_z) -> std::pair<float, float> {
  float const yaw = actor.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {cos_yaw * world_x - sin_yaw * world_z, sin_yaw * world_x + cos_yaw * world_z};
}

[[nodiscard]] inline auto world_to_local(const Engine::Core::TransformComponent& actor,
                                         float world_x,
                                         float world_z) -> std::pair<float, float> {
  return world_vector_to_local(
      actor, world_x - actor.position.x, world_z - actor.position.z);
}

} // namespace Game::Systems::Combat
