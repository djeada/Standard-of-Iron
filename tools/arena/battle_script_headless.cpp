#include "battle_script_headless.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>
#include <memory>
#include <unordered_map>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/world_digest.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/unit.h"
#include "utils/resource_utils.h"

namespace Arena::Headless {

namespace {

constexpr int k_local_owner = 1;

auto load_terrain(const ArenaScenarioDefinition& definition,
                  Game::Session::SessionContext& session,
                  QString* error) -> bool {
  Game::Map::MapDefinition map;
  if (!definition.campaign_map_path.isEmpty()) {
    if (!Game::Map::MapLoader::load_from_json_file(
            Utils::Resources::resolve_resource_path(definition.campaign_map_path),
            map,
            error)) {
      return false;
    }
  } else {
    const int extent =
        definition.terrain_grid_extent > 0 ? definition.terrain_grid_extent : 128;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.grid.width = extent;
    map.grid.height = extent;
    map.grid.tile_size = 1.0F;
    map.rivers = definition.rivers;
    map.lakes = definition.lakes;
    map.bridges = definition.bridges;
  }
  Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);
  session.terrain().initialize(map);
  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    pathfinder->update_navigation_grid();
  }
  return true;
}

} // namespace

auto run(const ArenaScenarioDefinition& definition, const Options& options) -> Result {
  Result result;
  result.scenario_id = definition.id;
  QElapsedTimer wall;
  wall.start();

  auto session = std::make_unique<Game::Session::SessionContext>();
  const Game::Session::ScopedSession scope(*session);

  auto& owners = session->owners();
  owners.clear();
  owners.register_owner_with_id(
      k_local_owner, Game::Systems::OwnerType::Player, "Arena Player");
  owners.set_owner_team(k_local_owner, 1);
  owners.set_local_player_id(k_local_owner);
  for (const auto& owner_team : definition.owner_teams) {
    if (owners.get_owner_type(owner_team.owner_id) ==
        Game::Systems::OwnerType::Neutral) {
      owners.register_owner_with_id(
          owner_team.owner_id,
          Game::Systems::OwnerType::Player,
          QStringLiteral("Scripted Army %1").arg(owner_team.owner_id).toStdString());
    }
    owners.set_owner_team(owner_team.owner_id, owner_team.team_id);
  }
  Game::Systems::initialize_default_content(session->nations());
  for (const auto& group : definition.groups) {
    session->nations().set_player_nation(group.owner_id, group.nation_id);
  }

  QString terrain_error;
  if (!load_terrain(definition, *session, &terrain_error)) {
    result.error = QStringLiteral("cannot load terrain: %1").arg(terrain_error);
    return result;
  }
  auto& world = session->world();
  Game::Systems::register_runtime_systems(world);

  Game::Units::UnitFactoryRegistry factories;
  Game::Units::register_built_in_units(factories);
  std::unordered_map<Engine::Core::EntityID, std::unique_ptr<Game::Units::Unit>>
      handles;

  ArenaScenarioHost host;
  host.spawn_unit = [&](const ArenaScenarioGroup& group,
                        const QVector3D& requested) -> Engine::Core::EntityID {
    Game::Units::SpawnParams params;
    params.position = Game::Systems::NavGrid::snap_to_walkable_ground(requested);
    params.rotation_y = group.facing_degrees;
    params.player_id = group.owner_id;
    params.spawn_type = Game::Units::spawn_typeFromTroopType(group.troop_type);
    params.ai_controlled = false;
    params.nation_id = group.nation_id;
    auto unit = factories.create(group.troop_type, world, params);
    if (unit == nullptr) {
      return 0U;
    }
    const auto id = unit->id();
    if (auto* transform = world.try_get<Engine::Core::TransformComponent>(id)) {
      transform->rotation.y = group.facing_degrees;
      transform->desired_yaw = group.facing_degrees;
      transform->has_desired_yaw = true;
    }
    if (auto* component = world.try_get<Engine::Core::UnitComponent>(id);
        component != nullptr && !group.keep_troop_speed) {
      component->speed = 2.2F;
    }
    handles.emplace(id, std::move(unit));
    ++result.spawned_units;
    return id;
  };
  host.find_unit = [&](Engine::Core::EntityID id) -> Game::Units::Unit* {
    const auto found = handles.find(id);
    return found != handles.end() ? found->second.get() : nullptr;
  };
  host.set_camera = [](const auto&, const auto&) {
  };
  host.set_force_full_creature_lod = [](bool) {
  };
  host.terrain = &session->terrain();
  host.building_collision = &session->building_collision();

  ArenaScenarioRunner runner(world, std::move(host), definition);
  if (options.duration_override > 0.0F) {
    runner.set_duration_limit(options.duration_override);
  }
  if (!runner.start()) {
    result.error = QStringLiteral("scenario failed validation or startup");
    for (const auto& issue : runner.report().issues) {
      result.error += QStringLiteral("; %1: %2").arg(issue.code, issue.message);
    }
    return result;
  }
  result.started = true;
  const float step = std::max(options.fixed_step_seconds, 1.0e-3F);
  float next_digest = 0.0F;
  float next_trace = 0.0F;
  const auto trace = [&]() {
    QStringList parts;
    for (const auto& group : definition.groups) {
      int living = 0;
      int health = 0;
      QVector3D sum;
      for (const auto id : runner.group_entities(group.name)) {
        const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
        const auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
        if (unit != nullptr && unit->health > 0) {
          ++living;
          health += unit->health;
          if (transform != nullptr) {
            sum += QVector3D(transform->position.x, 0.0F, transform->position.z);
          }
        }
      }
      if (runner.group_entities(group.name).empty()) {
        continue;
      }
      const QVector3D centre =
          living > 0 ? sum / static_cast<float>(living) : QVector3D();
      parts.push_back(QStringLiteral("%1 %2/%3 hp%4 @(%5,%6)")
                          .arg(group.name)
                          .arg(living)
                          .arg(runner.group_entities(group.name).size())
                          .arg(health)
                          .arg(static_cast<int>(centre.x()))
                          .arg(static_cast<int>(centre.z())));
    }
    qInfo().noquote() << QStringLiteral("[%1 s] %2")
                             .arg(QString::number(runner.elapsed_seconds(), 'f', 0))
                             .arg(parts.join(QStringLiteral(" | ")));
  };
  while (!runner.finished()) {
    if (runner.elapsed_seconds() + 1.0e-4F >= next_digest) {
      result.digests.push_back(
          {runner.elapsed_seconds(), Game::Session::world_digest(world)});
      next_digest += std::max(options.digest_interval_seconds, step);
    }
    if (options.trace_interval_seconds > 0.0F &&
        runner.elapsed_seconds() + 1.0e-4F >= next_trace) {
      trace();
      next_trace += options.trace_interval_seconds;
    }
    world.update(step);
    runner.update(step);
  }
  result.elapsed_seconds = runner.elapsed_seconds();
  result.events = runner.events();
  result.report = runner.report();
  result.final_digest = Game::Session::world_digest(world);
  result.wall_seconds = static_cast<double>(wall.elapsed()) / 1000.0;
  handles.clear();
  session->terrain().clear();
  return result;
}

auto compare(const Result& first, const Result& second) -> DeterminismReport {
  DeterminismReport report;
  report.runs = 2;
  const std::size_t samples = std::min(first.digests.size(), second.digests.size());
  for (std::size_t i = 0; i < samples; ++i) {
    if (first.digests[i].digest != second.digests[i].digest) {
      report.divergent_at = first.digests[i].time_seconds;
      report.detail = QStringLiteral("world digest diverged at %1 s")
                          .arg(QString::number(report.divergent_at, 'f', 2));
      return report;
    }
  }
  if (first.digests.size() != second.digests.size() ||
      first.final_digest != second.final_digest) {
    report.divergent_at = first.elapsed_seconds;
    report.detail = QStringLiteral("final world digest differs");
    return report;
  }
  if (first.events.size() != second.events.size()) {
    report.detail = QStringLiteral("phase event count differs");
    return report;
  }
  for (std::size_t i = 0; i < first.events.size(); ++i) {
    if (first.events[i].name != second.events[i].name ||
        first.events[i].time_seconds != second.events[i].time_seconds) {
      report.detail = QStringLiteral("phase event %1 differs (%2 at %3 vs %4 at %5)")
                          .arg(i)
                          .arg(first.events[i].name)
                          .arg(first.events[i].time_seconds)
                          .arg(second.events[i].name)
                          .arg(second.events[i].time_seconds);
      return report;
    }
  }
  report.deterministic = true;
  report.detail = QStringLiteral("%1 digest samples and %2 phase events identical")
                      .arg(samples)
                      .arg(first.events.size());
  return report;
}

auto timeline_json(const std::vector<Result>& results,
                   const QString& id) -> QByteArray {
  QJsonArray matches;
  for (const auto& result : results) {
    QJsonArray events;
    for (const auto& event : result.events) {
      events.append(
          QJsonObject{{QStringLiteral("event"), event.name},
                      {QStringLiteral("at"), static_cast<double>(event.time_seconds)}});
    }
    QJsonArray sides;
    for (const auto& side : result.report.battle.sides) {
      sides.append(QJsonObject{
          {QStringLiteral("label"), side.label},
          {QStringLiteral("owner_id"), side.owner_id},
          {QStringLiteral("peak_units"), side.peak_units},
          {QStringLiteral("living_units"), side.living_units},
          {QStringLiteral("eliminated_at"), static_cast<double>(side.eliminated_at)}});
    }
    matches.append(QJsonObject{
        {QStringLiteral("scenario"), result.scenario_id},
        {QStringLiteral("headless"), true},
        {QStringLiteral("elapsed_seconds"),
         static_cast<double>(result.elapsed_seconds)},
        {QStringLiteral("decided"), result.report.battle.decided},
        {QStringLiteral("victor"), result.report.battle.victor_label},
        {QStringLiteral("decided_at"),
         static_cast<double>(result.report.battle.decided_at_seconds)},
        {QStringLiteral("world_digest"),
         QStringLiteral("%1").arg(result.final_digest, 16, 16, QLatin1Char('0'))},
        {QStringLiteral("events"), events},
        {QStringLiteral("sides"), sides}});
  }
  return QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                   {QStringLiteral("matches"), matches}})
      .toJson(QJsonDocument::Indented);
}

} // namespace Arena::Headless
