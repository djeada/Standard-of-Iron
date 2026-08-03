#include "undead_awakening_system.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QVector3D>
#include <qjsonarray.h>
#include <qjsonobject.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/component.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/map/undead_shrine_placement.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "units/factory.h"
#include "units/unit.h"

namespace Game::Systems {

namespace {

constexpr float k_between_wave_delay_seconds = 1.5F;
constexpr float k_spawn_y_offset = 0.05F;
constexpr float k_anchor_match_distance = 3.5F;

constexpr float k_golden_angle_radians = 2.3999632F;
constexpr float k_min_spawn_ring_radius = 2.0F;
constexpr float k_spawn_ring_fraction = 0.8F;
constexpr int k_spawn_placement_attempts = 12;

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

auto is_followup_wave_trigger(const QString& trigger) -> bool {
  return trigger == QStringLiteral("after_clear") ||
         trigger == QStringLiteral("on_clear") ||
         trigger == QStringLiteral("next_wave");
}

} // namespace

UndeadAwakeningSystem::UndeadAwakeningSystem() = default;

UndeadAwakeningSystem::~UndeadAwakeningSystem() = default;

void UndeadAwakeningSystem::ensure_factory_registry() {
  if (m_factory_registry) {
    return;
  }
  m_factory_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*m_factory_registry);
}

void UndeadAwakeningSystem::configure(const Game::Map::MapDefinition& map_definition) {
  ensure_factory_registry();

  m_zones.clear();
  m_zone_index.clear();
  m_zones.reserve(map_definition.undead_zones.size());

  auto const& terrain_service = Game::Map::TerrainService::instance();
  Game::Map::UndeadShrineExclusions shrine_exclusions;

  for (const auto& zone_definition : map_definition.undead_zones) {
    RuntimeZone zone;
    zone.definition = zone_definition;
    if (zone.definition.waves.empty()) {
      zone.definition.waves = Game::Map::default_undead_waves();
    }
    zone.center_world =
        Game::Map::undead_zone_center_world(map_definition, zone_definition);
    zone.center_world.setY(terrain_service.resolve_surface_world_y(
        zone.center_world.x(), zone.center_world.z(), k_spawn_y_offset));
    zone.anchor_world = zone.center_world;

    float best_distance_sq = k_anchor_match_distance * k_anchor_match_distance;
    for (const auto& prop : terrain_service.world_props()) {
      if (prop.type != zone_definition.anchor_type) {
        continue;
      }
      QVector3D const prop_pos =
          terrain_service.world_prop_world_position(prop, k_spawn_y_offset);
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

    place_zone_shrine(map_definition, zone, shrine_exclusions);
    if (zone.anchor_world_prop_id == 0 && zone.shrine_placed) {
      zone.anchor_world = zone.shrine_world;
    }
    zone.anchor_pending = zone.shrine_placed;

    ensure_zone_owner_registered(zone);
    m_zone_index.insert(zone.definition.id, static_cast<int>(m_zones.size()));
    m_zones.push_back(std::move(zone));
  }

  m_allow_mission_start_trigger = true;
}

void UndeadAwakeningSystem::restore_state(const QJsonArray& state) {
  for (const auto& value : state) {
    auto const obj = value.toObject();
    RuntimeZone* zone = find_zone_mutable(obj.value(QStringLiteral("id")).toString());
    if (zone == nullptr) {
      continue;
    }

    zone->awakened = obj.value(QStringLiteral("awakened")).toBool(zone->awakened);
    zone->announced_awakening = zone->awakened;
    zone->garrison_broken =
        obj.value(QStringLiteral("garrison_broken")).toBool(zone->garrison_broken);
    zone->announced_defeat = zone->garrison_broken;
    zone->anchor_entity_id = static_cast<Engine::Core::EntityID>(
        obj.value(QStringLiteral("anchor_entity_id")).toInt(0));

    zone->anchor_pending = false;
    zone->next_wave_index =
        obj.value(QStringLiteral("next_wave_index")).toInt(zone->next_wave_index);
    zone->completed_waves =
        obj.value(QStringLiteral("completed_waves")).toInt(zone->completed_waves);
    zone->respawn_delay_remaining =
        static_cast<float>(obj.value(QStringLiteral("respawn_delay_remaining"))
                               .toDouble(zone->respawn_delay_remaining));
    zone->current_wave_elapsed =
        static_cast<float>(obj.value(QStringLiteral("current_wave_elapsed"))
                               .toDouble(zone->current_wave_elapsed));
    zone->active_spawn_ids.clear();
    const auto ids = obj.value(QStringLiteral("active_spawn_ids")).toArray();
    zone->active_spawn_ids.reserve(ids.size());
    for (const auto& id_value : ids) {
      zone->active_spawn_ids.push_back(
          static_cast<Engine::Core::EntityID>(id_value.toInt()));
    }
  }

  m_allow_mission_start_trigger = false;
}

auto UndeadAwakeningSystem::serialize_state() const -> QJsonArray {
  QJsonArray array;
  for (const auto& zone : m_zones) {
    QJsonObject obj;
    obj[QStringLiteral("id")] = zone.definition.id;
    obj[QStringLiteral("awakened")] = zone.awakened;
    obj[QStringLiteral("garrison_broken")] = zone.garrison_broken;
    obj[QStringLiteral("anchor_entity_id")] = static_cast<int>(zone.anchor_entity_id);
    obj[QStringLiteral("next_wave_index")] = zone.next_wave_index;
    obj[QStringLiteral("completed_waves")] = zone.completed_waves;
    obj[QStringLiteral("respawn_delay_remaining")] = zone.respawn_delay_remaining;
    obj[QStringLiteral("current_wave_elapsed")] = zone.current_wave_elapsed;
    QJsonArray active_ids;
    for (Engine::Core::EntityID const id : zone.active_spawn_ids) {
      active_ids.append(static_cast<int>(id));
    }
    obj[QStringLiteral("active_spawn_ids")] = active_ids;
    array.append(obj);
  }
  return array;
}

void UndeadAwakeningSystem::ensure_zone_owner_registered(
    const RuntimeZone& zone) const {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  if (owners.get_owner_type(zone.definition.owner_id) == OwnerType::Neutral) {

    owners.register_owner_with_id(
        zone.definition.owner_id,
        OwnerType::AI,
        QCoreApplication::translate("UndeadAwakeningSystem", "Iron Sepulcher %1")
            .arg(zone.definition.id)
            .toStdString());
  }
  owners.set_owner_team(zone.definition.owner_id,
                        zone.definition.team_id > 0 ? zone.definition.team_id
                                                    : zone.definition.owner_id);
  owners.set_owner_color(zone.definition.owner_id, 0.62F, 0.64F, 0.71F);

  auto& nations = Game::Systems::NationRegistry::instance();
  nations.set_player_nation(zone.definition.owner_id,
                            Game::Systems::NationID::IronSepulcher);
  Game::Systems::GlobalStatsRegistry::instance().mark_game_start(
      zone.definition.owner_id);
}

void UndeadAwakeningSystem::place_zone_shrine(
    const Game::Map::MapDefinition& map_definition,
    RuntimeZone& zone,
    Game::Map::UndeadShrineExclusions& exclusions) const {
  auto& terrain_service = Game::Map::TerrainService::instance();

  auto const placement = Game::Map::plan_undead_zone_shrine(
      terrain_service, map_definition, zone.definition, exclusions);

  zone.shrine_placed = placement.placed;
  zone.shrine_world = placement.world_position;

  if (!placement.placed) {
    qWarning() << "UndeadAwakeningSystem: zone" << zone.definition.id
               << "has no clear ground for its shrine - the zone will raise no "
                  "capturable barracks";
    return;
  }

  if (placement.adopted_existing_prop) {
    zone.shrine_world_prop_id = placement.prop_id;
  } else {
    Game::Map::WorldProp shrine;
    shrine.type = Game::Map::WorldProp::Type::MagicShrine;
    shrine.persistent = true;
    zone.shrine_world_prop_id = terrain_service.add_world_prop_at_world(
        shrine, placement.world_position.x(), placement.world_position.z());
  }

  exclusions.claimed_prop_ids.insert(zone.shrine_world_prop_id);
  exclusions.reserved_sites.push_back(placement.world_position);
}

void UndeadAwakeningSystem::refresh_active_spawns(Engine::Core::World& world,
                                                  RuntimeZone& zone) const {
  zone.active_spawn_ids.erase(
      std::remove_if(zone.active_spawn_ids.begin(),
                     zone.active_spawn_ids.end(),
                     [&world](Engine::Core::EntityID id) {
                       auto* entity = world.get_entity(id);
                       auto* unit =
                           entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
                       return unit == nullptr || unit->health <= 0;
                     }),
      zone.active_spawn_ids.end());

  if (zone.awakened && zone.active_spawn_ids.empty() &&
      zone.completed_waves < zone.next_wave_index) {
    zone.completed_waves = zone.next_wave_index;
    zone.respawn_delay_remaining = k_between_wave_delay_seconds;
  }
}

auto UndeadAwakeningSystem::should_awaken_zone(Engine::Core::World& world,
                                               const RuntimeZone& zone) const -> bool {
  auto const& owners = Game::Systems::OwnerRegistry::instance();

  for (const auto& raw_trigger : zone.definition.awaken_on) {
    QString const trigger = raw_trigger.trimmed().toLower();
    if (should_trigger_on_mission_start(trigger)) {
      if (m_allow_mission_start_trigger) {
        return true;
      }
      continue;
    }

    if (!should_trigger_on_unit_entry(trigger)) {
      continue;
    }

    float const radius_sq = zone.definition.radius * zone.definition.radius;
    for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->health <= 0) {
        continue;
      }
      if (unit->owner_id == zone.definition.owner_id ||
          !owners.are_enemies(zone.definition.owner_id, unit->owner_id)) {
        continue;
      }
      if (!Game::Units::is_troop_spawn(unit->spawn_type)) {
        continue;
      }

      float const dx = transform->position.x - zone.center_world.x();
      float const dz = transform->position.z - zone.center_world.z();
      if (dx * dx + dz * dz <= radius_sq) {
        return true;
      }
    }
  }

  return false;
}

auto UndeadAwakeningSystem::can_spawn_wave(const RuntimeZone& zone) const -> bool {
  if (zone.next_wave_index >= static_cast<int>(zone.definition.waves.size())) {
    return false;
  }
  if (!zone.awakened || zone.garrison_broken) {
    return false;
  }

  bool const wave_cleared = zone.active_spawn_ids.empty();

  bool const wave_timed_out =
      zone.definition.wave_timeout_seconds > 0.0F &&
      zone.current_wave_elapsed >= zone.definition.wave_timeout_seconds;

  if (!wave_cleared && !wave_timed_out) {
    return false;
  }
  if (wave_cleared && zone.respawn_delay_remaining > 0.0F) {
    return false;
  }

  QString const trigger =
      zone.definition.waves[zone.next_wave_index].trigger.trimmed().toLower();
  if (zone.next_wave_index == 0) {
    return is_initial_wave_trigger(trigger);
  }
  return is_followup_wave_trigger(trigger);
}

auto UndeadAwakeningSystem::spawn_position_for_index(
    const RuntimeZone& zone, int spawn_index, int spawn_count) const -> QVector3D {
  auto const& terrain_service = Game::Map::TerrainService::instance();
  QVector3D const origin =
      (zone.anchor_world_prop_id != 0) ? zone.anchor_world : zone.center_world;

  float const outer_radius =
      std::max(k_min_spawn_ring_radius, zone.definition.radius * k_spawn_ring_fraction);
  int const total = std::max(1, spawn_count);

  float const normalized =
      std::sqrt((static_cast<float>(spawn_index) + 0.5F) / static_cast<float>(total));
  float const base_radius =
      std::max(k_min_spawn_ring_radius * 0.5F, normalized * outer_radius);
  float const base_angle = static_cast<float>(spawn_index) * k_golden_angle_radians;

  for (int attempt = 0; attempt < k_spawn_placement_attempts; ++attempt) {
    float const angle =
        base_angle + static_cast<float>(attempt) * (k_golden_angle_radians * 0.25F);
    float const sample_radius =
        std::min(outer_radius, base_radius + static_cast<float>(attempt) * 0.6F);
    float const world_x = origin.x() + std::cos(angle) * sample_radius;
    float const world_z = origin.z() + std::sin(angle) * sample_radius;
    if (terrain_service.is_initialized() &&
        terrain_service.is_forbidden_world(world_x, world_z)) {
      continue;
    }
    return terrain_service.resolve_surface_world_position(
        world_x, world_z, k_spawn_y_offset, origin.y());
  }

  return terrain_service.resolve_surface_world_position(
      origin.x(), origin.z(), k_spawn_y_offset, origin.y());
}

void UndeadAwakeningSystem::ensure_anchor_structure(Engine::Core::World& world,
                                                    RuntimeZone& zone) {
  if (!zone.anchor_pending || m_factory_registry == nullptr) {
    return;
  }
  zone.anchor_pending = false;

  ensure_zone_owner_registered(zone);

  Game::Units::SpawnParams params;
  params.position = zone.shrine_world;
  params.player_id = zone.definition.owner_id;
  params.spawn_type = Game::Units::SpawnType::Barracks;
  params.ai_controlled = true;
  params.nation_id = Game::Systems::NationID::IronSepulcher;
  params.is_initial_spawn = true;

  params.enables_production = false;
  params.max_population = 0;

  auto anchor =
      m_factory_registry->create(Game::Units::SpawnType::Barracks, world, params);
  if (!anchor) {
    return;
  }
  zone.anchor_entity_id = anchor->id();
}

void UndeadAwakeningSystem::refresh_anchor_structure(Engine::Core::World& world,
                                                     RuntimeZone& zone) {
  if (zone.anchor_entity_id == 0 || zone.garrison_broken) {
    return;
  }

  auto* entity = world.get_entity(zone.anchor_entity_id);
  auto* unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;

  if (unit == nullptr || unit->health <= 0) {
    break_garrison(world, zone, false);
    return;
  }
  if (unit->owner_id != zone.definition.owner_id) {
    break_garrison(world, zone, true);
  }
}

void UndeadAwakeningSystem::break_garrison(Engine::Core::World& world,
                                           RuntimeZone& zone,
                                           bool captured) {
  zone.garrison_broken = true;
  zone.respawn_delay_remaining = 0.0F;
  zone.next_wave_index = static_cast<int>(zone.definition.waves.size());
  zone.completed_waves = zone.next_wave_index;

  for (Engine::Core::EntityID const spawn_id : zone.active_spawn_ids) {
    auto* entity = world.get_entity(spawn_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    unit->health = 0;
    Engine::Core::get_or_add_component<Engine::Core::DeathAnimationComponent>(*entity);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(spawn_id, unit->owner_id, unit->spawn_type));
  }
  zone.active_spawn_ids.clear();

  if (!zone.announced_defeat) {
    zone.announced_defeat = true;
    Engine::Core::EventManager::instance().publish(
        Engine::Core::MissionAnnouncementEvent(
            captured ? QCoreApplication::translate(
                           "UndeadAwakeningSystem",
                           "The shrine answers to you now. Its dead fall still.")
                     : QCoreApplication::translate(
                           "UndeadAwakeningSystem",
                           "The shrine is broken. Every risen guardian crumbles.")));
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent("alert.objective_complete"));
  }
}

void UndeadAwakeningSystem::refresh_capture_lock(Engine::Core::World& world,
                                                 const RuntimeZone& zone) const {
  if (zone.anchor_entity_id == 0) {
    return;
  }
  auto* anchor = world.get_entity(zone.anchor_entity_id);
  if (anchor == nullptr) {
    return;
  }
  auto* capture =
      Engine::Core::get_or_add_component<Engine::Core::CaptureComponent>(*anchor);
  if (capture == nullptr) {
    return;
  }

  capture->capture_blocked = !zone.garrison_broken && !zone.active_spawn_ids.empty();
}

void UndeadAwakeningSystem::awaken_zone(Engine::Core::World& world, RuntimeZone& zone) {
  zone.awakened = true;
  zone.respawn_delay_remaining = 0.0F;
  ensure_zone_owner_registered(zone);
  zone.announced_awakening = true;
  zone.current_wave_elapsed = 0.0F;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent("combat.hit.generic", 0.8F));

  try_spawn_next_wave(world, zone);
}

void UndeadAwakeningSystem::try_spawn_next_wave(Engine::Core::World& world,
                                                RuntimeZone& zone) {
  if (!can_spawn_wave(zone) || m_factory_registry == nullptr) {
    return;
  }

  auto const& wave = zone.definition.waves[zone.next_wave_index];
  int wave_size = 0;
  for (const auto& unit_spawn : wave.units) {
    wave_size += std::max(0, unit_spawn.count);
  }

  int spawn_index = 0;
  for (const auto& unit_spawn : wave.units) {
    for (int i = 0; i < unit_spawn.count; ++i) {
      Game::Units::SpawnParams params;
      params.position = spawn_position_for_index(zone, spawn_index++, wave_size);
      params.player_id = zone.definition.owner_id;
      params.spawn_type = unit_spawn.type;
      params.ai_controlled = true;
      params.nation_id = Game::Systems::NationID::IronSepulcher;
      params.is_initial_spawn = false;
      auto unit = m_factory_registry->create(unit_spawn.type, world, params);
      if (!unit) {
        continue;
      }
      zone.active_spawn_ids.push_back(unit->id());
    }
  }

  zone.next_wave_index += 1;
  zone.current_wave_elapsed = 0.0F;
  zone.respawn_delay_remaining = 0.0F;
  announce_wave(zone);
}

void UndeadAwakeningSystem::announce_wave(const RuntimeZone& zone) const {
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

void UndeadAwakeningSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto& zone : m_zones) {
    ensure_anchor_structure(*world, zone);
    refresh_anchor_structure(*world, zone);
    refresh_active_spawns(*world, zone);

    if (!zone.garrison_broken && !zone.awakened && should_awaken_zone(*world, zone)) {
      awaken_zone(*world, zone);
    }

    if (zone.respawn_delay_remaining > 0.0F) {
      zone.respawn_delay_remaining =
          std::max(0.0F, zone.respawn_delay_remaining - delta_time);
    }

    if (zone.awakened && !zone.garrison_broken) {
      zone.current_wave_elapsed += delta_time;
      try_spawn_next_wave(*world, zone);
    }

    refresh_capture_lock(*world, zone);

    if (!zone.announced_defeat && zone.awakened && zone.active_spawn_ids.empty() &&
        zone.next_wave_index >= static_cast<int>(zone.definition.waves.size())) {
      zone.announced_defeat = true;
      Engine::Core::EventManager::instance().publish(
          Engine::Core::MissionAnnouncementEvent(QCoreApplication::translate(
              "UndeadAwakeningSystem",
              "The risen guardians are put down. The ground is quiet.")));
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AudioCueEvent("alert.objective_complete"));
    }
  }

  m_allow_mission_start_trigger = false;
}

auto UndeadAwakeningSystem::find_zone(const QString& zone_id) const
    -> const RuntimeZone* {
  auto const it = m_zone_index.find(zone_id);
  if (it == m_zone_index.end()) {
    return nullptr;
  }
  return &m_zones[it.value()];
}

auto UndeadAwakeningSystem::find_zone_mutable(const QString& zone_id) -> RuntimeZone* {
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
  return zone != nullptr && zone->shrine_placed && is_zone_cleared(zone_id);
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

auto UndeadAwakeningSystem::completed_wave_count(const QString& zone_id) const -> int {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr ? zone->completed_waves : 0;
}

} // namespace Game::Systems
