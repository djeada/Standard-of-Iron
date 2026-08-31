#include <QtGlobal>

#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/core/component.h"
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
#include "game/systems/ai_system/ai_doctrine_catalog.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
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
constexpr int k_owner = 2;
constexpr int k_town_grid = 64;
constexpr int k_opening_builders = 5;

struct Standing {
  std::string type;
  float x = 0.0F;
  float z = 0.0F;
};

struct TownCensus {
  int barracks = 0;
  int homes = 0;
  int farms = 0;
  int towers = 0;
  int walls = 0;
  int markets = 0;
  int engines = 0;

  [[nodiscard]] auto total() const -> int {
    return barracks + homes + farms + towers + walls + markets + engines;
  }

  [[nodiscard]] auto signature() const -> std::string {
    return std::to_string(barracks) + "/" + std::to_string(homes) + "/" +
           std::to_string(farms) + "/" + std::to_string(towers) + "/" +
           std::to_string(walls) + "/" + std::to_string(markets);
  }
};

struct ArmyCensus {
  int foot = 0;
  int missile = 0;
  int horse = 0;
  int engines = 0;

  [[nodiscard]] auto total() const -> int { return foot + missile + horse + engines; }
};

class AiTownPlanTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_world_state();
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    reset_shared_world_state();
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

  auto settle(Game::Units::SpawnType commander,
              Game::Systems::NationID nation) -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);

    session.owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::AI, "settler");
    session.owners().set_owner_team(k_owner, 1);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_owner, nation);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    scatter_resources(map_definition, k_town_grid, k_town_grid);
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());

    auto& economy = session.economy();
    economy.ensure_owner(k_owner);
    economy.set(k_owner, Game::Systems::ResourceType::Gold, 600);
    economy.set(k_owner, Game::Systems::ResourceType::Food, 300);
    economy.set(k_owner, Game::Systems::ResourceType::Wood, 400);
    economy.set(k_owner, Game::Systems::ResourceType::Stone, 300);
    economy.set(k_owner, Game::Systems::ResourceType::Iron, 200);

    spawn(session, commander, world_of(k_town_grid + 2, k_town_grid + 2));
    for (int index = 0; index < k_opening_builders; ++index) {
      spawn(session,
            Game::Units::SpawnType::Builder,
            world_of(k_town_grid + 4, k_town_grid - 4 + index * 2));
    }

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      auto profile =
          Game::Systems::AI::doctrine_profile_for_owner(session.world(), k_owner);
      EXPECT_TRUE(profile.has_value()) << "the commander has no authored doctrine";
      if (profile.has_value()) {
        ai->set_ai_profile(k_owner, *profile);
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
    for (int ring = 0; ring < 10; ++ring) {
      const int offset = 22 + ring * 2;
      for (int lane = -3; lane <= 3; ++lane) {
        const int shift = lane * 3;
        add(Game::Map::WorldProp::Type::OliveTree, grid_x + offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::PineTree, grid_x - offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::Boulder, grid_x + shift, grid_z + offset);
        add(Game::Map::WorldProp::Type::IronOre, grid_x + shift, grid_z - offset);
      }
    }
  }

  auto spawn(SessionContext& session,
             Game::Units::SpawnType type,
             QVector3D position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = type;
    params.ai_controlled = true;
    params.is_initial_spawn = true;
    params.max_population = 280;
    params.enables_production = true;
    const auto* nation = session.nations().get_nation_for_player(k_owner);
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

  static auto standing_town(SessionContext& session) -> std::vector<Standing> {
    std::vector<Standing> town;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          !Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      const auto* transform =
          session.world().try_get<Engine::Core::TransformComponent>(id);
      if (transform == nullptr) {
        continue;
      }
      town.push_back(Standing{.type = Game::Units::spawn_typeToString(unit.spawn_type),
                              .x = transform->position.x,
                              .z = transform->position.z});
    }
    return town;
  }

  static auto census_of(const std::vector<Standing>& town) -> TownCensus {
    TownCensus census;
    for (const auto& building : town) {
      if (building.type == "barracks") {
        ++census.barracks;
      } else if (building.type == "home") {
        ++census.homes;
      } else if (building.type == "farm") {
        ++census.farms;
      } else if (building.type == "defense_tower") {
        ++census.towers;
      } else if (building.type == "wall_segment" || building.type == "wall_gate") {
        ++census.walls;
      } else if (building.type == "marketplace") {
        ++census.markets;
      } else if (building.type == "catapult" || building.type == "ballista") {
        ++census.engines;
      }
    }
    return census;
  }

  static auto army_of(SessionContext& session) -> ArmyCensus {
    ArmyCensus army;
    const auto* nation = session.nations().get_nation_for_player(k_owner);
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          Game::Units::is_building_spawn(unit.spawn_type) ||
          unit.spawn_type == Game::Units::SpawnType::Builder ||
          unit.spawn_type == Game::Units::SpawnType::Civilian) {
        continue;
      }
      const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      if (troop.has_value() && Game::Units::is_commander_troop(*troop)) {
        continue;
      }
      if (unit.spawn_type == Game::Units::SpawnType::Catapult ||
          unit.spawn_type == Game::Units::SpawnType::Ballista) {
        ++army.engines;
      } else if (Game::Units::is_cavalry(unit.spawn_type)) {
        ++army.horse;
      } else if (troop.has_value() && nation != nullptr &&
                 nation->is_ranged_unit(*troop)) {
        ++army.missile;
      } else {
        ++army.foot;
      }
    }
    return army;
  }

  static void draw_town(const char* name,
                        const std::vector<Standing>& town,
                        const TownCensus& census,
                        const ArmyCensus& army) {
    if (!qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
      return;
    }
    constexpr int k_span = 46;
    constexpr int k_rows = 23;
    std::vector<std::string> canvas(k_rows,
                                    std::string(static_cast<std::size_t>(k_span), ' '));
    const float centre = static_cast<float>(k_town_grid) - (k_map_size / 2.0F);
    for (const auto& building : town) {
      const int gx = static_cast<int>((building.x - centre) / 2.2F) + (k_span / 2);
      const int gz = static_cast<int>((building.z - centre) / 4.0F) + (k_rows / 2);
      if (gx < 0 || gx >= k_span || gz < 0 || gz >= k_rows) {
        continue;
      }
      char glyph = '?';
      if (building.type == "barracks") {
        glyph = 'B';
      } else if (building.type == "home") {
        glyph = 'h';
      } else if (building.type == "farm") {
        glyph = 'f';
      } else if (building.type == "defense_tower") {
        glyph = 'T';
      } else if (building.type == "wall_segment" || building.type == "wall_gate") {
        glyph = '#';
      } else if (building.type == "marketplace") {
        glyph = 'M';
      } else if (building.type == "catapult" || building.type == "ballista") {
        glyph = 'C';
      }
      canvas[static_cast<std::size_t>(gz)][static_cast<std::size_t>(gx)] = glyph;
    }
    std::printf("\n== %s  bar %d home %d farm %d tower %d wall %d market %d engine %d"
                " | foot %d bow %d horse %d engine %d\n",
                name,
                census.barracks,
                census.homes,
                census.farms,
                census.towers,
                census.walls,
                census.markets,
                census.engines,
                army.foot,
                army.missile,
                army.horse,
                army.engines);
    for (const auto& row : canvas) {
      std::printf("|%s|\n", row.c_str());
    }
  }

  struct Settlement {
    TownCensus census;
    ArmyCensus army;
  };

  static auto barracks_manpower(SessionContext& session) -> int {
    int manpower = 0;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner ||
          unit.spawn_type != Game::Units::SpawnType::Barracks) {
        continue;
      }
      if (const auto* production =
              session.world().try_get<Engine::Core::ProductionComponent>(id)) {
        manpower += production->manpower_available;
      }
    }
    return manpower;
  }

  auto raise_town(const char* name,
                  Game::Units::SpawnType commander,
                  Game::Systems::NationID nation,
                  double minutes) -> Settlement {
    auto& session = settle(commander, nation);
    run_for(session, minutes * 60.0);
    const auto town = standing_town(session);
    const auto census = census_of(town);
    const auto army = army_of(session);
    if (qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
      if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
        if (const auto* plan = ai->plan_for(k_owner); plan != nullptr) {
          const auto* doctrine = plan->strategy_config.doctrine;
          std::printf(
              "   doctrine: %s cav %.2f ranged %.2f siege %.2f | counts melee "
              "%d ranged %d horse %d siege %d\n",
              doctrine != nullptr ? doctrine->id.c_str() : "(none)",
              static_cast<double>(
                  doctrine != nullptr ? doctrine->recruitment.cavalry_share : -1.0F),
              static_cast<double>(
                  doctrine != nullptr ? doctrine->recruitment.ranged_share : -1.0F),
              static_cast<double>(
                  doctrine != nullptr ? doctrine->recruitment.siege_share : -1.0F),
              plan->melee_count,
              plan->ranged_count,
              plan->cavalry_count,
              plan->siege_count);
        }
      }
      auto& economy = session.economy();
      std::printf("   purse: food %d wood %d stone %d iron %d gold %d | manpower %d\n",
                  economy.get(k_owner, Game::Systems::ResourceType::Food),
                  economy.get(k_owner, Game::Systems::ResourceType::Wood),
                  economy.get(k_owner, Game::Systems::ResourceType::Stone),
                  economy.get(k_owner, Game::Systems::ResourceType::Iron),
                  economy.get(k_owner, Game::Systems::ResourceType::Gold),
                  barracks_manpower(session));
    }
    draw_town(name, town, census, army);
    return Settlement{.census = census, .army = army};
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

void expect_a_working_town(const char* name, const TownCensus& census) {
  EXPECT_GE(census.barracks, 1) << name << " never raised a barracks";
  EXPECT_GE(census.homes, 2) << name << " never raised its homes";
  EXPECT_GE(census.farms, 1) << name << " never broke ground on a field";
  EXPECT_GE(census.total(), 8)
      << name << " raised only " << census.total() << " buildings from an empty field";
}

TEST_F(AiTownPlanTest, EveryCommanderRaisesItsOwnTownFromAnEmptyField) {
  struct Settler {
    const char* name;
    Game::Units::SpawnType commander;
    Game::Systems::NationID nation;
  };

  const Settler settlers[] = {
      {"fabius",
       Game::Units::SpawnType::RomanLegionOrganizer,
       Game::Systems::NationID::RomanRepublic},
      {"scipio",
       Game::Units::SpawnType::RomanVeteranConsul,
       Game::Systems::NationID::RomanRepublic},
      {"marcellus",
       Game::Units::SpawnType::RomanFieldCommander,
       Game::Systems::NationID::RomanRepublic},
      {"hanno",
       Game::Units::SpawnType::CarthageSpearCommander,
       Game::Systems::NationID::Carthage},
      {"hasdrubal",
       Game::Units::SpawnType::CarthageBowCommander,
       Game::Systems::NationID::Carthage},
      {"hannibal",
       Game::Units::SpawnType::CarthageSwordCommander,
       Game::Systems::NationID::Carthage},
  };

  std::map<std::string, Settlement> towns;
  for (const auto& settler : settlers) {
    const auto settlement =
        raise_town(settler.name, settler.commander, settler.nation, 26.0);
    expect_a_working_town(settler.name, settlement.census);
    towns.emplace(settler.name, settlement);
    TearDown();
    SetUp();
  }

  std::map<std::string, std::string> by_shape;
  for (const auto& [name, settlement] : towns) {
    const auto signature = settlement.census.signature();
    const auto twin = by_shape.find(signature);
    EXPECT_EQ(twin, by_shape.end())
        << name << " and " << (twin != by_shape.end() ? twin->second : std::string{})
        << " raised the same town (" << signature
        << "); every commander is supposed to build its own";
    by_shape.emplace(signature, name);
  }

  EXPECT_EQ(towns.at("hasdrubal").census.walls, 0)
      << "the Barcid raider camp is authored without a wall; it must stay open";
  EXPECT_GE(towns.at("fabius").census.walls, 3)
      << "the Fabian bulwark is a walled castrum and must raise a frame";
  EXPECT_GE(towns.at("hannibal").census.towers, 3)
      << "the Hannibalic hexagon is authored with a tower on every corner";
  EXPECT_GE(towns.at("marcellus").census.barracks, 2)
      << "the vanguard chevron is authored around three barracks";
}

TEST_F(AiTownPlanTest, ACommanderThatWantsHorseFieldsHorse) {
  const auto raiders = raise_town("hasdrubal",
                                  Game::Units::SpawnType::CarthageBowCommander,
                                  Game::Systems::NationID::Carthage,
                                  22.0);

  EXPECT_GT(raiders.army.missile, raiders.army.foot)
      << "a bow doctrine must field more missile troops than foot";
  EXPECT_GT(raiders.army.horse, 0)
      << "a doctrine with a quarter of its army mounted must be able to pay for a "
         "horse: a barracks that cannot hold the price of its own cavalry never "
         "fields any";
}

} // namespace
