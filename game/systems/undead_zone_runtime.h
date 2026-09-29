#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "map/map_definition.h"

namespace Engine::Core {
using EntityID = std::uint64_t;
}

namespace Game::Systems {

inline constexpr float k_undead_spawn_y_offset = 0.05F;

struct UndeadRuntimeZone {
  Game::Map::UndeadZone definition;
  std::vector<Game::Map::UndeadWave> authored_waves;
  QVector3D center_world;
  QVector3D anchor_world;
  QVector3D shrine_world;
  std::uint64_t anchor_world_prop_id = 0;
  std::uint64_t shrine_world_prop_id = 0;
  bool shrine_placed = false;
  Engine::Core::EntityID anchor_entity_id = 0;
  bool anchor_pending = false;
  bool awakened = false;
  int awakened_by_owner_id = 0;
  bool garrison_broken = false;
  bool announced_awakening = false;
  bool announced_defeat = false;
  int next_wave_index = 0;
  int completed_waves = 0;
  float respawn_delay_remaining = 0.0F;
  float current_wave_elapsed = 0.0F;
  float post_ring_phase_degrees = 0.0F;
  std::vector<Engine::Core::EntityID> active_spawn_ids;
};

[[nodiscard]] inline auto
undead_zone_origin(const UndeadRuntimeZone& zone) -> QVector3D {
  return (zone.anchor_world_prop_id != 0) ? zone.anchor_world : zone.center_world;
}

[[nodiscard]] inline auto
undead_waves_exhausted(const UndeadRuntimeZone& zone) -> bool {
  return zone.next_wave_index >= static_cast<int>(zone.definition.waves.size());
}

} // namespace Game::Systems
