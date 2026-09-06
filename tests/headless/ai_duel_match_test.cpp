#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "game/core/component_economy.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/structure_placement_service.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace {

using Engine::Core::BuilderProductionComponent;
using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_map_size = 128;
constexpr int k_north = 2;
constexpr int k_south = 3;
constexpr int k_north_grid = 26;
constexpr int k_south_grid = k_map_size - 26;
constexpr int k_opening_builders = 4;

struct DuelSetup {
  bool river_and_bridge = false;
  int gold = 500;
};

constexpr float k_river_crossing_margin = 8.0F;

auto is_across_the_river(int owner, float x, float z) -> bool {
  const float along = x + z;
  return owner == k_north ? along > k_river_crossing_margin
                          : along < -k_river_crossing_margin;
}

void add_river_and_bridge(Game::Map::MapDefinition& map) {
  using Game::Map::HillShape;
  using Game::Map::TerrainType;
  const auto reach = [](float x, float z) {
    return QVector3D{x, 0.0F, z};
  };
  map.rivers.push_back(
      Game::Map::RiverSegment{reach(-60.10F, 67.18F), reach(-31.47F, 27.93F), 12.0F});
  map.rivers.push_back(
      Game::Map::RiverSegment{reach(-31.47F, 27.93F), reach(-9.90F, 9.90F), 11.0F});
  map.rivers.push_back(
      Game::Map::RiverSegment{reach(-9.90F, 9.90F), reach(9.90F, -9.90F), 10.0F});
  map.rivers.push_back(
      Game::Map::RiverSegment{reach(9.90F, -9.90F), reach(27.93F, -31.47F), 11.0F});
  map.rivers.push_back(
      Game::Map::RiverSegment{reach(27.93F, -31.47F), reach(67.18F, -60.10F), 12.0F});
  map.bridges.push_back(
      Game::Map::Bridge{{-5.7F, 0.0F, -5.7F}, {5.7F, 0.0F, 5.7F}, 10.0F, 0.5F});

  const auto hill = [&map](float x,
                           float z,
                           float radius,
                           float height,
                           HillShape shape,
                           float thickness,
                           float rotation) {
    Game::Map::TerrainFeature feature;
    feature.type = TerrainType::Hill;
    feature.center_x = x;
    feature.center_z = z;
    feature.radius = radius;
    feature.height = height;
    feature.shape = shape;
    feature.thickness = thickness;
    feature.rotation_deg = rotation;
    map.terrain.push_back(feature);
  };
  hill(1.41F, -28.28F, 12.0F, 3.6F, HillShape::Corridor, 5.0F, -45.0F);
  hill(-1.41F, 28.28F, 12.0F, 3.6F, HillShape::Corridor, 5.0F, -45.0F);
  hill(-43.84F, 2.83F, 11.0F, 4.0F, HillShape::Blob, 7.0F, 0.0F);
  hill(43.84F, -2.83F, 11.0F, 4.0F, HillShape::Blob, 7.0F, 0.0F);
  hill(-38.89F, -6.36F, 11.0F, 3.6F, HillShape::Elbow, 5.5F, 135.0F);
  hill(38.89F, 6.36F, 11.0F, 3.6F, HillShape::Arc, 5.0F, -45.0F);
}

struct SideReport {
  int buildings = 0;
  int barracks = 0;
  int homes = 0;
  int farms = 0;
  int towers = 0;
  int walls = 0;
  int markets = 0;
  int builders = 0;
  int civilians = 0;
  int fighters = 0;

  int foot = 0;
  int missile = 0;
  int horse = 0;
  int engines = 0;
  bool commander_alive = false;
};

struct SideHistory {
  int most_buildings = 0;
  int most_homes = 0;
  int most_farms = 0;
  int most_fighters = 0;
  int biggest_wave = 0;
  int harvested = 0;
  float commander_lost_at = -1.0F;
  float commander_last_distance_from_home = 0.0F;

  int most_across_the_river = 0;
  float crossed_the_river_at = -1.0F;

  int buildings_lost = 0;

  float eliminated_at = -1.0F;
};

class AiDuelMatchTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_world_state();
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  static void reset_shared_world_state() {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
    Game::Map::TerrainService::instance().clear();
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::FormationCombat::invalidate_layout_cache();
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    reset_shared_world_state();
  }

  auto make_duel(Game::Units::SpawnType north_commander,
                 Game::Systems::NationID north_nation,
                 Game::Units::SpawnType south_commander,
                 Game::Systems::NationID south_nation,
                 const DuelSetup& setup = {}) -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);

    auto& owners = session.owners();
    owners.register_owner_with_id(k_north, Game::Systems::OwnerType::AI, "north");
    owners.register_owner_with_id(k_south, Game::Systems::OwnerType::AI, "south");
    owners.set_owner_team(k_north, 1);
    owners.set_owner_team(k_south, 2);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_north, north_nation);
    session.nations().set_player_nation(k_south, south_nation);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    scatter_resources(map_definition, k_north_grid, k_north_grid);
    scatter_resources(map_definition, k_south_grid, k_south_grid);
    if (setup.river_and_bridge) {
      add_river_and_bridge(map_definition);
    }
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());

    for (const int owner : {k_north, k_south}) {
      auto& economy = session.economy();
      economy.ensure_owner(owner);
      economy.set(owner, Game::Systems::ResourceType::Gold, setup.gold);
      economy.set(owner, Game::Systems::ResourceType::Food, 200);
      economy.set(owner, Game::Systems::ResourceType::Wood, 250);
      economy.set(owner, Game::Systems::ResourceType::Stone, 120);
      economy.set(owner, Game::Systems::ResourceType::Iron, 80);
    }

    seat_town(session, k_north, k_north_grid, north_commander);
    seat_town(session, k_south, k_south_grid, south_commander);

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      for (const int owner : {k_north, k_south}) {
        auto profile =
            Game::Systems::AI::doctrine_profile_for_owner(session.world(), owner);
        EXPECT_TRUE(profile.has_value())
            << "owner " << owner << " has no authored commander doctrine";
        if (profile.has_value()) {
          ai->set_ai_profile(owner, *profile);
        }
      }
    }
    return session;
  }

  static void scatter_resources(Game::Map::MapDefinition& map, int grid_x, int grid_z) {
    const auto add = [&map](Game::Map::WorldProp::Type type, int x, int z) {
      if (x < 2 || z < 2 || x >= k_map_size - 2 || z >= k_map_size - 2) {
        return;
      }
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = static_cast<float>(x);
      prop.z = static_cast<float>(z);
      map.world_props.push_back(prop);
    };
    for (int ring = 0; ring < 8; ++ring) {
      const int offset = 10 + ring * 2;
      for (int lane = -2; lane <= 2; ++lane) {
        const int shift = lane * 3;
        add(Game::Map::WorldProp::Type::OliveTree, grid_x + offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::OliveTree, grid_x - offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::PineTree, grid_x + shift, grid_z + offset);
        add(Game::Map::WorldProp::Type::PineTree, grid_x + shift, grid_z - offset);
      }
      add(Game::Map::WorldProp::Type::Boulder, grid_x + offset, grid_z + 6);
      add(Game::Map::WorldProp::Type::Boulder, grid_x + offset, grid_z + 9);
      add(Game::Map::WorldProp::Type::Boulder, grid_x - offset, grid_z + 9);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset, grid_z - 6);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset, grid_z - 9);
    }
  }

  void seat_town(SessionContext& session,
                 int owner,
                 int grid,
                 Game::Units::SpawnType commander) {
    spawn(session, Game::Units::SpawnType::Barracks, owner, world_of(grid, grid));
    spawn(session, commander, owner, world_of(grid + 2, grid + 2));
    for (int index = 0; index < k_opening_builders; ++index) {
      spawn(session,
            Game::Units::SpawnType::Builder,
            owner,
            world_of(grid + 4, grid - 3 + index * 2));
    }
  }

  auto spawn(SessionContext& session,
             Game::Units::SpawnType type,
             int owner_id,
             QVector3D position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.ai_controlled = true;
    params.is_initial_spawn = true;
    params.max_population = 280;
    params.enables_production = true;
    const auto* nation = session.nations().get_nation_for_player(owner_id);
    params.nation_id =
        nation != nullptr ? nation->id : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, session.world(), params);
    return unit ? unit->id() : 0;
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return Game::Systems::NavGrid::grid_to_world(Game::Systems::Point(grid_x, grid_z));
  }

  static void run_for(SessionContext& session, double seconds) {
    const double step = session.clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      session.clock().advance(step);
      while (session.clock().consume_tick()) {
        session.world().update(static_cast<float>(step));
      }
    }
  }

  static auto survey(SessionContext& session, int owner) -> SideReport {
    SideReport report;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != owner || unit.health <= 0) {
        continue;
      }
      if (Game::Units::is_building_spawn(unit.spawn_type)) {
        ++report.buildings;
        switch (unit.spawn_type) {
        case Game::Units::SpawnType::Barracks:
          ++report.barracks;
          break;
        case Game::Units::SpawnType::Home:
          ++report.homes;
          break;
        case Game::Units::SpawnType::Farm:
          ++report.farms;
          break;
        case Game::Units::SpawnType::DefenseTower:
          ++report.towers;
          break;
        case Game::Units::SpawnType::WallSegment:
          ++report.walls;
          break;
        case Game::Units::SpawnType::Marketplace:
          ++report.markets;
          break;
        default:
          break;
        }
        continue;
      }
      switch (unit.spawn_type) {
      case Game::Units::SpawnType::Builder:
        ++report.builders;
        break;
      case Game::Units::SpawnType::Civilian:
        ++report.civilians;
        break;
      default:
        if (const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
            troop.has_value() && Game::Units::is_commander_troop(*troop)) {
          report.commander_alive = true;
        } else {
          ++report.fighters;
          switch (unit.spawn_type) {
          case Game::Units::SpawnType::Archer:
            ++report.missile;
            break;
          case Game::Units::SpawnType::MountedKnight:
          case Game::Units::SpawnType::HorseArcher:
          case Game::Units::SpawnType::HorseSpearman:
          case Game::Units::SpawnType::Elephant:
            ++report.horse;
            break;
          case Game::Units::SpawnType::Catapult:
          case Game::Units::SpawnType::Ballista:
            ++report.engines;
            break;
          default:
            ++report.foot;
            break;
          }
        }
        break;
      }
    }
    return report;
  }

  static auto harvested_total(SessionContext& session, int owner) -> int {
    const auto carried = session.economy().get_harvested_all(owner);
    int total = 0;
    for (const auto type : Game::Systems::k_all_resource_types) {
      total += carried.get(type);
    }
    return total;
  }

  static auto barracks_manpower(SessionContext& session, int owner) -> int {
    int manpower = 0;
    for (auto [id, prod] : session.world().view<Engine::Core::ProductionComponent>()) {
      const auto* unit = session.world().try_get<UnitComponent>(id);
      if (unit != nullptr && unit->owner_id == owner &&
          unit->spawn_type == Game::Units::SpawnType::Barracks) {
        manpower += prod.manpower_available;
      }
    }
    return manpower;
  }

  static auto commander_distance_from_home(SessionContext& session,
                                           int owner) -> float {
    auto* ai = session.world().get_system<Game::Systems::AISystem>();
    const auto* plan = ai != nullptr ? ai->plan_for(owner) : nullptr;
    if (plan == nullptr || !plan->has_base_anchor) {
      return 0.0F;
    }
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != owner || unit.health <= 0) {
        continue;
      }
      const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      if (!troop.has_value() || !Game::Units::is_commander_troop(*troop)) {
        continue;
      }
      const auto* transform =
          session.world().try_get<Engine::Core::TransformComponent>(id);
      if (transform == nullptr) {
        continue;
      }
      const float dx = transform->position.x - plan->base_pos_x;
      const float dz = transform->position.z - plan->base_pos_z;
      return std::sqrt((dx * dx) + (dz * dz));
    }
    return 0.0F;
  }

  static auto fighters_across_the_river(SessionContext& session, int owner) -> int {
    int across = 0;
    for (auto [id, unit, transform] :
         session.world().view<UnitComponent, Engine::Core::TransformComponent>()) {
      if (unit.owner_id != owner || unit.health <= 0 ||
          Game::Units::is_building_spawn(unit.spawn_type) ||
          unit.spawn_type == Game::Units::SpawnType::Builder ||
          unit.spawn_type == Game::Units::SpawnType::Civilian) {
        continue;
      }
      if (is_across_the_river(owner, transform.position.x, transform.position.z)) {
        ++across;
      }
    }
    return across;
  }

  static void
  observe(SessionContext& session, int owner, float minute, SideHistory& history) {
    const auto report = survey(session, owner);
    auto* ai = session.world().get_system<Game::Systems::AISystem>();
    const auto* plan = ai != nullptr ? ai->plan_for(owner) : nullptr;
    if (report.commander_alive) {
      history.commander_last_distance_from_home =
          commander_distance_from_home(session, owner);
    }

    const int across = fighters_across_the_river(session, owner);
    history.most_across_the_river = std::max(history.most_across_the_river, across);
    if (across > 0 && history.crossed_the_river_at < 0.0F) {
      history.crossed_the_river_at = minute;
    }
    history.buildings_lost =
        std::max(history.buildings_lost, history.most_buildings - report.buildings);
    if (history.most_buildings > 0 && report.buildings == 0 && report.fighters == 0 &&
        !report.commander_alive && history.eliminated_at < 0.0F) {
      history.eliminated_at = minute;
    }

    history.most_buildings = std::max(history.most_buildings, report.buildings);
    history.most_homes = std::max(history.most_homes, report.homes);
    history.most_farms = std::max(history.most_farms, report.farms);
    history.most_fighters = std::max(history.most_fighters, report.fighters);
    history.harvested = harvested_total(session, owner);

    if (plan != nullptr && plan->wave.committed) {
      history.biggest_wave =
          std::max(history.biggest_wave, static_cast<int>(plan->wave.members.size()));
    }

    if (!report.commander_alive && history.commander_lost_at < 0.0F) {
      history.commander_lost_at = minute;
    }
  }

  static void narrate(SessionContext& session,
                      float minute,
                      const char* north_name,
                      const char* south_name) {
    if (!qEnvironmentVariableIsSet("SOI_DUEL_NARRATE")) {
      return;
    }
    auto* ai = session.world().get_system<Game::Systems::AISystem>();
    auto& economy = session.economy();
    std::printf("t=%5.1f min\n", static_cast<double>(minute));
    const std::pair<int, const char*> sides[] = {{k_north, north_name},
                                                 {k_south, south_name}};
    for (const auto& side : sides) {
      const auto report = survey(session, side.first);
      const auto* plan = ai != nullptr ? ai->plan_for(side.first) : nullptr;
      if (qEnvironmentVariableIsSet("SOI_DUEL_WAVE") && plan != nullptr) {
        std::printf("    %s wave committed %d target %llu at %.1f,%.1f members %zu\n",
                    side.second,
                    plan->wave.committed ? 1 : 0,
                    static_cast<unsigned long long>(plan->wave.target_id),
                    plan->wave.target_x,
                    plan->wave.target_z,
                    plan->wave.members.size());
        for (auto [id, unit, transform, movement] :
             session.world()
                 .view<UnitComponent,
                       Engine::Core::TransformComponent,
                       Engine::Core::MovementComponent>()) {
          if (unit.owner_id != side.first || unit.health <= 0 ||
              Game::Units::is_building_spawn(unit.spawn_type) ||
              unit.spawn_type == Game::Units::SpawnType::Builder ||
              unit.spawn_type == Game::Units::SpawnType::Civilian) {
            continue;
          }
          const bool in_wave =
              std::find(plan->wave.members.begin(), plan->wave.members.end(), id) !=
              plan->wave.members.end();

          auto reach = [&](float gx, float gz) -> std::string {
            auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
            if (pathfinder == nullptr) {
              return "?";
            }
            const auto from = Game::Systems::NavGrid::world_to_grid(
                transform.position.x, transform.position.z);
            const auto to = Game::Systems::NavGrid::world_to_grid(gx, gz);
            const auto path = pathfinder->find_path(from, to);
            const bool complete = !path.empty() && path.back() == to;
            return std::to_string(path.size()) + (complete ? "" : "!");
          };
          const std::string to_objective =
              in_wave ? reach(plan->wave.target_x, plan->wave.target_z) : "-";
          const std::string to_bridge = in_wave ? reach(0.0F, 0.0F) : "-";
          std::printf("      %s%s %llu at %.1f,%.1f hp %d -> goal %.1f,%.1f target %d "
                      "path %zu/%zu | reach objective %s bridge %s\n",
                      in_wave ? "W " : "  ",
                      Game::Units::spawn_typeToString(unit.spawn_type).c_str(),
                      static_cast<unsigned long long>(id),
                      transform.position.x,
                      transform.position.z,
                      unit.health,
                      movement.get_goal_x(),
                      movement.get_goal_y(),
                      movement.get_has_target() ? 1 : 0,
                      movement.get_path_index(),
                      movement.get_path().size(),
                      to_objective.c_str(),
                      to_bridge.c_str());
        }
      }
      if (qEnvironmentVariableIsSet("SOI_DUEL_CIVILIANS")) {
        for (auto [id, unit, transform, movement] :
             session.world()
                 .view<UnitComponent,
                       Engine::Core::TransformComponent,
                       Engine::Core::MovementComponent>()) {
          if (unit.owner_id != side.first || unit.health <= 0 ||
              unit.spawn_type != Game::Units::SpawnType::Civilian) {
            continue;
          }
          const auto here = Game::Systems::NavGrid::world_to_grid(transform.position.x,
                                                                  transform.position.z);
          std::printf("    civilian %llu at %.1f,%.1f (walk %d) -> goal %.1f,%.1f "
                      "target %d path %zu/%zu\n",
                      static_cast<unsigned long long>(id),
                      transform.position.x,
                      transform.position.z,
                      Game::Systems::NavGrid::is_grid_walkable(here) ? 1 : 0,
                      movement.get_goal_x(),
                      movement.get_goal_y(),
                      movement.get_has_target() ? 1 : 0,
                      movement.get_path_index(),
                      movement.get_path().size());
        }
        for (auto [id, unit, transform, prod] :
             session.world()
                 .view<UnitComponent,
                       Engine::Core::TransformComponent,
                       Engine::Core::ProductionComponent>()) {
          if (unit.owner_id != side.first ||
              unit.spawn_type != Game::Units::SpawnType::Barracks) {
            continue;
          }
          std::printf("    barracks %llu at %.1f,%.1f manpower %d/%d (max_units %d) "
                      "queue %zu\n",
                      static_cast<unsigned long long>(id),
                      transform.position.x,
                      transform.position.z,
                      prod.manpower_available,
                      prod.manpower_limit(),
                      prod.max_units,
                      prod.production_queue.size());
        }
      }
      if (qEnvironmentVariableIsSet("SOI_DUEL_BUILDERS")) {
        for (auto [id, unit, transform, movement] :
             session.world()
                 .view<UnitComponent,
                       Engine::Core::TransformComponent,
                       Engine::Core::MovementComponent>()) {
          if (unit.owner_id != side.first ||
              unit.spawn_type != Game::Units::SpawnType::Builder) {
            continue;
          }
          const auto goal = Game::Systems::NavGrid::world_to_grid(
              movement.get_goal_x(), movement.get_goal_y());
          const auto here = Game::Systems::NavGrid::world_to_grid(transform.position.x,
                                                                  transform.position.z);
          std::printf(
              "    builder %llu at %.1f,%.1f (walk %d) -> goal %.1f,%.1f (walk %d) "
              "target %d path %zu/%zu\n",
              static_cast<unsigned long long>(id),
              transform.position.x,
              transform.position.z,
              Game::Systems::NavGrid::is_grid_walkable(here) ? 1 : 0,
              movement.get_goal_x(),
              movement.get_goal_y(),
              Game::Systems::NavGrid::is_grid_walkable(goal) ? 1 : 0,
              movement.get_has_target() ? 1 : 0,
              movement.get_path_index(),
              movement.get_path().size());
          if (const auto* b =
                  session.world().try_get<Engine::Core::BuilderProductionComponent>(
                      id)) {
            const auto* carry =
                session.world().try_get<Engine::Core::ResourceCarryComponent>(id);
            std::printf("      gather %d auto %d prod '%s' site %d(%.1f,%.1f rot %.0f) "
                        "prog %d task %d/%llu carry %d depot %d\n",
                        b->has_gather_order ? 1 : 0,
                        b->auto_gather ? 1 : 0,
                        b->product_type.c_str(),
                        b->has_construction_site ? 1 : 0,
                        b->construction_site_x,
                        b->construction_site_z,
                        b->construction_site_rotation_y,
                        b->in_progress ? 1 : 0,
                        b->has_task_target ? 1 : 0,
                        static_cast<unsigned long long>(b->task_target_id),
                        carry != nullptr ? 1 : 0,
                        carry != nullptr && carry->has_depot ? 1 : 0);
          }
        }
        for (auto [id, unit, transform] :
             session.world().view<UnitComponent, Engine::Core::TransformComponent>()) {
          if (unit.owner_id != side.first ||
              !Game::Units::is_building_spawn(unit.spawn_type)) {
            continue;
          }
          const std::string type = Game::Units::spawn_typeToString(unit.spawn_type);
          std::printf("    %s %llu at %.1f,%.1f rot %.0f\n",
                      type.c_str(),
                      static_cast<unsigned long long>(id),
                      transform.position.x,
                      transform.position.z,
                      transform.rotation.y);
        }
      }
      std::printf(
          "  %-10s bar %d home %d farm %d tow %d wall %d mkt %d | work %d/%d "
          "fight %d (foot %d bow %d horse %d engine %d) | "
          "food %d wood %d stone %d iron %d gold %d manpower %d | "
          "left %d homesfirst %d | pop %d/%d | wave %s(%d) %s | across %d\n",
          side.second,
          report.barracks,
          report.homes,
          report.farms,
          report.towers,
          report.walls,
          report.markets,
          report.builders,
          report.civilians,
          report.fighters,
          report.foot,
          report.missile,
          report.horse,
          report.engines,
          economy.get(side.first, Game::Systems::ResourceType::Food),
          economy.get(side.first, Game::Systems::ResourceType::Wood),
          economy.get(side.first, Game::Systems::ResourceType::Stone),
          economy.get(side.first, Game::Systems::ResourceType::Iron),
          economy.get(side.first, Game::Systems::ResourceType::Gold),
          barracks_manpower(session, side.first),
          plan != nullptr ? plan->home_civilians_remaining : -1,
          plan != nullptr && plan->macro_targets.raise_homes_first ? 1 : 0,
          plan != nullptr ? plan->population_used : -1,
          plan != nullptr ? plan->population_cap : -1,
          (plan != nullptr && plan->wave.committed) ? "yes" : "no",
          plan != nullptr ? static_cast<int>(plan->wave.members.size()) : 0,
          plan != nullptr
              ? Game::Systems::AI::AIStrategyFactory::state_to_string(plan->state)
                    .toUtf8()
                    .constData()
              : "?",
          fighters_across_the_river(session, side.first));
    }
  }

  struct DuelOutcome {
    SideHistory north;
    SideHistory south;
  };

  auto play(Game::Units::SpawnType north_commander,
            Game::Systems::NationID north_nation,
            const char* north_name,
            Game::Units::SpawnType south_commander,
            Game::Systems::NationID south_nation,
            const char* south_name,
            int minutes,
            const DuelSetup& setup = {}) -> DuelOutcome {
    make_duel(north_commander, north_nation, south_commander, south_nation, setup);
    auto& session = *m_session;
    DuelOutcome outcome;
    for (int minute = 1; minute <= minutes; ++minute) {
      run_for(session, 60.0);
      const auto now = static_cast<float>(minute);
      observe(session, k_north, now, outcome.north);
      observe(session, k_south, now, outcome.south);
      narrate(session, now, north_name, south_name);
    }
    expect_the_towns_are_well_laid_out(session);
    return outcome;
  }

  static void expect_one_of_them_fought_a_war(const SideHistory& north,
                                              const SideHistory& south) {
    const int best_army = std::max(north.most_fighters, south.most_fighters);
    const int best_wave = std::max(north.biggest_wave, south.biggest_wave);
    EXPECT_GE(best_army, 5) << "neither side ever fielded more than " << best_army
                            << " soldiers; nobody built an army";
    EXPECT_GE(best_wave, 3) << "neither side ever gathered a wave bigger than "
                            << best_wave << "; nobody attacked";
  }

  static void expect_the_towns_are_well_laid_out(SessionContext& session) {
    struct Standing {
      Engine::Core::EntityID id = 0;
      std::string type;
      float x = 0.0F;
      float z = 0.0F;
      float yaw = 0.0F;
    };

    std::vector<Standing> town;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.health <= 0 || !Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      const auto* transform =
          session.world().try_get<Engine::Core::TransformComponent>(id);
      if (transform == nullptr) {
        continue;
      }
      town.push_back(Standing{.id = id,
                              .type = Game::Units::spawn_typeToString(unit.spawn_type),
                              .x = transform->position.x,
                              .z = transform->position.z,
                              .yaw = transform->rotation.y});
    }

    ASSERT_GE(town.size(), 4U)
        << "the towns were empty, so this says nothing about how they were laid out";

    const auto neighbours_of = [&session](const Standing& building) {
      std::string listed;
      for (const auto& other : session.building_collision().get_all_buildings()) {
        if (other.entity_id == building.id ||
            std::hypot(other.center_x - building.x, other.center_z - building.z) >
                12.0F) {
          continue;
        }
        listed += " #" + std::to_string(other.entity_id) + "@" +
                  std::to_string(other.center_x) + "," +
                  std::to_string(other.center_z) + " " + std::to_string(other.width) +
                  "x" + std::to_string(other.depth) + (other.wall_link ? " link" : "") +
                  (other.blocks_navigation ? "" : " passable");
      }
      for (auto [id, builder] : session.world().view<BuilderProductionComponent>()) {
        if (!builder.has_construction_site ||
            std::hypot(builder.construction_site_x - building.x,
                       builder.construction_site_z - building.z) > 12.0F) {
          continue;
        }
        listed += " site(" + builder.product_type + ")@" +
                  std::to_string(builder.construction_site_x) + "," +
                  std::to_string(builder.construction_site_z) + " by #" +
                  std::to_string(id) + (builder.in_progress ? " working" : " idle") +
                  (builder.at_construction_site ? " there" : " away") + " wallsite " +
                  std::to_string(builder.construction_site_entity_id);
      }
      return listed;
    };

    for (const auto& building : town) {
      const auto verdict = Game::Systems::assess_ground(session.world(),
                                                        building.type,
                                                        building.x,
                                                        building.z,
                                                        building.id,
                                                        building.yaw);
      EXPECT_NE(verdict, Game::Systems::GroundVerdict::Occupied)
          << building.type << " at " << building.x << "," << building.z
          << " overlaps another structure; near it:" << neighbours_of(building);
      EXPECT_NE(verdict, Game::Systems::GroundVerdict::Water)
          << building.type << " at " << building.x << "," << building.z
          << " stands in the water";
      EXPECT_NE(verdict, Game::Systems::GroundVerdict::Uneven)
          << building.type << " at " << building.x << "," << building.z
          << " was raised on ground too steep to build on";
    }
  }

  static void expect_a_commander_played_the_match(const SideHistory& side,
                                                  const char* name) {
    EXPECT_GE(side.most_buildings, 5)
        << name << " raised only " << side.most_buildings << " buildings";
    EXPECT_GE(side.most_homes, 3)
        << name << " raised only " << side.most_homes
        << " homes, so its barracks was always short of manpower";
    EXPECT_GE(side.most_farms, 1)
        << name << " never broke ground on a field, so it ran out of food";
    EXPECT_GE(side.most_fighters, 2)
        << name << " fielded at most " << side.most_fighters
        << " soldiers, so its barracks never really opened";
    EXPECT_GT(side.harvested, 400)
        << name << " carried in only " << side.harvested << " of everything";

    constexpr float k_home_ground = 45.0F;
    EXPECT_TRUE(side.commander_lost_at < 0.0F ||
                side.commander_last_distance_from_home <= k_home_ground)
        << name << " lost its commander at minute " << side.commander_lost_at << ", "
        << side.commander_last_distance_from_home
        << "m from its own base, and its whole nation with him: a lord that far "
           "out was thrown away rather than beaten";
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

} // namespace

TEST_F(AiDuelMatchTest, ScipioAndFabiusBothPlayTheirDoctrine) {
  const auto outcome = play(Game::Units::SpawnType::RomanVeteranConsul,
                            Game::Systems::NationID::RomanRepublic,
                            "scipio",
                            Game::Units::SpawnType::RomanLegionOrganizer,
                            Game::Systems::NationID::RomanRepublic,
                            "fabius",
                            30);

  expect_a_commander_played_the_match(outcome.north, "Scipio");
  expect_a_commander_played_the_match(outcome.south, "Fabius");
  expect_one_of_them_fought_a_war(outcome.north, outcome.south);

  EXPECT_GE(outcome.north.biggest_wave, 4)
      << "Scipio's aggressive doctrine never gathered a wave bigger than "
      << outcome.north.biggest_wave;
}

TEST_F(AiDuelMatchTest, MarcellusAndHannoBothPlayTheirDoctrine) {
  const auto outcome = play(Game::Units::SpawnType::RomanFieldCommander,
                            Game::Systems::NationID::RomanRepublic,
                            "marcellus",
                            Game::Units::SpawnType::CarthageSpearCommander,
                            Game::Systems::NationID::Carthage,
                            "hanno",
                            30);

  expect_a_commander_played_the_match(outcome.north, "Marcellus");
  expect_a_commander_played_the_match(outcome.south, "Hanno");
  expect_one_of_them_fought_a_war(outcome.north, outcome.south);

  EXPECT_GE(outcome.north.biggest_wave, 3)
      << "Marcellus' rushing doctrine never gathered a wave";
}

TEST_F(AiDuelMatchTest, HannibalAndHasdrubalBothPlayTheirDoctrine) {
  const auto outcome = play(Game::Units::SpawnType::CarthageSwordCommander,
                            Game::Systems::NationID::Carthage,
                            "hannibal",
                            Game::Units::SpawnType::CarthageBowCommander,
                            Game::Systems::NationID::Carthage,
                            "hasdrubal",
                            30);

  expect_a_commander_played_the_match(outcome.north, "Hannibal");
  expect_a_commander_played_the_match(outcome.south, "Hasdrubal");
  expect_one_of_them_fought_a_war(outcome.north, outcome.south);
}

TEST_F(AiDuelMatchTest, HannibalMirrorGetsItsArmyAcrossTheRiver) {
  DuelSetup setup;
  setup.river_and_bridge = true;
  setup.gold = 3000;
  const auto outcome = play(Game::Units::SpawnType::CarthageSwordCommander,
                            Game::Systems::NationID::Carthage,
                            "hannibal_n",
                            Game::Units::SpawnType::CarthageSwordCommander,
                            Game::Systems::NationID::Carthage,
                            "hannibal_s",
                            30,
                            setup);

  constexpr float k_full_match_minutes = 15.0F;
  const auto expect_if_it_lived = [&](const SideHistory& side, const char* name) {
    if (side.eliminated_at < 0.0F || side.eliminated_at >= k_full_match_minutes) {
      expect_a_commander_played_the_match(side, name);
    }
  };
  expect_if_it_lived(outcome.north, "Hannibal (north)");
  expect_if_it_lived(outcome.south, "Hannibal (south)");
  expect_one_of_them_fought_a_war(outcome.north, outcome.south);

  EXPECT_FALSE(outcome.north.eliminated_at >= 0.0F &&
               outcome.south.eliminated_at >= 0.0F)
      << "both towns were wiped out";

  const float first_crossing = [&] {
    float best = -1.0F;
    for (const float at :
         {outcome.north.crossed_the_river_at, outcome.south.crossed_the_river_at}) {
      if (at >= 0.0F && (best < 0.0F || at < best)) {
        best = at;
      }
    }
    return best;
  }();
  EXPECT_GE(first_crossing, 0.0F)
      << "neither army ever crossed the river in 30 minutes; the aggressive "
         "doctrine parked at the bridge";
  if (first_crossing >= 0.0F) {
    EXPECT_LE(first_crossing, 15.0F)
        << "the first crossing took " << first_crossing
        << " minutes; a decisive doctrine with 3000 gold should be over the "
           "bridge well inside the first half";
  }
  EXPECT_GE(std::max(outcome.north.most_across_the_river,
                     outcome.south.most_across_the_river),
            3)
      << "no side ever had more than a straggler across the river";
}
