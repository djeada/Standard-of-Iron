#include "undead_awakening_system.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "core/component_core.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "core/world_spatial_index.h"
#include "game/map/terrain_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/spawn_flare.h"
#include "game/systems/undead_zone_persistence.h"
#include "units/factory.h"
#include "units/unit.h"

namespace Game::Systems {

namespace {

constexpr float k_announced_wave_delay_seconds = 5.0F;
constexpr float k_min_wave_multiplier = 0.1F;
constexpr float k_max_wave_multiplier = 8.0F;
constexpr float k_anchor_match_distance = 3.5F;

constexpr const char* k_awakening_cue = "alert.undead_awakening";

auto should_trigger_on_mission_start(const QString& trigger) -> bool {
  return trigger == QStringLiteral("mission_start") ||
         trigger == QStringLiteral("initial");
}

auto should_trigger_on_unit_entry(const QString& trigger) -> bool {
  return trigger == QStringLiteral("unit_enters_radius") ||
         trigger == QStringLiteral("player_enters_radius");
}

auto is_initial_wave_trigger(const QString& trigger) -> bool {
  return trigger.isEmpty() || trigger == QStringLiteral("initial") ||
         trigger == QStringLiteral("awaken") ||
         trigger == QStringLiteral("mission_start");
}

auto is_timed_wave_trigger(const QString& trigger) -> bool {
  return trigger == QStringLiteral("next_wave") ||
         trigger == QStringLiteral("after_timeout") ||
         trigger == QStringLiteral("timed");
}

auto is_followup_wave_trigger(const QString& trigger) -> bool {
  return trigger == QStringLiteral("after_clear") ||
         trigger == QStringLiteral("on_clear") || is_timed_wave_trigger(trigger);
}

} // namespace

UndeadAwakeningSystem::UndeadAwakeningSystem(Services services)
    : m_services(services)
    , m_shrine(services.terrain, services.owners, services.nations, services.stats)
    , m_guardians(services.terrain)
    , m_music(services.owners) {
}

UndeadAwakeningSystem::~UndeadAwakeningSystem() = default;

void UndeadAwakeningSystem::ensure_factory_registry() {
  if (m_factory_registry) {
    return;
  }
  m_factory_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*m_factory_registry);
}

void UndeadAwakeningSystem::match_anchor_prop(UndeadRuntimeZone& zone) const {
  auto const& terrain_service = m_services.terrain;
  float best_distance_sq = k_anchor_match_distance * k_anchor_match_distance;
  for (const auto& prop : terrain_service.world_props()) {
    if (prop.type != zone.definition.anchor_type) {
      continue;
    }
    QVector3D const prop_pos =
        terrain_service.world_prop_world_position(prop, k_undead_spawn_y_offset);
    float const dx = prop_pos.x() - zone.center_world.x();
    float const dz = prop_pos.z() - zone.center_world.z();
    float const distance_sq = dx * dx + dz * dz;
    if (distance_sq > best_distance_sq) {
      continue;
    }
    zone.anchor_world_prop_id = prop.id;
    zone.anchor_world = prop_pos;
    best_distance_sq = distance_sq;
  }
}

auto UndeadAwakeningSystem::build_zone(
    const Game::Map::MapDefinition& map_definition,
    const Game::Map::UndeadZone& zone_definition,
    Game::Map::UndeadShrineExclusions& shrine_exclusions) -> UndeadRuntimeZone {
  auto const& terrain_service = m_services.terrain;
  UndeadRuntimeZone zone;
  zone.definition = zone_definition;
  if (zone.definition.waves.empty()) {
    zone.definition.waves = Game::Map::default_undead_waves();
  }
  zone.authored_waves = zone.definition.waves;
  apply_wave_multiplier(zone);
  zone.center_world =
      Game::Map::undead_zone_center_world(map_definition, zone_definition);
  zone.center_world.setY(terrain_service.resolve_surface_world_y(
      zone.center_world.x(), zone.center_world.z(), k_undead_spawn_y_offset));
  zone.anchor_world = zone.center_world;

  match_anchor_prop(zone);

  m_shrine.place(map_definition, zone, shrine_exclusions);
  if (zone.anchor_world_prop_id == 0 && zone.shrine_placed) {
    zone.anchor_world = zone.shrine_world;
  }
  zone.anchor_pending = zone.shrine_placed;

  m_shrine.register_zone_owner(zone);
  return zone;
}

void UndeadAwakeningSystem::configure(const Game::Map::MapDefinition& map_definition) {
  ensure_factory_registry();

  m_zones.clear();
  m_zone_index.clear();
  m_zones.reserve(map_definition.undead_zones.size());

  Game::Map::UndeadShrineExclusions shrine_exclusions;
  for (const auto& zone_definition : map_definition.undead_zones) {
    UndeadRuntimeZone zone =
        build_zone(map_definition, zone_definition, shrine_exclusions);
    m_zone_index.insert(zone.definition.id, static_cast<int>(m_zones.size()));
    m_zones.push_back(std::move(zone));
  }

  m_allow_mission_start_trigger = true;
}

void UndeadAwakeningSystem::set_wave_multiplier(float multiplier) {
  m_wave_multiplier =
      std::clamp(multiplier, k_min_wave_multiplier, k_max_wave_multiplier);
  for (auto& zone : m_zones) {
    apply_wave_multiplier(zone);
  }
}

void UndeadAwakeningSystem::apply_wave_multiplier(UndeadRuntimeZone& zone) const {
  zone.definition.waves = zone.authored_waves;
  for (auto& wave : zone.definition.waves) {
    for (auto& unit_spawn : wave.units) {
      if (unit_spawn.count <= 0) {
        continue;
      }
      unit_spawn.count =
          std::max(1,
                   static_cast<int>(std::lround(static_cast<float>(unit_spawn.count) *
                                                m_wave_multiplier)));
    }
  }
}

auto UndeadAwakeningSystem::wave_squad_count(const QString& zone_id,
                                             int wave_index) const -> int {
  const UndeadRuntimeZone* zone = find_zone(zone_id);
  if (zone == nullptr || wave_index < 0 ||
      wave_index >= static_cast<int>(zone->definition.waves.size())) {
    return 0;
  }
  int total = 0;
  for (const auto& unit_spawn : zone->definition.waves[wave_index].units) {
    total += std::max(0, unit_spawn.count);
  }
  return total;
}

auto UndeadAwakeningSystem::would_wake_a_zone(float world_x,
                                              float world_z,
                                              float body_radius) const -> bool {
  return std::any_of(
      m_zones.begin(), m_zones.end(), [&](const UndeadRuntimeZone& zone) {
        if (zone.awakened || zone.garrison_broken) {
          return false;
        }
        const float reach = zone.definition.radius + std::max(0.0F, body_radius);
        const float dx = world_x - zone.center_world.x();
        const float dz = world_z - zone.center_world.z();
        return (dx * dx) + (dz * dz) < reach * reach;
      });
}

void UndeadAwakeningSystem::restore_state(const QJsonArray& state) {
  for (const auto value : state) {
    auto const obj = value.toObject();
    UndeadRuntimeZone* zone =
        find_zone_mutable(obj.value(QStringLiteral("id")).toString());
    if (zone == nullptr) {
      continue;
    }
    restore_undead_zone(*zone, obj);
  }

  m_allow_mission_start_trigger = false;
}

auto UndeadAwakeningSystem::serialize_state() const -> QJsonArray {
  QJsonArray array;
  for (const auto& zone : m_zones) {
    array.append(serialize_undead_zone(zone));
  }
  return array;
}

void UndeadAwakeningSystem::refresh_active_spawns(Engine::Core::World& world,
                                                  UndeadRuntimeZone& zone) const {
  zone.active_spawn_ids.erase(
      std::remove_if(zone.active_spawn_ids.begin(),
                     zone.active_spawn_ids.end(),
                     [&world](Engine::Core::EntityID id) {
                       auto* entity = world.get_entity(id);
                       auto* unit = entity != nullptr
                                        ? world.try_get<Engine::Core::UnitComponent>(
                                              entity->get_id())
                                        : nullptr;
                       return unit == nullptr || unit->health <= 0;
                     }),
      zone.active_spawn_ids.end());

  if (zone.awakened && zone.active_spawn_ids.empty() &&
      zone.completed_waves < zone.next_wave_index) {
    zone.completed_waves = zone.next_wave_index;
    begin_wave_interval(zone);
  }
}

void UndeadAwakeningSystem::begin_wave_interval(UndeadRuntimeZone& zone) const {
  zone.respawn_delay_remaining = std::max(0.0F, zone.definition.wave_delay_seconds);
  if (zone.next_wave_index >= static_cast<int>(zone.definition.waves.size()) ||
      zone.garrison_broken) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UndeadZonePhaseEvent(zone.definition.id,
                                         Engine::Core::UndeadZonePhase::Stirring,
                                         zone.definition.owner_id,
                                         zone.respawn_delay_remaining));
  if (zone.respawn_delay_remaining < k_announced_wave_delay_seconds) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::MissionAnnouncementEvent(QCoreApplication::translate(
          "UndeadAwakeningSystem",
          "The ground is moving under the dead. More are coming up.")));
}

auto UndeadAwakeningSystem::should_awaken_zone(Engine::Core::World& world,
                                               const UndeadRuntimeZone& zone) const
    -> std::optional<int> {
  auto const& owners = m_services.owners;

  for (const auto& raw_trigger : zone.definition.awaken_on) {
    QString const trigger = raw_trigger.trimmed().toLower();
    if (should_trigger_on_mission_start(trigger)) {
      if (m_allow_mission_start_trigger) {
        return 0;
      }
      continue;
    }

    if (!should_trigger_on_unit_entry(trigger)) {
      continue;
    }

    std::optional<int> entered;
    world.spatial_index().for_each_in_radius(
        zone.center_world.x(),
        zone.center_world.z(),
        zone.definition.radius,
        [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
          if (entered.has_value() || entry.health <= 0 ||
              entry.owner_id == zone.definition.owner_id ||
              !owners.are_enemies(zone.definition.owner_id, entry.owner_id)) {
            return;
          }
          const auto* unit = world.try_get<Engine::Core::UnitComponent>(entry.id);
          if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
            return;
          }
          entered = entry.owner_id;
        });
    if (entered.has_value()) {
      return entered;
    }
  }

  return std::nullopt;
}

auto UndeadAwakeningSystem::can_spawn_wave(const UndeadRuntimeZone& zone) const
    -> bool {
  if (zone.next_wave_index >= static_cast<int>(zone.definition.waves.size())) {
    return false;
  }
  if (!zone.awakened || zone.garrison_broken) {
    return false;
  }

  QString const trigger =
      zone.definition.waves[zone.next_wave_index].trigger.trimmed().toLower();
  if (zone.next_wave_index == 0) {
    return is_initial_wave_trigger(trigger);
  }
  if (!is_followup_wave_trigger(trigger)) {
    return false;
  }

  bool const wave_cleared = zone.active_spawn_ids.empty();
  if (wave_cleared) {
    return zone.respawn_delay_remaining <= 0.0F;
  }

  return is_timed_wave_trigger(trigger) &&
         zone.definition.wave_timeout_seconds > 0.0F &&
         zone.current_wave_elapsed >= zone.definition.wave_timeout_seconds;
}

void UndeadAwakeningSystem::awaken_zone(Engine::Core::World& world,
                                        UndeadRuntimeZone& zone,
                                        int woken_by) {
  zone.awakened = true;
  zone.awakened_by_owner_id = woken_by;
  zone.respawn_delay_remaining = 0.0F;
  m_shrine.register_zone_owner(zone);
  zone.announced_awakening = true;
  zone.current_wave_elapsed = 0.0F;

  try_spawn_next_wave(world, zone);

  QVector3D const origin = undead_zone_origin(zone);
  Engine::Core::EventManager::instance().publish(Engine::Core::UndeadZoneAwakenedEvent(
      zone.definition.id, origin.x(), origin.z(), zone.definition.owner_id, woken_by));
}

void UndeadAwakeningSystem::spawn_wave_units(Engine::Core::World& world,
                                             UndeadRuntimeZone& zone,
                                             const Game::Map::UndeadWave& wave) {
  int wave_size = 0;
  for (const auto& unit_spawn : wave.units) {
    wave_size += std::max(0, unit_spawn.count);
  }

  int spawn_index = 0;
  for (const auto& unit_spawn : wave.units) {
    for (int i = 0; i < unit_spawn.count; ++i) {
      Game::Units::SpawnParams params;
      params.position =
          m_guardians.spawn_position_for_index(zone, spawn_index++, wave_size);
      params.player_id = zone.definition.owner_id;
      params.spawn_type = unit_spawn.type;
      params.ai_controlled = true;
      params.nation_id = Game::Systems::NationID::IronSepulcher;
      params.is_initial_spawn = false;
      auto unit = m_factory_registry->create(unit_spawn.type, world, params);
      if (!unit) {
        continue;
      }

      attach_spawn_flare(
          world, unit->id(), unit_spawn.type, Engine::Core::SpawnFlareStyle::Awakening);
      zone.active_spawn_ids.push_back(unit->id());
    }
  }
}

void UndeadAwakeningSystem::try_spawn_next_wave(Engine::Core::World& world,
                                                UndeadRuntimeZone& zone) {
  if (!can_spawn_wave(zone) || m_factory_registry == nullptr) {
    return;
  }

  spawn_wave_units(world, zone, zone.definition.waves[zone.next_wave_index]);
  m_guardians.post_new_wave(world, zone);

  zone.next_wave_index += 1;
  zone.current_wave_elapsed = 0.0F;
  zone.respawn_delay_remaining = 0.0F;

  if (zone.awakened_by_owner_id == 0) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent(k_awakening_cue));
  } else {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent::for_owner(zone.awakened_by_owner_id,
                                               k_awakening_cue));
  }
  announce_wave(zone);
}

void UndeadAwakeningSystem::announce_wave(const UndeadRuntimeZone& zone) const {
  int const wave_number = zone.next_wave_index;
  int const wave_total = static_cast<int>(zone.definition.waves.size());
  QString const progress =
      QCoreApplication::translate("UndeadAwakeningSystem", "Wave %1/%2")
          .arg(wave_number)
          .arg(wave_total);

  QString const text = wave_number <= 1
                           ? QCoreApplication::translate(
                                 "UndeadAwakeningSystem",
                                 "The Iron Sepulcher wakes. %1 rises to meet you.")
                                 .arg(progress)
                           : QCoreApplication::translate("UndeadAwakeningSystem",
                                                         "%1 claws out of the ground.")
                                 .arg(progress);
  Engine::Core::EventManager::instance().publish(
      Engine::Core::MissionAnnouncementEvent(text));
}

void UndeadAwakeningSystem::announce_zone_cleared(UndeadRuntimeZone& zone) const {
  zone.announced_defeat = true;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UndeadZonePhaseEvent(zone.definition.id,
                                         Engine::Core::UndeadZonePhase::Cleared,
                                         zone.definition.owner_id));
  Engine::Core::EventManager::instance().publish(
      Engine::Core::MissionAnnouncementEvent(QCoreApplication::translate(
          "UndeadAwakeningSystem",
          "The risen guardians are put down. Hold the shrine to purify it.")));
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent("alert.objective_complete"));
}

void UndeadAwakeningSystem::update_zone(Engine::Core::World& world,
                                        UndeadRuntimeZone& zone,
                                        float delta_time) {
  m_shrine.ensure_anchor_structure(world, zone, m_factory_registry.get());
  m_shrine.refresh_anchor_structure(world, zone);
  refresh_active_spawns(world, zone);

  if (!zone.garrison_broken && !zone.awakened) {
    if (auto const woken_by = should_awaken_zone(world, zone)) {
      awaken_zone(world, zone, *woken_by);
    }
  }

  if (zone.respawn_delay_remaining > 0.0F) {
    zone.respawn_delay_remaining =
        std::max(0.0F, zone.respawn_delay_remaining - delta_time);
  }

  if (zone.awakened && !zone.garrison_broken) {
    zone.current_wave_elapsed += delta_time;
    try_spawn_next_wave(world, zone);
  }

  m_shrine.refresh_capture_lock(world, zone);

  if (!zone.announced_defeat && zone.awakened && zone.active_spawn_ids.empty() &&
      undead_waves_exhausted(zone)) {
    announce_zone_cleared(zone);
  }
}

void UndeadAwakeningSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  if (!m_zones.empty()) {
    world->spatial_index().refresh(*world);
  }

  for (auto& zone : m_zones) {
    update_zone(*world, zone, delta_time);
  }

  m_guardians.enforce_leashes(*world, m_zones, delta_time);
  m_music.update(*world, m_zones, delta_time);
  m_allow_mission_start_trigger = false;
}

auto UndeadAwakeningSystem::find_zone(const QString& zone_id) const
    -> const UndeadRuntimeZone* {
  auto const it = m_zone_index.find(zone_id);
  if (it == m_zone_index.end()) {
    return nullptr;
  }
  return &m_zones[it.value()];
}

auto UndeadAwakeningSystem::find_zone_mutable(const QString& zone_id)
    -> UndeadRuntimeZone* {
  auto const it = m_zone_index.find(zone_id);
  if (it == m_zone_index.end()) {
    return nullptr;
  }
  return &m_zones[it.value()];
}

auto UndeadAwakeningSystem::has_zone(const QString& zone_id) const -> bool {
  return find_zone(zone_id) != nullptr;
}

auto UndeadAwakeningSystem::is_zone_cleared(const QString& zone_id) const -> bool {
  auto const* zone = find_zone(zone_id);
  if (zone == nullptr) {
    return false;
  }

  if (zone->garrison_broken) {
    return true;
  }
  return zone->awakened &&
         zone->next_wave_index >= static_cast<int>(zone->definition.waves.size()) &&
         zone->active_spawn_ids.empty();
}

auto UndeadAwakeningSystem::is_shrine_purified(const QString& zone_id) const -> bool {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr && zone->shrine_placed && zone->garrison_broken;
}

auto UndeadAwakeningSystem::anchor_entity(const QString& zone_id) const
    -> Engine::Core::EntityID {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr ? zone->anchor_entity_id : 0U;
}

auto UndeadAwakeningSystem::has_shrine(const QString& zone_id) const -> bool {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr && zone->shrine_placed;
}

auto UndeadAwakeningSystem::shrine_world_position(const QString& zone_id) const
    -> QVector3D {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr ? zone->shrine_world : QVector3D{};
}

auto UndeadAwakeningSystem::shrine_prop_id(const QString& zone_id) const
    -> std::uint64_t {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr ? zone->shrine_world_prop_id : 0U;
}

auto UndeadAwakeningSystem::zones_without_shrine() const -> std::vector<QString> {
  std::vector<QString> zone_ids;
  for (const auto& zone : m_zones) {
    if (!zone.shrine_placed) {
      zone_ids.push_back(zone.definition.id);
    }
  }
  return zone_ids;
}

auto UndeadAwakeningSystem::shrine_markers() const -> std::vector<ShrineMarker> {
  std::vector<ShrineMarker> markers;
  markers.reserve(m_zones.size());
  for (const auto& zone : m_zones) {
    if (!zone.shrine_placed) {
      continue;
    }
    markers.push_back(ShrineMarker{zone.definition.id,
                                   zone.shrine_world,
                                   zone.awakened,
                                   is_zone_cleared(zone.definition.id)});
  }
  return markers;
}

auto UndeadAwakeningSystem::completed_wave_count(const QString& zone_id) const -> int {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr ? zone->completed_waves : 0;
}

} // namespace Game::Systems
