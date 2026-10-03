#include "rockfall_system.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../audio/cue_ids.h"
#include "core/component_combat.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "core/world_spatial_index.h"
#include "game/core/ownership_constants.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system/combat_hit_resolver.h"
#include "game/systems/combat_system/damage_application.h"
#include "game/systems/combat_system/damage_processor.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/owner_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems {

namespace {

using Index = Engine::Core::WorldSpatialIndex;

constexpr float k_gravity = 14.0F;

constexpr float k_rolling_share = 5.0F / 7.0F;
constexpr float k_rolling_resistance = 0.20F;
constexpr float k_restitution = 0.30F;
constexpr float k_landing_friction = 0.85F;
constexpr float k_rebound_speed = 1.8F;

constexpr float k_lift_off_clearance = 0.35F;
constexpr float k_max_speed = 24.0F;
constexpr float k_speed_kept_in_bends = 0.85F;

constexpr float k_max_launch_speed = 6.0F;
constexpr float k_settle_speed = 0.45F;
constexpr float k_min_roll_seconds = 1.0F;
constexpr float k_max_substep = 1.0F / 60.0F;
constexpr float k_push_speed = 2.5F;

constexpr float k_reference_speed = 9.0F;
constexpr float k_momentum_kept_per_strike = 0.82F;

constexpr float k_overhead_clearance = 1.6F;
constexpr float k_dust_lifetime = 1.3F;
constexpr float k_trail_dust_interval = 0.18F;
constexpr float k_trail_dust_speed = 3.5F;
constexpr float k_audible_landing_speed = 5.0F;
constexpr float k_dusty_landing_speed = 3.0F;
constexpr std::size_t k_max_dust = 128;
constexpr float k_settled_sink_seconds = 1.2F;

constexpr float k_min_ramp_rise = 1.5F;

constexpr float k_cache_setback = 1.2F;

constexpr float k_crest_fraction = 0.92F;
constexpr int k_crest_samples = 48;

constexpr float k_push_reach = 2.5F;

constexpr float k_approach_timeout = 6.0F;

constexpr float k_ramp_half_width = 4.5F;

constexpr float k_ai_release_reach = 0.7F;

constexpr float k_cache_merge_distance = 5.0F;

[[nodiscard]] auto hash01(std::uint64_t seed) -> float {
  std::uint64_t value = seed * 0x9e3779b97f4a7c15ULL;
  value ^= value >> 29;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 32;
  return static_cast<float>(value & 0xffffffULL) / 16777215.0F;
}

[[nodiscard]] auto flat(const QVector3D& v) -> QVector3D {
  return {v.x(), 0.0F, v.z()};
}

[[nodiscard]] auto
authored_to_world(float coord, int grid_size, float tile_size) -> float {
  float const safe_tile = std::max(tile_size, 1.0e-4F);
  return (coord - (static_cast<float>(grid_size) * 0.5F - 0.5F)) * safe_tile;
}

[[nodiscard]] auto trigger_name(Game::Map::RockfallTriggerMode mode) -> const char* {
  switch (mode) {
  case Game::Map::RockfallTriggerMode::Zone:
    return "zone";
  case Game::Map::RockfallTriggerMode::AiDefender:
    return "ai";
  case Game::Map::RockfallTriggerMode::Scripted:
    return "scripted";
  case Game::Map::RockfallTriggerMode::Claimable:
    return "claim";
  }
  return "zone";
}

[[nodiscard]] auto vector_json(const QVector3D& v) -> QJsonArray {
  return {v.x(), v.y(), v.z()};
}

[[nodiscard]] auto vector_from_json(const QJsonValue& value) -> QVector3D {
  auto const arr = value.toArray();
  if (arr.size() < 3) {
    return {};
  }
  return {float(arr.at(0).toDouble()),
          float(arr.at(1).toDouble()),
          float(arr.at(2).toDouble())};
}

void publish_cue_at(const char* cue_id, const QVector3D& where) {
  Engine::Core::AudioCueEvent cue(cue_id);
  cue.at(where.x(), where.y(), where.z());
  Engine::Core::EventManager::instance().publish(cue);
}

void announce_to(int owner_id, const QString& text, const char* cue_id) {
  if (Game::Core::is_neutral_owner(owner_id)) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::MissionAnnouncementEvent::for_owner(owner_id, text));
  if (cue_id != nullptr) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent::for_owner(owner_id, cue_id));
  }
}

[[nodiscard]] auto is_usable_troop(const Index::Entry& entry) -> bool {
  return entry.health > 0 && !entry.is(Index::k_building) &&
         !entry.is(Index::k_wildlife) && !entry.is(Index::k_pending_removal);
}

} // namespace

RockfallSystem::RockfallSystem(Services services)
    : m_services(services) {
}

RockfallSystem::~RockfallSystem() = default;

auto RockfallSystem::to_world(const Game::Map::MapDefinition& map_definition,
                              float x,
                              float z) const -> QVector3D {
  QVector3D result =
      map_definition.coordSystem == Game::Map::CoordSystem::World
          ? QVector3D(x, 0.0F, z)
          : QVector3D(authored_to_world(
                          x, map_definition.grid.width, map_definition.grid.tile_size),
                      0.0F,
                      authored_to_world(z,
                                        map_definition.grid.height,
                                        map_definition.grid.tile_size));
  result.setY(m_services.terrain.get_terrain_height(result.x(), result.z()));
  return result;
}

void RockfallSystem::add_trap(const Game::Map::RockfallTrap& definition,
                              const QVector3D& release_world,
                              const QVector3D& target_world,
                              bool hill_cache) {
  RuntimeTrap trap;
  trap.definition = definition;
  trap.release_world = release_world;
  trap.target_world = target_world;
  QVector3D const downhill = flat(target_world - release_world);
  trap.downhill = downhill.lengthSquared() > 1.0e-4F ? downhill.normalized()
                                                     : QVector3D(1.0F, 0.0F, 0.0F);
  trap.owner_id = definition.owner_id;
  trap.hill_cache = hill_cache;
  m_traps.push_back(std::move(trap));
}

void RockfallSystem::stage_hill_caches() {
  auto const* height_map = m_services.terrain.get_height_map();
  if (height_map == nullptr) {
    return;
  }
  auto const& terrain = m_services.terrain;
  int index = 0;
  for (auto const& line : height_map->hill_navigation().entrance_centerlines) {
    QVector3D top = line.end;
    QVector3D foot = line.start;
    top.setY(terrain.get_terrain_height(top.x(), top.z()));
    foot.setY(terrain.get_terrain_height(foot.x(), foot.z()));
    if (foot.y() > top.y()) {
      std::swap(top, foot);
    }
    if (top.y() - foot.y() < k_min_ramp_rise) {
      continue;
    }

    QVector3D const uphill = flat(top - foot).normalized();
    float const crest_height = foot.y() + (top.y() - foot.y()) * k_crest_fraction;
    QVector3D crest = top;
    for (int step = 1; step <= k_crest_samples; ++step) {
      QVector3D const probe =
          foot + (top - foot) * (static_cast<float>(step) / k_crest_samples);
      if (terrain.get_terrain_height(probe.x(), probe.z()) >= crest_height) {
        crest = probe;
        break;
      }
    }
    crest.setY(terrain.get_terrain_height(crest.x(), crest.z()));
    top = crest;
    QVector3D cache = top + uphill * k_cache_setback;
    cache.setY(terrain.get_terrain_height(cache.x(), cache.z()));
    bool const duplicate =
        std::any_of(m_traps.begin(), m_traps.end(), [&](const RuntimeTrap& other) {
          return other.hill_cache &&
                 flat(other.release_world - cache).length() < k_cache_merge_distance;
        });
    if (duplicate) {
      continue;
    }

    QVector3D const zone_centre = (top + foot) * 0.5F;
    float const ramp_length = flat(top - foot).length();
    Game::Map::RockfallTrap definition;
    definition.id = QStringLiteral("hill_cache_%1").arg(++index);
    definition.trigger = Game::Map::RockfallTriggerMode::Claimable;
    definition.owner_id = Game::Core::NEUTRAL_OWNER_ID;
    definition.release_x = cache.x();
    definition.release_z = cache.z();
    definition.target_x = zone_centre.x();
    definition.target_z = zone_centre.z();
    definition.zone_radius = std::max(4.0F, ramp_length * 0.5F + 1.5F);
    definition.boulder_count = 5;
    definition.boulder_radius = 0.6F;
    definition.release_spread = 1.6F;
    definition.release_interval = 0.22F;
    definition.damage = 60;
    definition.casualty_fraction = 0.5F;
    definition.ai_min_targets = 1;
    add_trap(definition, cache, foot, true);
  }
}

void RockfallSystem::configure(const Game::Map::MapDefinition& map_definition) {
  m_traps.clear();
  m_boulders.clear();
  m_dust.clear();
  m_total_strikes = 0;
  m_release_sequence = 0;
  float const tile = std::max(map_definition.grid.tile_size, 1.0e-4F);
  m_half_extent_x = static_cast<float>(map_definition.grid.width) * tile * 0.5F;
  m_half_extent_z = static_cast<float>(map_definition.grid.height) * tile * 0.5F;

  m_traps.reserve(map_definition.rockfall_traps.size());
  for (const auto& definition : map_definition.rockfall_traps) {
    add_trap(definition,
             to_world(map_definition, definition.release_x, definition.release_z),
             to_world(map_definition, definition.target_x, definition.target_z),
             false);
  }
  if (map_definition.hill_rockfall_caches) {
    stage_hill_caches();
  }
}

auto RockfallSystem::trap(std::size_t index) const -> TrapView {
  if (index >= m_traps.size()) {
    return {};
  }
  auto const& runtime = m_traps[index];
  return {.id = runtime.definition.id,
          .release_world = runtime.release_world,
          .target_world = runtime.target_world,
          .trigger = runtime.definition.trigger,
          .owner_id = runtime.owner_id,
          .times_fired = runtime.times_fired,
          .armed =
              runtime.armed && runtime.pending_releases == 0 && runtime.pusher == 0,
          .spent = runtime.spent,
          .pushing = runtime.pusher != 0,
          .hill_cache = runtime.hill_cache,
          .hostile_troops_in_zone = runtime.hostile_in_zone};
}

auto RockfallSystem::boulders() const -> std::vector<BoulderView> {
  std::vector<BoulderView> result;
  result.reserve(m_boulders.size());
  for (const auto& boulder : m_boulders) {
    result.push_back({.position = boulder.position,
                      .velocity = boulder.velocity,
                      .orientation = boulder.orientation,
                      .radius = boulder.radius,
                      .airborne = boulder.airborne,
                      .settled = boulder.settled,
                      .troops_struck = static_cast<int>(boulder.struck.size())});
  }
  return result;
}

auto RockfallSystem::trigger(const QString& trap_id) -> bool {
  for (auto& trap : m_traps) {
    if (trap.definition.id != trap_id) {
      continue;
    }
    if (!trap.armed || trap.pending_releases > 0 || trap.pusher != 0) {
      return false;
    }
    fire(trap);
    return true;
  }
  return false;
}

auto RockfallSystem::cache_in_reach(Engine::Core::World& world,
                                    Engine::Core::EntityID troop) const
    -> std::optional<std::size_t> {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(troop);
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(troop);
  if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
      !Game::Units::is_troop_spawn(unit->spawn_type)) {
    return std::nullopt;
  }
  QVector3D const at(transform->position.x, 0.0F, transform->position.z);
  std::optional<std::size_t> best;
  float best_distance = k_rockfall_use_radius;
  for (std::size_t index = 0; index < m_traps.size(); ++index) {
    auto const& trap = m_traps[index];
    if (!trap.armed || trap.spent || trap.pending_releases > 0 || trap.pusher != 0 ||
        trap.owner_id != unit->owner_id ||
        trap.definition.trigger == Game::Map::RockfallTriggerMode::Scripted) {
      continue;
    }
    float const distance = flat(trap.release_world - at).length();
    if (distance <= best_distance) {
      best_distance = distance;
      best = index;
    }
  }
  return best;
}

auto RockfallSystem::order_release(Engine::Core::World& world,
                                   Engine::Core::EntityID troop) -> bool {
  auto const index = cache_in_reach(world, troop);
  if (!index.has_value()) {
    return false;
  }
  begin_push(world, m_traps[*index], troop);
  return true;
}

auto RockfallSystem::push_spot(const RuntimeTrap& trap) const -> QVector3D {
  QVector3D spot =
      trap.release_world - trap.downhill * (trap.definition.boulder_radius * 2.6F);
  spot.setY(m_services.terrain.get_terrain_height(spot.x(), spot.z()));
  return spot;
}

void RockfallSystem::begin_push(Engine::Core::World& world,
                                RuntimeTrap& trap,
                                Engine::Core::EntityID troop) {
  trap.pusher = troop;
  trap.push_remaining = k_rockfall_push_seconds;
  auto const* transform = world.try_get<Engine::Core::TransformComponent>(troop);
  QVector3D const spot = push_spot(trap);
  bool const far_off =
      transform != nullptr &&
      flat(QVector3D(transform->position.x, 0.0F, transform->position.z) - spot)
              .length() > k_push_reach;
  if (far_off) {

    trap.approaching = true;
    trap.approach_remaining = k_approach_timeout;
    Game::Systems::CommandService::move_unit(world, troop, spot);
    return;
  }
  start_heaving(world, trap);
}

void RockfallSystem::start_heaving(Engine::Core::World& world, RuntimeTrap& trap) {
  trap.approaching = false;
  trap.approach_remaining = 0.0F;
  if (auto* movement = world.try_get<Engine::Core::MovementComponent>(trap.pusher)) {
    movement->stop();
  }
  if (world.try_get<Engine::Core::RockfallPushComponent>(trap.pusher) == nullptr) {
    world.emplace<Engine::Core::RockfallPushComponent>(trap.pusher);
  }
}

void RockfallSystem::advance_push(Engine::Core::World& world,
                                  RuntimeTrap& trap,
                                  float delta_time) {
  auto const pusher = trap.pusher;
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(pusher);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(pusher);
  if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
      unit->owner_id != trap.owner_id) {

    trap.pusher = 0;
    trap.push_remaining = 0.0F;
    trap.approaching = false;
    world.remove<Engine::Core::RockfallPushComponent>(pusher);
    return;
  }
  if (trap.approaching) {
    QVector3D const at(transform->position.x, 0.0F, transform->position.z);
    trap.approach_remaining -= delta_time;
    bool const arrived = flat(at - push_spot(trap)).length() <= k_push_reach;
    if (!arrived && trap.approach_remaining > 0.0F) {
      return;
    }
    if (!arrived && flat(at - trap.release_world).length() > k_rockfall_use_radius) {

      trap.pusher = 0;
      trap.approaching = false;
      return;
    }
    start_heaving(world, trap);
  }
  transform->desired_yaw = std::atan2(trap.downhill.x(), trap.downhill.z()) * 180.0F /
                           std::numbers::pi_v<float>;
  transform->has_desired_yaw = true;
  if (auto* movement = world.try_get<Engine::Core::MovementComponent>(pusher)) {
    movement->stop();
  }
  auto* push = world.try_get<Engine::Core::RockfallPushComponent>(pusher);
  if (push == nullptr) {
    push = world.emplace<Engine::Core::RockfallPushComponent>(pusher);
  }
  if (push != nullptr) {
    push->elapsed = k_rockfall_push_seconds - trap.push_remaining;
  }

  trap.push_remaining -= delta_time;
  if (trap.push_remaining <= 0.0F) {
    trap.pusher = 0;
    trap.push_remaining = 0.0F;
    world.remove<Engine::Core::RockfallPushComponent>(pusher);
    fire(trap);
  }
}

void RockfallSystem::fire(RuntimeTrap& trap) {
  trap.armed = false;
  ++trap.times_fired;
  trap.pending_releases = trap.definition.boulder_count;
  trap.released_in_volley = 0;
  trap.release_timer = 0.0F;
  trap.rearm_remaining = trap.definition.rearm_seconds;
  trap.ai_dwell = 0.0F;
  publish_cue_at(Game::Audio::Cue::k_hazard_rockfall, trap.release_world);
}

auto RockfallSystem::is_hostile(int trap_owner, int other_owner) const -> bool {
  if (other_owner == trap_owner) {
    return false;
  }
  if (Game::Core::is_neutral_owner(trap_owner)) {
    return true;
  }
  return m_services.owners.are_enemies(trap_owner, other_owner);
}

auto RockfallSystem::hostile_troops_in_zone(Engine::Core::World& world,
                                            const RuntimeTrap& trap,
                                            float reach) const -> int {
  int count = 0;
  float const zone_x =
      trap.hill_cache ? trap.definition.target_x : trap.target_world.x();
  float const zone_z =
      trap.hill_cache ? trap.definition.target_z : trap.target_world.z();

  QVector3D const crest = flat(trap.release_world);
  QVector3D const ramp = flat(trap.target_world) - crest;
  float const ramp_length_sq = std::max(ramp.lengthSquared(), 1.0e-4F);
  world.spatial_index().for_each_in_radius(
      zone_x, zone_z, trap.definition.zone_radius, [&](const Index::Entry& entry) {
        if (!is_usable_troop(entry) || !is_hostile(trap.owner_id, entry.owner_id)) {
          return;
        }
        if (trap.hill_cache) {
          QVector3D const at(entry.x, 0.0F, entry.z);
          float const along = QVector3D::dotProduct(at - crest, ramp) / ramp_length_sq;
          QVector3D const nearest = crest + ramp * std::clamp(along, 0.0F, 1.0F);
          if (along < 0.0F || along > reach ||
              (at - nearest).length() > k_ramp_half_width) {
            return;
          }
        }
        const auto* unit = world.try_get<Engine::Core::UnitComponent>(entry.id);
        if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
          return;
        }
        ++count;
      });
  return count;
}

auto RockfallSystem::nearest_own_troop(Engine::Core::World& world,
                                       const RuntimeTrap& trap) const
    -> Engine::Core::EntityID {
  Engine::Core::EntityID best = 0;
  float best_distance = std::numeric_limits<float>::max();
  world.spatial_index().for_each_in_radius(
      trap.release_world.x(),
      trap.release_world.z(),
      k_rockfall_use_radius,
      [&](const Index::Entry& entry) {
        if (!is_usable_troop(entry) || entry.owner_id != trap.owner_id) {
          return;
        }
        const auto* unit = world.try_get<Engine::Core::UnitComponent>(entry.id);
        if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
          return;
        }
        float const dx = entry.x - trap.release_world.x();
        float const dz = entry.z - trap.release_world.z();
        float const distance = dx * dx + dz * dz;
        if (distance < best_distance) {
          best_distance = distance;
          best = entry.id;
        }
      });
  return best;
}

void RockfallSystem::update_claim(Engine::Core::World& world, RuntimeTrap& trap) {
  if (trap.definition.trigger != Game::Map::RockfallTriggerMode::Claimable ||
      !trap.armed || trap.pusher != 0) {
    return;
  }
  bool owner_present = false;
  int challenger = Game::Core::NEUTRAL_OWNER_ID;
  float challenger_distance = std::numeric_limits<float>::max();
  world.spatial_index().for_each_in_radius(
      trap.release_world.x(),
      trap.release_world.z(),
      k_rockfall_claim_radius,
      [&](const Index::Entry& entry) {
        if (!is_usable_troop(entry) || entry.is(Index::k_undead) ||
            Game::Core::is_neutral_owner(entry.owner_id)) {
          return;
        }
        const auto* unit = world.try_get<Engine::Core::UnitComponent>(entry.id);
        if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
          return;
        }
        if (entry.owner_id == trap.owner_id) {
          owner_present = true;
          return;
        }
        float const dx = entry.x - trap.release_world.x();
        float const dz = entry.z - trap.release_world.z();
        float const distance = dx * dx + dz * dz;
        if (distance < challenger_distance) {
          challenger_distance = distance;
          challenger = entry.owner_id;
        }
      });
  if (owner_present || Game::Core::is_neutral_owner(challenger)) {
    return;
  }
  int const previous = trap.owner_id;
  trap.owner_id = challenger;
  trap.ai_dwell = 0.0F;
  trap.warned_owner = false;
  if (!m_services.owners.is_ai(challenger)) {
    announce_to(challenger,
                QCoreApplication::translate(
                    "RockfallSystem",
                    "Your men hold the stones above the slope. Select troops beside "
                    "them and order Roll Stones when the enemy climbs."),
                Game::Audio::Cue::k_alert_objective_complete);
  }
  if (!Game::Core::is_neutral_owner(previous) && !m_services.owners.is_ai(previous)) {
    announce_to(
        previous,
        QCoreApplication::translate("RockfallSystem",
                                    "The enemy has taken your stones on the heights."),
        nullptr);
  }
}

void RockfallSystem::warn_owner(RuntimeTrap& trap) {
  if (trap.warned_owner ||
      trap.definition.trigger != Game::Map::RockfallTriggerMode::Claimable) {
    return;
  }
  trap.warned_owner = true;
  if (m_services.owners.is_ai(trap.owner_id)) {
    return;
  }
  announce_to(
      trap.owner_id,
      QCoreApplication::translate("RockfallSystem",
                                  "Enemies are climbing below your stones. Roll them!"),
      nullptr);
}

void RockfallSystem::evaluate_auto_trigger(Engine::Core::World& world,
                                           RuntimeTrap& trap,
                                           float delta_time) {
  using Mode = Game::Map::RockfallTriggerMode;
  switch (trap.definition.trigger) {
  case Mode::Scripted:
    return;
  case Mode::Zone:
    if (trap.hostile_in_zone > 0) {
      fire(trap);
    }
    return;
  case Mode::AiDefender:
  case Mode::Claimable: {
    if (trap.hostile_in_zone == 0) {
      trap.ai_dwell = 0.0F;
      trap.warned_owner = false;
      return;
    }
    if (Game::Core::is_neutral_owner(trap.owner_id)) {
      return;
    }
    warn_owner(trap);
    if (!m_services.owners.is_ai(trap.owner_id)) {
      return;
    }

    int const in_reach = trap.hill_cache
                             ? hostile_troops_in_zone(world, trap, k_ai_release_reach)
                             : trap.hostile_in_zone;
    if (in_reach == 0) {
      return;
    }
    trap.ai_dwell += delta_time;
    if (in_reach < trap.definition.ai_min_targets &&
        trap.ai_dwell < k_rockfall_ai_patience_seconds) {
      return;
    }
    if (trap.definition.trigger == Mode::AiDefender) {
      fire(trap);
      return;
    }
    if (auto const troop = nearest_own_troop(world, trap); troop != 0) {
      begin_push(world, trap, troop);
    }
    return;
  }
  }
}

void RockfallSystem::release_boulder(RuntimeTrap& trap, int trap_index) {
  auto const& definition = trap.definition;
  std::uint64_t const seed =
      (static_cast<std::uint64_t>(trap_index + 1) << 32U) ^ ++m_release_sequence;

  QVector3D const aim = trap.downhill;
  QVector3D const lateral(-aim.z(), 0.0F, aim.x());

  int const count = std::max(1, definition.boulder_count);
  int const index = trap.released_in_volley++;
  float const fan =
      count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) - 0.5F
                : 0.0F;
  float const lateral_offset =
      definition.release_spread * fan + (hash01(seed ^ 0x51ULL) - 0.5F) * 0.6F;
  float const depth_offset = (hash01(seed ^ 0xa3ULL) - 0.5F) * 0.8F;

  Boulder boulder;
  boulder.trap_index = trap_index;
  boulder.seed = static_cast<std::uint32_t>(seed ^ (seed >> 32U));
  boulder.radius = definition.boulder_radius * (0.8F + 0.4F * hash01(seed ^ 0x7fULL));
  QVector3D position =
      trap.release_world + lateral * lateral_offset + aim * depth_offset;
  position.setY(m_services.terrain.get_terrain_height(position.x(), position.z()) +
                boulder.radius);
  boulder.position = position;
  boulder.velocity = aim * (k_push_speed + 1.5F * hash01(seed ^ 0x3cULL)) +
                     lateral * (hash01(seed ^ 0x99ULL) - 0.5F) * 0.6F;
  QVector3D const tumble_axis(hash01(seed ^ 0x11ULL) - 0.5F,
                              hash01(seed ^ 0x22ULL) - 0.5F,
                              hash01(seed ^ 0x33ULL) - 0.5F);
  boulder.orientation = QQuaternion::fromAxisAndAngle(
      tumble_axis.lengthSquared() > 1.0e-4F ? tumble_axis.normalized()
                                            : QVector3D(0.0F, 1.0F, 0.0F),
      360.0F * hash01(seed ^ 0x44ULL));
  add_dust(position - QVector3D(0.0F, boulder.radius * 0.6F, 0.0F),
           boulder.radius * 1.4F);
  m_boulders.push_back(std::move(boulder));
}

void RockfallSystem::step_boulder(Boulder& boulder, float dt) {
  if (boulder.settled) {
    return;
  }
  auto const& terrain = m_services.terrain;
  boulder.age += dt;
  float const radius = boulder.radius;
  QVector3D const up(0.0F, 1.0F, 0.0F);

  if (boulder.airborne) {
    boulder.velocity.setY(boulder.velocity.y() - k_gravity * dt);
    boulder.position += boulder.velocity * dt;
    float const ground =
        terrain.get_terrain_height(boulder.position.x(), boulder.position.z());
    if (boulder.position.y() - radius <= ground) {
      boulder.position.setY(ground + radius);
      QVector3D const normal =
          terrain.sample_ground_normal(boulder.position.x(), boulder.position.z());
      float const into_ground = QVector3D::dotProduct(boulder.velocity, normal);
      if (into_ground < 0.0F) {
        float const impact = -into_ground;
        QVector3D tangential = boulder.velocity - normal * into_ground;
        tangential *= k_landing_friction;
        boulder.velocity = tangential + normal * (impact * k_restitution);
        boulder.airborne = impact * k_restitution > k_rebound_speed;
        if (impact > k_dusty_landing_speed) {
          add_dust(boulder.position - QVector3D(0.0F, radius * 0.8F, 0.0F),
                   radius * std::clamp(impact / 7.0F, 0.8F, 2.2F));
        }
        if (impact > k_audible_landing_speed) {
          publish_cue_at(Game::Audio::Cue::k_combat_siege_impact, boulder.position);
        }
        if (!boulder.airborne) {
          boulder.velocity -= normal * QVector3D::dotProduct(boulder.velocity, normal);
        }
      } else {
        boulder.airborne = false;
      }
    }
  } else {
    QVector3D const normal =
        terrain.sample_ground_normal(boulder.position.x(), boulder.position.z());
    float const support = std::max(normal.y(), 0.0F);
    QVector3D const downhill = (normal * support - up) * (k_gravity * k_rolling_share);
    QVector3D velocity = boulder.velocity + downhill * dt;
    float const speed_before_turn = velocity.length();
    velocity -= normal * QVector3D::dotProduct(velocity, normal);

    if (float const turned = velocity.length(); turned > 1.0e-4F) {
      float const kept = std::max(turned, speed_before_turn * k_speed_kept_in_bends);
      velocity *= kept / turned;
    }
    float const speed = velocity.length();
    float const resistance = k_rolling_resistance * k_gravity * support * dt;
    velocity =
        speed > resistance ? velocity * ((speed - resistance) / speed) : QVector3D();
    boulder.velocity = velocity;

    QVector3D const predicted = boulder.position + velocity * dt;
    float const ground_next = terrain.get_terrain_height(predicted.x(), predicted.z());
    if (predicted.y() > ground_next + radius + k_lift_off_clearance) {
      boulder.airborne = true;
      boulder.position = predicted;
    } else {
      boulder.position = QVector3D(predicted.x(), ground_next + radius, predicted.z());
    }

    if (velocity.length() > k_trail_dust_speed) {
      boulder.dust_timer += dt;
      if (boulder.dust_timer >= k_trail_dust_interval) {
        boulder.dust_timer = 0.0F;
        add_dust(boulder.position - QVector3D(0.0F, radius * 0.85F, 0.0F),
                 radius * std::clamp(velocity.length() / 9.0F, 0.5F, 1.1F));
      }
    }
  }

  float const speed = boulder.velocity.length();
  if (speed > k_max_speed) {
    boulder.velocity *= k_max_speed / speed;
  }

  QVector3D const travel = flat(boulder.velocity);
  if (travel.lengthSquared() > 1.0e-6F) {
    QVector3D const axis = QVector3D::crossProduct(up, travel).normalized();
    float const roll_rate = boulder.airborne ? 0.5F : 1.0F;
    float const angle_deg = travel.length() / std::max(radius, 0.05F) * dt * roll_rate *
                            (180.0F / std::numbers::pi_v<float>);
    boulder.orientation =
        (QQuaternion::fromAxisAndAngle(axis, angle_deg) * boulder.orientation)
            .normalized();
  }

  if (!boulder.airborne && boulder.velocity.length() < k_settle_speed &&
      boulder.age > k_min_roll_seconds) {
    boulder.settled = true;
    boulder.velocity = QVector3D();
    add_dust(boulder.position - QVector3D(0.0F, radius * 0.7F, 0.0F), radius);
  }

  bool const outside = m_half_extent_x > 0.0F && m_half_extent_z > 0.0F &&
                       (std::abs(boulder.position.x()) > m_half_extent_x - 0.5F ||
                        std::abs(boulder.position.z()) > m_half_extent_z - 0.5F);
  if (outside || boulder.age > k_rockfall_max_boulder_age_seconds) {
    boulder.settled = true;
    boulder.velocity = QVector3D();
    boulder.settled_age = k_rockfall_settled_linger_seconds;
  }
}

void RockfallSystem::strike_troops(Engine::Core::World& world, Boulder& boulder) {
  if (boulder.settled) {
    return;
  }
  float const speed = boulder.velocity.length();
  if (speed < k_rockfall_lethal_speed) {
    return;
  }
  int const trap_owner =
      boulder.trap_index >= 0 && boulder.trap_index < static_cast<int>(m_traps.size())
          ? m_traps[static_cast<std::size_t>(boulder.trap_index)].owner_id
          : Game::Core::NEUTRAL_OWNER_ID;

  std::vector<Engine::Core::EntityID> victims;
  world.spatial_index().for_each_in_radius(
      boulder.position.x(),
      boulder.position.z(),
      boulder.radius + k_rockfall_troop_reach,
      [&](const Index::Entry& entry) {
        if (!is_usable_troop(entry) || !is_hostile(trap_owner, entry.owner_id) ||
            std::find(boulder.struck.begin(), boulder.struck.end(), entry.id) !=
                boulder.struck.end()) {
          return;
        }
        float const ground = m_services.terrain.get_terrain_height(entry.x, entry.z);
        if (boulder.position.y() - boulder.radius > ground + k_overhead_clearance) {
          return;
        }
        victims.push_back(entry.id);
      });

  for (auto const id : victims) {
    auto* target = world.get_entity(id);
    if (target == nullptr || world.has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }
    strike(world, boulder, *target, boulder.velocity.length());
  }
}

void RockfallSystem::strike(Engine::Core::World& world,
                            Boulder& boulder,
                            Engine::Core::Entity& target,
                            float speed) {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(target.get_id());
  auto const* transform =
      world.try_get<Engine::Core::TransformComponent>(target.get_id());
  if (unit == nullptr || transform == nullptr || unit->health <= 0) {
    return;
  }
  boulder.struck.push_back(target.get_id());
  ++m_total_strikes;

  Game::Map::RockfallTrap const fallback;
  auto const& definition =
      boulder.trap_index >= 0 && boulder.trap_index < static_cast<int>(m_traps.size())
          ? m_traps[static_cast<std::size_t>(boulder.trap_index)].definition
          : fallback;
  float const momentum = std::clamp(speed / k_reference_speed, 0.45F, 1.5F);
  int const proportional = static_cast<int>(
      std::ceil(static_cast<float>(unit->max_health) * definition.casualty_fraction));
  int const damage = std::max(
      1,
      static_cast<int>(std::round(
          static_cast<float>(std::max(definition.damage, proportional)) * momentum)));

  QVector3D const contact(transform->position.x,
                          boulder.position.y() - boulder.radius * 0.5F,
                          transform->position.z);
  QVector3D const travel = flat(boulder.velocity);

  if (Game::Systems::CombatRules::uses_rpg_combat_rules(&target)) {
    Game::Systems::Combat::deal_damage(&world, &target, damage);
  } else {
    auto const application =
        Game::Systems::Combat::apply_unit_damage(&world,
                                                 &target,
                                                 damage,
                                                 0,
                                                 contact,
                                                 std::nullopt,
                                                 speed,
                                                 Game::Units::SpawnType::Catapult);
    Game::Systems::Combat::launch_new_casualties_along(
        world,
        target.get_id(),
        travel.x(),
        travel.z(),
        application.queued_soldier_casualties,
        std::min(speed, k_max_launch_speed));
  }

  if (unit->health > 0) {
    Game::Systems::Combat::add_or_extend_stagger(
        &target,
        Engine::Core::HitFeedbackComponent::duration_for_tier(
            Engine::Core::StaggerTier::Knockdown),
        Engine::Core::StaggerTier::Knockdown);
  }

  boulder.velocity *= k_momentum_kept_per_strike;
  add_dust(contact, boulder.radius * 1.6F);
}

void RockfallSystem::add_dust(const QVector3D& position, float scale) {
  if (m_dust.size() >= k_max_dust) {
    m_dust.erase(m_dust.begin());
  }
  m_dust.push_back(
      {.position = position, .scale = scale, .age = 0.0F, .lifetime = k_dust_lifetime});
}

void RockfallSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  float const dt = std::max(0.0F, delta_time);
  if (dt > 0.0F && (!m_traps.empty() || !m_boulders.empty())) {
    world->spatial_index().refresh(*world);

    for (std::size_t index = 0; index < m_traps.size(); ++index) {
      auto& trap = m_traps[index];
      if (!trap.armed && !trap.spent && trap.pending_releases == 0 &&
          trap.pusher == 0) {
        if (trap.definition.rearm_seconds > 0.0F) {
          trap.rearm_remaining -= dt;
          if (trap.rearm_remaining <= 0.0F) {
            trap.rearm_remaining = 0.0F;
            trap.armed = true;
          }
        } else {
          trap.spent = true;
        }
      }
      if (trap.armed && !trap.spent) {
        update_claim(*world, trap);
        trap.hostile_in_zone = hostile_troops_in_zone(*world, trap, 1.0F);
        if (trap.pusher != 0) {
          advance_push(*world, trap, dt);
        } else {
          evaluate_auto_trigger(*world, trap, dt);
        }
      } else {
        trap.hostile_in_zone = 0;
      }
      if (trap.pending_releases > 0) {
        trap.release_timer -= dt;
        while (trap.pending_releases > 0 && trap.release_timer <= 0.0F) {
          release_boulder(trap, static_cast<int>(index));
          --trap.pending_releases;
          trap.release_timer += std::max(trap.definition.release_interval, 0.0F);
          if (trap.definition.release_interval <= 0.0F) {
            trap.release_timer = 0.0F;
          }
        }
      }
    }

    int const substeps =
        std::max(1, static_cast<int>(std::ceil(dt / k_max_substep - 1.0e-4F)));
    float const h = dt / static_cast<float>(substeps);
    for (auto& boulder : m_boulders) {
      for (int step = 0; step < substeps && !boulder.settled; ++step) {
        step_boulder(boulder, h);
        strike_troops(*world, boulder);
      }
      if (boulder.settled) {
        boulder.settled_age += dt;
      }
    }
    std::erase_if(m_boulders, [](const Boulder& boulder) {
      return boulder.settled &&
             boulder.settled_age >= k_rockfall_settled_linger_seconds;
    });

    for (auto& dust : m_dust) {
      dust.age += dt;
    }
    std::erase_if(m_dust, [](const Dust& dust) { return dust.age >= dust.lifetime; });
  }

  publish_render_views(*world);
}

void RockfallSystem::publish_render_views(Engine::Core::World& world) const {
  auto& frame = world.render_effects_frame();
  frame.rockfall_boulders.clear();
  frame.rockfall_boulders.reserve(m_boulders.size());
  float const sink_start = k_rockfall_settled_linger_seconds - k_settled_sink_seconds;
  for (const auto& boulder : m_boulders) {
    float const sink =
        boulder.settled && boulder.settled_age > sink_start
            ? std::clamp((boulder.settled_age - sink_start) / k_settled_sink_seconds,
                         0.0F,
                         1.0F) *
                  boulder.radius * 2.0F
            : 0.0F;
    frame.rockfall_boulders.push_back({.position = boulder.position,
                                       .orientation = boulder.orientation,
                                       .radius = boulder.radius,
                                       .sink = sink,
                                       .seed = boulder.seed});
  }

  frame.rockfall_caches.clear();
  for (std::size_t index = 0; index < m_traps.size(); ++index) {
    auto const& trap = m_traps[index];
    int const left = trap.armed ? trap.definition.boulder_count : trap.pending_releases;
    if (trap.spent || left <= 0) {
      continue;
    }
    float const zone_x =
        trap.hill_cache ? trap.definition.target_x : trap.target_world.x();
    float const zone_z =
        trap.hill_cache ? trap.definition.target_z : trap.target_world.z();
    frame.rockfall_caches.push_back(
        {.position = trap.release_world,
         .downhill = trap.downhill,
         .zone_target = QVector3D(
             zone_x, m_services.terrain.get_terrain_height(zone_x, zone_z), zone_z),
         .zone_radius = trap.definition.zone_radius,
         .boulder_radius = trap.definition.boulder_radius,
         .boulders_left = left,
         .owner_id = trap.owner_id,
         .push_progress =
             trap.pusher != 0 && !trap.approaching
                 ? std::clamp(
                       1.0F - trap.push_remaining / k_rockfall_push_seconds, 0.0F, 1.0F)
                 : -1.0F,
         .threatened = trap.hostile_in_zone > 0,
         .armed = trap.armed,
         .seed = static_cast<std::uint32_t>((index + 1U) * 2654435761U)});
  }

  frame.rockfall_dust.clear();
  frame.rockfall_dust.reserve(m_dust.size());
  for (const auto& dust : m_dust) {
    frame.rockfall_dust.push_back(
        {.position = dust.position, .scale = dust.scale, .age = dust.age});
  }
}

auto RockfallSystem::serialize_state() const -> QJsonObject {
  QJsonArray traps;
  for (const auto& trap : m_traps) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), trap.definition.id);
    obj.insert(QStringLiteral("trigger"), trigger_name(trap.definition.trigger));
    obj.insert(QStringLiteral("owner"), trap.owner_id);
    obj.insert(QStringLiteral("armed"), trap.armed);
    obj.insert(QStringLiteral("spent"), trap.spent);
    obj.insert(QStringLiteral("times_fired"), trap.times_fired);
    obj.insert(QStringLiteral("rearm_remaining"), trap.rearm_remaining);
    obj.insert(QStringLiteral("pending_releases"), trap.pending_releases);
    obj.insert(QStringLiteral("released_in_volley"), trap.released_in_volley);
    obj.insert(QStringLiteral("release_timer"), trap.release_timer);
    obj.insert(QStringLiteral("ai_dwell"), trap.ai_dwell);
    obj.insert(QStringLiteral("pusher"), static_cast<qint64>(trap.pusher));
    obj.insert(QStringLiteral("push_remaining"), trap.push_remaining);
    obj.insert(QStringLiteral("approaching"), trap.approaching);
    obj.insert(QStringLiteral("approach_remaining"), trap.approach_remaining);
    obj.insert(QStringLiteral("warned_owner"), trap.warned_owner);
    traps.append(obj);
  }

  QJsonArray boulders;
  for (const auto& boulder : m_boulders) {
    QJsonObject obj;
    obj.insert(QStringLiteral("position"), vector_json(boulder.position));
    obj.insert(QStringLiteral("velocity"), vector_json(boulder.velocity));
    obj.insert(QStringLiteral("orientation"),
               QJsonArray{boulder.orientation.scalar(),
                          boulder.orientation.x(),
                          boulder.orientation.y(),
                          boulder.orientation.z()});
    obj.insert(QStringLiteral("radius"), boulder.radius);
    obj.insert(QStringLiteral("age"), boulder.age);
    obj.insert(QStringLiteral("settled_age"), boulder.settled_age);
    obj.insert(QStringLiteral("airborne"), boulder.airborne);
    obj.insert(QStringLiteral("settled"), boulder.settled);
    obj.insert(QStringLiteral("trap"), boulder.trap_index);
    obj.insert(QStringLiteral("seed"), static_cast<qint64>(boulder.seed));
    QJsonArray struck;
    for (auto const id : boulder.struck) {
      struck.append(static_cast<qint64>(id));
    }
    obj.insert(QStringLiteral("struck"), struck);
    boulders.append(obj);
  }

  QJsonObject state;
  state.insert(QStringLiteral("traps"), traps);
  state.insert(QStringLiteral("boulders"), boulders);
  state.insert(QStringLiteral("release_sequence"),
               static_cast<qint64>(m_release_sequence));
  state.insert(QStringLiteral("strikes"), m_total_strikes);
  return state;
}

void RockfallSystem::restore_state(const QJsonObject& state) {
  for (const auto value : state.value(QStringLiteral("traps")).toArray()) {
    auto const obj = value.toObject();
    QString const id = obj.value(QStringLiteral("id")).toString();
    auto it =
        std::find_if(m_traps.begin(), m_traps.end(), [&](const RuntimeTrap& trap) {
          return trap.definition.id == id;
        });
    if (it == m_traps.end()) {
      continue;
    }
    it->owner_id = obj.value(QStringLiteral("owner")).toInt(it->owner_id);
    it->armed = obj.value(QStringLiteral("armed")).toBool(true);
    it->spent = obj.value(QStringLiteral("spent")).toBool(false);
    it->times_fired = obj.value(QStringLiteral("times_fired")).toInt();
    it->rearm_remaining =
        float(obj.value(QStringLiteral("rearm_remaining")).toDouble());
    it->pending_releases = obj.value(QStringLiteral("pending_releases")).toInt();
    it->released_in_volley = obj.value(QStringLiteral("released_in_volley")).toInt();
    it->release_timer = float(obj.value(QStringLiteral("release_timer")).toDouble());
    it->ai_dwell = float(obj.value(QStringLiteral("ai_dwell")).toDouble());
    it->pusher = static_cast<Engine::Core::EntityID>(
        obj.value(QStringLiteral("pusher")).toInteger());
    it->push_remaining = float(obj.value(QStringLiteral("push_remaining")).toDouble());
    it->approaching = obj.value(QStringLiteral("approaching")).toBool();
    it->approach_remaining =
        float(obj.value(QStringLiteral("approach_remaining")).toDouble());
    it->warned_owner = obj.value(QStringLiteral("warned_owner")).toBool();
  }

  m_boulders.clear();
  m_dust.clear();
  for (const auto value : state.value(QStringLiteral("boulders")).toArray()) {
    auto const obj = value.toObject();
    Boulder boulder;
    boulder.position = vector_from_json(obj.value(QStringLiteral("position")));
    boulder.velocity = vector_from_json(obj.value(QStringLiteral("velocity")));
    auto const q = obj.value(QStringLiteral("orientation")).toArray();
    if (q.size() == 4) {
      boulder.orientation = QQuaternion(float(q.at(0).toDouble(1.0)),
                                        float(q.at(1).toDouble()),
                                        float(q.at(2).toDouble()),
                                        float(q.at(3).toDouble()));
    }
    boulder.radius = float(obj.value(QStringLiteral("radius")).toDouble(0.55));
    boulder.age = float(obj.value(QStringLiteral("age")).toDouble());
    boulder.settled_age = float(obj.value(QStringLiteral("settled_age")).toDouble());
    boulder.airborne = obj.value(QStringLiteral("airborne")).toBool();
    boulder.settled = obj.value(QStringLiteral("settled")).toBool();
    boulder.trap_index = obj.value(QStringLiteral("trap")).toInt(-1);
    boulder.seed =
        static_cast<std::uint32_t>(obj.value(QStringLiteral("seed")).toInteger());
    for (const auto id : obj.value(QStringLiteral("struck")).toArray()) {
      boulder.struck.push_back(static_cast<Engine::Core::EntityID>(id.toInteger()));
    }
    m_boulders.push_back(std::move(boulder));
  }
  m_release_sequence = static_cast<std::uint32_t>(
      state.value(QStringLiteral("release_sequence")).toInteger());
  m_total_strikes = state.value(QStringLiteral("strikes")).toInt();
}

} // namespace Game::Systems
