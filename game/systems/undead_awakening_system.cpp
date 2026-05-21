#include "undead_awakening_system.h"

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
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "units/factory.h"
#include "units/unit.h"

namespace Game::Systems {

namespace {

constexpr float k_grid_to_world_epsilon = 0.0001F;
constexpr float k_between_wave_delay_seconds = 1.5F;
constexpr float k_spawn_y_offset = 0.05F;
constexpr float k_anchor_match_distance = 3.5F;

auto authored_to_world(float coord, int grid_size, float tile_size) -> float {
  float const safe_tile = std::max(tile_size, k_grid_to_world_epsilon);
  return (coord - (static_cast<float>(grid_size) * 0.5F - 0.5F)) * safe_tile;
}

auto authored_to_world_position(const Game::Map::MapDefinition& map_definition,
                                float x,
                                float z) -> QVector3D {
  if (map_definition.coordSystem == Game::Map::CoordSystem::World) {
    return {x, 0.0F, z};
  }
  return {
      authored_to_world(x, map_definition.grid.width, map_definition.grid.tile_size),
      0.0F,
      authored_to_world(z, map_definition.grid.height, map_definition.grid.tile_size)};
}

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
  auto const& world_props = terrain_service.world_props();

  for (const auto& zone_definition : map_definition.undead_zones) {
    RuntimeZone zone;
    zone.definition = zone_definition;
    zone.center_world = authored_to_world_position(
        map_definition, zone_definition.x, zone_definition.z);
    zone.center_world.setY(terrain_service.resolve_surface_world_y(
        zone.center_world.x(), zone.center_world.z(), k_spawn_y_offset));
    zone.anchor_world = zone.center_world;

    float best_distance_sq = k_anchor_match_distance * k_anchor_match_distance;
    for (const auto& prop : world_props) {
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
    zone->next_wave_index =
        obj.value(QStringLiteral("next_wave_index")).toInt(zone->next_wave_index);
    zone->completed_waves =
        obj.value(QStringLiteral("completed_waves")).toInt(zone->completed_waves);
    zone->respawn_delay_remaining =
        static_cast<float>(obj.value(QStringLiteral("respawn_delay_remaining"))
                               .toDouble(zone->respawn_delay_remaining));
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
    obj[QStringLiteral("next_wave_index")] = zone.next_wave_index;
    obj[QStringLiteral("completed_waves")] = zone.completed_waves;
    obj[QStringLiteral("respawn_delay_remaining")] = zone.respawn_delay_remaining;
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
        QStringLiteral("Iron Sepulcher %1").arg(zone.definition.id).toStdString());
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
  if (!zone.awakened || !zone.active_spawn_ids.empty() ||
      zone.respawn_delay_remaining > 0.0F) {
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
    const RuntimeZone& zone, int spawn_index) const -> QVector3D {
  auto const& terrain_service = Game::Map::TerrainService::instance();
  QVector3D const origin =
      (zone.anchor_world_prop_id != 0) ? zone.anchor_world : zone.center_world;
  float const zone_radius = std::max(1.5F, zone.definition.radius * 0.55F);

  for (int attempt = 0; attempt < 16; ++attempt) {
    float const angle = static_cast<float>(spawn_index * 3 + attempt) * 2.3999632F;
    float const ring = 1.0F + static_cast<float>((spawn_index + attempt) % 4);
    float const sample_radius = std::min(
        zone_radius, 1.25F + ring * std::max(0.8F, zone.definition.radius * 0.15F));
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

void UndeadAwakeningSystem::awaken_zone(Engine::Core::World& world, RuntimeZone& zone) {
  zone.awakened = true;
  zone.respawn_delay_remaining = 0.0F;
  ensure_zone_owner_registered(zone);
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioTriggerEvent("combat_hit_generic", 0.8F, false, 4));
  try_spawn_next_wave(world, zone);
}

void UndeadAwakeningSystem::try_spawn_next_wave(Engine::Core::World& world,
                                                RuntimeZone& zone) {
  if (!can_spawn_wave(zone) || m_factory_registry == nullptr) {
    return;
  }

  auto const& wave = zone.definition.waves[zone.next_wave_index];
  int spawn_index = 0;
  for (const auto& unit_spawn : wave.units) {
    for (int i = 0; i < unit_spawn.count; ++i) {
      Game::Units::SpawnParams params;
      params.position = spawn_position_for_index(zone, spawn_index++);
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
}

void UndeadAwakeningSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto& zone : m_zones) {
    refresh_active_spawns(*world, zone);

    if (!zone.awakened && should_awaken_zone(*world, zone)) {
      awaken_zone(*world, zone);
    }

    if (zone.respawn_delay_remaining > 0.0F) {
      zone.respawn_delay_remaining =
          std::max(0.0F, zone.respawn_delay_remaining - delta_time);
    }

    if (zone.awakened) {
      try_spawn_next_wave(*world, zone);
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
  return zone != nullptr && zone->awakened &&
         zone->next_wave_index >= static_cast<int>(zone->definition.waves.size()) &&
         zone->active_spawn_ids.empty();
}

auto UndeadAwakeningSystem::is_shrine_purified(const QString& zone_id) const -> bool {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr &&
         zone->definition.anchor_type == Game::Map::WorldProp::Type::MagicShrine &&
         is_zone_cleared(zone_id);
}

auto UndeadAwakeningSystem::completed_wave_count(const QString& zone_id) const -> int {
  auto const* zone = find_zone(zone_id);
  return zone != nullptr ? zone->completed_waves : 0;
}

} // namespace Game::Systems
