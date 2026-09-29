#include "undead_zone_music.h"

#include "core/event_manager.h"
#include "core/world.h"
#include "core/world_spatial_index.h"
#include "game/systems/owner_registry.h"

namespace Game::Systems {

namespace {
constexpr const char* k_awakening_music = "music.event.skeletons_awaken";
constexpr float k_zone_music_poll_seconds = 0.25F;
} // namespace

UndeadZoneMusic::UndeadZoneMusic(const OwnerRegistry& owners)
    : m_owners(owners) {
}

auto UndeadZoneMusic::local_player_inside(Engine::Core::World& world,
                                          const UndeadRuntimeZone& zone) const -> bool {
  const int local_owner = m_owners.get_local_player_id();
  if (local_owner == 0) {
    return false;
  }
  bool inside = false;
  world.spatial_index().for_each_in_radius(
      zone.center_world.x(),
      zone.center_world.z(),
      zone.definition.radius,
      [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
        if (entry.health > 0 && entry.owner_id == local_owner) {
          inside = true;
        }
      });
  return inside;
}

auto UndeadZoneMusic::inside_a_woken_zone(
    Engine::Core::World& world,
    const std::vector<UndeadRuntimeZone>& zones) const -> bool {
  for (const auto& zone : zones) {
    const bool cleared = zone.active_spawn_ids.empty() && undead_waves_exhausted(zone);
    if (!zone.awakened || zone.garrison_broken || cleared) {
      continue;
    }
    const int local_owner = m_owners.get_local_player_id();
    if (zone.awakened_by_owner_id != 0 && zone.awakened_by_owner_id != local_owner) {
      continue;
    }
    if (local_player_inside(world, zone)) {
      return true;
    }
  }
  return false;
}

void UndeadZoneMusic::update(Engine::Core::World& world,
                             const std::vector<UndeadRuntimeZone>& zones,
                             float delta_time) {
  m_poll += delta_time;
  if (m_poll < k_zone_music_poll_seconds) {
    return;
  }
  m_poll = 0.0F;

  const bool inside = inside_a_woken_zone(world, zones);
  if (inside == m_playing) {
    return;
  }
  m_playing = inside;
  if (inside) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::MusicTriggerEvent(k_awakening_music));
  } else {
    Engine::Core::EventManager::instance().publish(Engine::Core::MusicStopEvent());
  }
}

} // namespace Game::Systems
