#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_map_size = 128;
constexpr int k_north = 2;
constexpr int k_south = 3;
constexpr int k_north_grid = 26;
constexpr int k_south_grid = k_map_size - 26;
constexpr int k_opening_builders = 4;

struct SideReport {
  int buildings = 0;
  int barracks = 0;
  int homes = 0;
  int farms = 0;
  int towers = 0;
  int walls = 0;
  int builders = 0;
  int civilians = 0;
  int fighters = 0;
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
};

class AiDuelMatchTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::TerrainService::instance().clear();
  }

  auto make_duel(Game::Units::SpawnType north_commander,
                 Game::Systems::NationID north_nation,
                 Game::Units::SpawnType south_commander,
                 Game::Systems::NationID south_nation) -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);

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
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());
    session.troop_counts().initialize();

    for (const int owner : {k_north, k_south}) {
      auto& economy = session.economy();
      economy.ensure_owner(owner);
      economy.set(owner, Game::Systems::ResourceType::Gold, 250);
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
      add(Game::Map::WorldProp::Type::OliveTree, grid_x + offset, grid_z);
      add(Game::Map::WorldProp::Type::OliveTree, grid_x - offset, grid_z);
      add(Game::Map::WorldProp::Type::PineTree, grid_x, grid_z + offset);
      add(Game::Map::WorldProp::Type::PineTree, grid_x, grid_z - offset);
      add(Game::Map::WorldProp::Type::Boulder, grid_x + offset, grid_z + 6);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset, grid_z - 6);
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

  static void
  observe(SessionContext& session, int owner, float minute, SideHistory& history) {
    const auto report = survey(session, owner);
    auto* ai = session.world().get_system<Game::Systems::AISystem>();
    const auto* plan = ai != nullptr ? ai->plan_for(owner) : nullptr;
    if (report.commander_alive) {
      history.commander_last_distance_from_home =
          commander_distance_from_home(session, owner);
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
      std::printf(
          "  %-10s bar %d home %d farm %d tow %d wall %d | work %d/%d "
          "fight %d | food %d manpower %d | left %d homesfirst %d | "
          "wave %s(%d) %s\n",
          side.second,
          report.barracks,
          report.homes,
          report.farms,
          report.towers,
          report.walls,
          report.builders,
          report.civilians,
          report.fighters,
          economy.get(side.first, Game::Systems::ResourceType::Food),
          barracks_manpower(session, side.first),
          plan != nullptr ? plan->home_civilians_remaining : -1,
          plan != nullptr && plan->macro_targets.raise_homes_first ? 1 : 0,
          (plan != nullptr && plan->wave.committed) ? "yes" : "no",
          plan != nullptr ? static_cast<int>(plan->wave.members.size()) : 0,
          plan != nullptr
              ? Game::Systems::AI::AIStrategyFactory::state_to_string(plan->state)
                    .toUtf8()
                    .constData()
              : "?");
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
            int minutes) -> DuelOutcome {
    make_duel(north_commander, north_nation, south_commander, south_nation);
    auto& session = *m_session;
    DuelOutcome outcome;
    for (int minute = 1; minute <= minutes; ++minute) {
      run_for(session, 60.0);
      const auto now = static_cast<float>(minute);
      observe(session, k_north, now, outcome.north);
      observe(session, k_south, now, outcome.south);
      narrate(session, now, north_name, south_name);
    }
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
