#include "undead_awakening_system.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QStringList>
#include <QVector3D>
#include <qjsonarray.h>
#include <qjsonobject.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/component_combat.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/death_sequence.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/local_audience.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "core/world_spatial_index.h"
#include "game/map/terrain_service.h"
#include "game/map/undead_shrine_placement.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system/combat_utils.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/order_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_feedback.h"
#include "game/util/planar_math.h"
#include "units/factory.h"
#include "units/unit.h"

namespace Game::Systems {

namespace {

constexpr float k_between_wave_delay_seconds = 1.5F;
constexpr float k_spawn_y_offset = 0.05F;
constexpr float k_anchor_match_distance = 3.5F;

constexpr const char* k_awakening_music = "music.event.skeletons_awaken";
constexpr const char* k_awakening_cue = "alert.undead_awakening";
constexpr float k_zone_music_poll_seconds = 0.25F;

constexpr float k_golden_angle_radians = 2.3999632F;
constexpr float k_min_spawn_ring_radius = 2.0F;
constexpr float k_spawn_ring_fraction = 0.8F;
constexpr int k_spawn_placement_attempts = 12;

constexpr float k_leash_poll_seconds = 0.5F;
constexpr float k_leash_slack = 1.5F;
constexpr float k_min_guard_radius = 2.0F;
constexpr float k_post_ring_fraction = 0.5F;
constexpr float k_min_post_ring_radius = 1.5F;
constexpr float k_post_ring_margin = 2.0F;
constexpr float k_post_ring_drift_degrees_per_second = 4.0F;
constexpr float k_post_arrival_distance = 1.25F;
constexpr int k_post_placement_attempts = 6;

auto post_ring_radius(const Game::Map::UndeadZone& definition) -> float {
  float const inside_leash =
      std::max(k_min_post_ring_radius, definition.leash_radius - k_post_ring_margin);
  return std::clamp(
      definition.radius * k_post_ring_fraction, k_min_post_ring_radius, inside_leash);
}

auto yaw_degrees_toward(float dx, float dz) -> float {
  return std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
}

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
    : m_services(services) {
}

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

  auto const& terrain_service = m_services.terrain;
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
  for (const auto value : state) {
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
        obj.value(QStringLiteral("anchor_entity_id")).toVariant().toULongLong());

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
    for (const auto id_value : ids) {
      zone->active_spawn_ids.push_back(
          static_cast<Engine::Core::EntityID>(id_value.toVariant().toULongLong()));
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
    obj[QStringLiteral("anchor_entity_id")] =
        static_cast<qint64>(zone.anchor_entity_id);
    obj[QStringLiteral("next_wave_index")] = zone.next_wave_index;
    obj[QStringLiteral("completed_waves")] = zone.completed_waves;
    obj[QStringLiteral("respawn_delay_remaining")] = zone.respawn_delay_remaining;
    obj[QStringLiteral("current_wave_elapsed")] = zone.current_wave_elapsed;
    QJsonArray active_ids;
    for (Engine::Core::EntityID const id : zone.active_spawn_ids) {
      active_ids.append(static_cast<qint64>(id));
    }
    obj[QStringLiteral("active_spawn_ids")] = active_ids;
    array.append(obj);
  }
  return array;
}

void UndeadAwakeningSystem::ensure_zone_owner_registered(
    const RuntimeZone& zone) const {
  auto& owners = m_services.owners;
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

  auto& nations = m_services.nations;
  nations.set_player_nation(zone.definition.owner_id,
                            Game::Systems::NationID::IronSepulcher);
  m_services.stats.mark_game_start(zone.definition.owner_id);
}

void UndeadAwakeningSystem::place_zone_shrine(
    const Game::Map::MapDefinition& map_definition,
    RuntimeZone& zone,
    Game::Map::UndeadShrineExclusions& exclusions) const {
  auto& terrain_service = m_services.terrain;

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
    zone.respawn_delay_remaining = k_between_wave_delay_seconds;
  }
}

auto UndeadAwakeningSystem::should_awaken_zone(
    Engine::Core::World& world, const RuntimeZone& zone) const -> std::optional<int> {
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

auto UndeadAwakeningSystem::can_spawn_wave(const RuntimeZone& zone) const -> bool {
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

auto UndeadAwakeningSystem::zone_origin(const RuntimeZone& zone) const -> QVector3D {
  return (zone.anchor_world_prop_id != 0) ? zone.anchor_world : zone.center_world;
}

auto UndeadAwakeningSystem::guard_post_for_index(const RuntimeZone& zone,
                                                 int post_index,
                                                 int post_count) const -> QVector3D {
  auto const& terrain_service = m_services.terrain;
  QVector3D const origin = zone_origin(zone);
  int const total = std::max(1, post_count);
  float const ring = post_ring_radius(zone.definition);
  float const base_angle =
      zone.post_ring_phase_degrees * std::numbers::pi_v<float> / 180.0F +
      2.0F * std::numbers::pi_v<float> * static_cast<float>(post_index) /
          static_cast<float>(total);

  for (int attempt = 0; attempt < k_post_placement_attempts; ++attempt) {
    float const angle =
        base_angle + static_cast<float>(attempt) * (k_golden_angle_radians * 0.5F);
    float const sample_radius =
        std::max(k_min_post_ring_radius, ring - static_cast<float>(attempt) * 0.5F);
    float const world_x = origin.x() + std::cos(angle) * sample_radius;
    float const world_z = origin.z() + std::sin(angle) * sample_radius;
    if (terrain_service.is_initialized() &&
        terrain_service.is_forbidden_world(world_x, world_z)) {
      continue;
    }
    return terrain_service.resolve_surface_world_position(
        world_x, world_z, k_spawn_y_offset, origin.y());
  }

  return origin;
}

void UndeadAwakeningSystem::station_guardian(Engine::Core::World& world,
                                             const RuntimeZone& zone,
                                             Engine::Core::EntityID guardian_id,
                                             int post_index,
                                             int post_count,
                                             bool recall) const {
  auto* guardian = world.get_entity(guardian_id);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(guardian_id);
  if (guardian == nullptr || transform == nullptr) {
    return;
  }

  QVector3D const post = guard_post_for_index(zone, post_index, post_count);
  QVector3D const origin = zone_origin(zone);

  auto* guard =
      Engine::Core::get_or_add_component<Engine::Core::GuardModeComponent>(*guardian);
  if (guard == nullptr) {
    return;
  }
  guard->active = true;
  guard->has_guard_target = true;
  guard->guarded_entity_id = 0;
  guard->guard_position_x = post.x();
  guard->guard_position_z = post.z();
  guard->has_reach_center = true;
  guard->reach_center_x = origin.x();
  guard->reach_center_z = origin.z();
  guard->guard_radius = std::max(k_min_guard_radius, zone.definition.leash_radius);

  auto const* movement = world.try_get<Engine::Core::MovementComponent>(guardian_id);
  if (recall) {
    OrderService::clear_attack_target(guardian);
    CombatRules::clear_rts_melee_lock(guardian);
    float const homeward_reach = post_ring_radius(zone.definition) + k_post_ring_margin;
    bool const already_homeward =
        movement != nullptr && movement->get_has_target() &&
        std::hypot(movement->get_goal_x() - origin.x(),
                   movement->get_goal_y() - origin.z()) <= homeward_reach;
    if (!already_homeward) {
      OrderService::reset_movement(guardian);
      guard->returning_to_guard_position = false;
    }
    return;
  }

  auto const* attack_target =
      world.try_get<Engine::Core::AttackTargetComponent>(guardian_id);
  bool const busy = (attack_target != nullptr && attack_target->target_id != 0) ||
                    (movement != nullptr && movement->get_has_target());
  if (busy || transform->has_desired_yaw) {
    return;
  }

  float const dx = transform->position.x - post.x();
  float const dz = transform->position.z - post.z();
  if (dx * dx + dz * dz > k_post_arrival_distance * k_post_arrival_distance) {
    return;
  }
  float const out_x = post.x() - origin.x();
  float const out_z = post.z() - origin.z();
  if (out_x * out_x + out_z * out_z < 0.0001F) {
    return;
  }
  float const outward = yaw_degrees_toward(out_x, out_z);
  if (std::fabs(Game::Systems::signed_yaw_delta(transform->rotation.y, outward)) <
      1.0F) {
    return;
  }
  transform->desired_yaw = outward;
  transform->has_desired_yaw = true;
}

void UndeadAwakeningSystem::enforce_leash(Engine::Core::World& world,
                                          RuntimeZone& zone) const {
  if (zone.active_spawn_ids.empty()) {
    return;
  }

  QVector3D const origin = zone_origin(zone);
  float const leash = zone.definition.leash_radius + k_leash_slack;
  int const post_count = static_cast<int>(zone.active_spawn_ids.size());

  for (int index = 0; index < post_count; ++index) {
    Engine::Core::EntityID const guardian_id = zone.active_spawn_ids[index];
    auto const* transform =
        world.try_get<Engine::Core::TransformComponent>(guardian_id);
    if (transform == nullptr) {
      continue;
    }
    auto const* attack_target =
        world.try_get<Engine::Core::AttackTargetComponent>(guardian_id);
    auto* prey = attack_target != nullptr && attack_target->target_id != 0
                     ? world.get_entity(attack_target->target_id)
                     : nullptr;
    float allowed = leash;
    bool strayed = false;
    if (prey != nullptr) {
      auto* guardian = world.get_entity(guardian_id);
      strayed = !Combat::within_guard_reach(
          guardian, prey, Combat::GuardReachRule::AnswersFire);
      allowed += Combat::guard_answer_fire_margin(prey);
    }
    float const dx = transform->position.x - origin.x();
    float const dz = transform->position.z - origin.z();
    strayed = strayed || dx * dx + dz * dz > allowed * allowed;
    station_guardian(world, zone, guardian_id, index, post_count, strayed);
  }
}

auto UndeadAwakeningSystem::spawn_position_for_index(
    const RuntimeZone& zone, int spawn_index, int spawn_count) const -> QVector3D {
  auto const& terrain_service = m_services.terrain;
  QVector3D const origin = zone_origin(zone);

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

void UndeadAwakeningSystem::pay_clear_reward(Engine::Core::World& world,
                                             const RuntimeZone& zone,
                                             bool captured) const {
  if (zone.definition.clear_reward.empty()) {
    return;
  }

  int beneficiary = m_services.owners.get_local_player_id();
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

  const bool every_wave_put_down =
      zone.awakened &&
      zone.next_wave_index >= static_cast<int>(zone.definition.waves.size()) &&
      zone.active_spawn_ids.empty();
  capture->capture_blocked = !zone.garrison_broken && !every_wave_put_down;
}

void UndeadAwakeningSystem::awaken_zone(Engine::Core::World& world,
                                        RuntimeZone& zone,
                                        int woken_by) {
  zone.awakened = true;
  zone.awakened_by_owner_id = woken_by;
  zone.respawn_delay_remaining = 0.0F;
  ensure_zone_owner_registered(zone);
  zone.announced_awakening = true;
  zone.current_wave_elapsed = 0.0F;

  try_spawn_next_wave(world, zone);

  QVector3D const origin = zone_origin(zone);
  Engine::Core::EventManager::instance().publish(Engine::Core::UndeadZoneAwakenedEvent(
      zone.definition.id, origin.x(), origin.z(), zone.definition.owner_id, woken_by));
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

  int const post_count = static_cast<int>(zone.active_spawn_ids.size());
  for (int index = 0; index < post_count; ++index) {
    station_guardian(
        world, zone, zone.active_spawn_ids[index], index, post_count, true);
  }

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

  if (!m_zones.empty()) {
    world->spatial_index().refresh(*world);
  }

  for (auto& zone : m_zones) {
    ensure_anchor_structure(*world, zone);
    refresh_anchor_structure(*world, zone);
    refresh_active_spawns(*world, zone);

    if (!zone.garrison_broken && !zone.awakened) {
      if (auto const woken_by = should_awaken_zone(*world, zone)) {
        awaken_zone(*world, zone, *woken_by);
      }
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
              "The risen guardians are put down. Hold the shrine to purify it.")));
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AudioCueEvent("alert.objective_complete"));
    }
  }

  m_leash_poll += delta_time;
  if (m_leash_poll >= k_leash_poll_seconds) {
    for (auto& zone : m_zones) {
      zone.post_ring_phase_degrees =
          std::fmod(zone.post_ring_phase_degrees +
                        k_post_ring_drift_degrees_per_second * m_leash_poll,
                    360.0F);
      enforce_leash(*world, zone);
    }
    m_leash_poll = 0.0F;
  }

  update_zone_music(*world, delta_time);
  m_allow_mission_start_trigger = false;
}

auto UndeadAwakeningSystem::local_player_inside(Engine::Core::World& world,
                                                const RuntimeZone& zone) const -> bool {
  const int local_owner = m_services.owners.get_local_player_id();
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

void UndeadAwakeningSystem::update_zone_music(Engine::Core::World& world,
                                              float delta_time) {

  m_zone_music_poll += delta_time;
  if (m_zone_music_poll < k_zone_music_poll_seconds) {
    return;
  }
  m_zone_music_poll = 0.0F;

  bool inside_a_woken_zone = false;
  for (const auto& zone : m_zones) {
    const bool cleared =
        zone.active_spawn_ids.empty() &&
        zone.next_wave_index >= static_cast<int>(zone.definition.waves.size());
    if (!zone.awakened || zone.garrison_broken || cleared) {
      continue;
    }
    const int local_owner = m_services.owners.get_local_player_id();
    if (zone.awakened_by_owner_id != 0 && zone.awakened_by_owner_id != local_owner) {
      continue;
    }
    if (local_player_inside(world, zone)) {
      inside_a_woken_zone = true;
      break;
    }
  }

  if (inside_a_woken_zone == m_zone_music_playing) {
    return;
  }
  m_zone_music_playing = inside_a_woken_zone;
  if (inside_a_woken_zone) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::MusicTriggerEvent(k_awakening_music));
  } else {

    Engine::Core::EventManager::instance().publish(Engine::Core::MusicStopEvent());
  }
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
