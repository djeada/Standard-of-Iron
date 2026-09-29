#include "undead_shrine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/death_sequence.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_feedback.h"
#include "units/factory.h"
#include "units/unit.h"

namespace Game::Systems {

namespace {

auto anchor_owner(Engine::Core::World& world,
                  Engine::Core::EntityID anchor_entity_id) -> int {
  if (anchor_entity_id == 0) {
    return Engine::Core::k_owner_everyone;
  }
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(anchor_entity_id);
  if (unit == nullptr || Game::Core::is_neutral_owner(unit->owner_id)) {
    return Engine::Core::k_owner_everyone;
  }
  return unit->owner_id;
}

} // namespace

UndeadShrine::UndeadShrine(Game::Map::TerrainService& terrain,
                           OwnerRegistry& owners,
                           NationRegistry& nations,
                           GlobalStatsRegistry& stats)
    : m_terrain(terrain)
    , m_owners(owners)
    , m_nations(nations)
    , m_stats(stats) {
}

void UndeadShrine::register_zone_owner(const UndeadRuntimeZone& zone) const {
  auto& owners = m_owners;
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

  auto& nations = m_nations;
  nations.set_player_nation(zone.definition.owner_id,
                            Game::Systems::NationID::IronSepulcher);
  m_stats.mark_game_start(zone.definition.owner_id);
}

void UndeadShrine::place(const Game::Map::MapDefinition& map_definition,
                         UndeadRuntimeZone& zone,
                         Game::Map::UndeadShrineExclusions& exclusions) const {
  auto& terrain_service = m_terrain;

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

void UndeadShrine::ensure_anchor_structure(
    Engine::Core::World& world,
    UndeadRuntimeZone& zone,
    Game::Units::UnitFactoryRegistry* factories) const {
  if (!zone.anchor_pending || factories == nullptr) {
    return;
  }
  zone.anchor_pending = false;

  register_zone_owner(zone);

  Game::Units::SpawnParams params;
  params.position = zone.shrine_world;
  params.player_id = zone.definition.owner_id;
  params.spawn_type = Game::Units::SpawnType::Barracks;
  params.ai_controlled = true;
  params.nation_id = Game::Systems::NationID::IronSepulcher;
  params.is_initial_spawn = true;

  params.enables_production = false;
  params.max_population = 0;

  auto anchor = factories->create(Game::Units::SpawnType::Barracks, world, params);
  if (!anchor) {
    return;
  }
  zone.anchor_entity_id = anchor->id();
}

void UndeadShrine::refresh_anchor_structure(Engine::Core::World& world,
                                            UndeadRuntimeZone& zone) const {
  if (zone.anchor_entity_id == 0 || zone.garrison_broken) {
    return;
  }

  auto* entity = world.get_entity(zone.anchor_entity_id);
  auto* unit = entity != nullptr
                   ? world.try_get<Engine::Core::UnitComponent>(entity->get_id())
                   : nullptr;

  if (unit == nullptr || unit->health <= 0) {
    break_garrison(world, zone, false);
    return;
  }
  if (unit->owner_id != zone.definition.owner_id) {
    break_garrison(world, zone, true);
  }
}

void UndeadShrine::break_garrison(Engine::Core::World& world,
                                  UndeadRuntimeZone& zone,
                                  bool captured) const {
  zone.garrison_broken = true;
  zone.respawn_delay_remaining = 0.0F;
  zone.next_wave_index = static_cast<int>(zone.definition.waves.size());
  zone.completed_waves = zone.next_wave_index;

  for (Engine::Core::EntityID const spawn_id : zone.active_spawn_ids) {
    auto* entity = world.get_entity(spawn_id);
    auto* unit = entity != nullptr
                     ? world.try_get<Engine::Core::UnitComponent>(entity->get_id())
                     : nullptr;
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    unit->health = 0;
    Engine::Core::begin_death_sequence(*entity, 0U);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(spawn_id, unit->owner_id, unit->spawn_type));
  }
  zone.active_spawn_ids.clear();

  if (!zone.announced_defeat) {
    zone.announced_defeat = true;

    int const victor = captured ? anchor_owner(world, zone.anchor_entity_id)
                                : Engine::Core::k_owner_everyone;
    Engine::Core::EventManager::instance().publish(
        Engine::Core::MissionAnnouncementEvent::for_owner(
            victor,
            captured ? QCoreApplication::translate(
                           "UndeadAwakeningSystem",
                           "The shrine answers to you now. Its dead fall still.")
                     : QCoreApplication::translate(
                           "UndeadAwakeningSystem",
                           "The shrine is broken. Every risen guardian crumbles.")));
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent::for_owner(victor, "alert.objective_complete"));
  }

  pay_clear_reward(world, zone, captured);
}

void UndeadShrine::pay_clear_reward(Engine::Core::World& world,
                                    const UndeadRuntimeZone& zone,
                                    bool captured) const {
  if (zone.definition.clear_reward.empty()) {
    return;
  }

  int beneficiary = m_owners.get_local_player_id();
  if (captured) {
    if (int const captor = anchor_owner(world, zone.anchor_entity_id);
        captor != Engine::Core::k_owner_everyone) {
      beneficiary = captor;
    }
  }

  QStringList spoils;
  for (ResourceType const type : k_all_resource_types) {
    int const amount = zone.definition.clear_reward.get(type);
    if (amount <= 0) {
      continue;
    }
    grant_resource(beneficiary, zone.anchor_entity_id, type, amount);
    spoils.append(QStringLiteral("%1 %2").arg(amount).arg(
        QString::fromLatin1(resource_type_key(type))));
  }

  if (spoils.isEmpty()) {
    return;
  }

  Engine::Core::EventManager::instance().publish(
      Engine::Core::MissionAnnouncementEvent::for_owner(
          beneficiary,
          QCoreApplication::translate("UndeadAwakeningSystem",
                                      "The barrow gives up its hoard: %1.")
              .arg(spoils.join(QStringLiteral(", ")))));
}

void UndeadShrine::refresh_capture_lock(Engine::Core::World& world,
                                        const UndeadRuntimeZone& zone) const {
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

  const bool every_wave_put_down =
      zone.awakened &&
      zone.next_wave_index >= static_cast<int>(zone.definition.waves.size()) &&
      zone.active_spawn_ids.empty();
  capture->capture_blocked = !zone.garrison_broken && !every_wave_put_down;
}

} // namespace Game::Systems
