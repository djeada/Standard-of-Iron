#include "undead_zone_music.h"

#include "core/ambient_session.h"
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

auto UndeadZoneMusic::owners_in_woken_zones(
    Engine::Core::World& world,
    const std::vector<UndeadRuntimeZone>& zones) const -> std::set<int> {
  std::set<int> owners;
  for (const auto& zone : zones) {
    const bool cleared = zone.active_spawn_ids.empty() && undead_waves_exhausted(zone);
    if (!zone.awakened || zone.garrison_broken || cleared) {
      continue;
    }
    world.spatial_index().for_each_in_radius(
        zone.center_world.x(),
        zone.center_world.z(),
        zone.definition.radius,
        [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
          if (entry.health > 0 && m_owners.is_player(entry.owner_id) &&
              (zone.awakened_by_owner_id == 0 ||
               zone.awakened_by_owner_id == entry.owner_id)) {
            owners.insert(entry.owner_id);
          }
        });
  }
  return owners;
}

void UndeadZoneMusic::update(Engine::Core::World& world,
                             const std::vector<UndeadRuntimeZone>& zones,
                             float delta_time) {
  m_poll += delta_time;
  if (m_poll < k_zone_music_poll_seconds) {
    return;
  }
  m_poll = 0.0F;

  auto inside = owners_in_woken_zones(world, zones);
  auto& events = *Game::Session::services_for(world).events;
  for (const int owner : m_playing) {
    if (!inside.contains(owner)) {
      events.publish(Engine::Core::MusicStopEvent(owner));
    }
  }
  for (const int owner : inside) {
    if (!m_playing.contains(owner)) {
      events.publish(
          Engine::Core::MusicTriggerEvent(k_awakening_music, 1.0F, true, owner));
    }
  }
  m_playing.swap(inside);
}

} // namespace Game::Systems
