#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/core/component_economy.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
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
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/unit_activity.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_map_size = 180;
constexpr int k_owner = 2;
constexpr int k_opening_builders = 5;

constexpr int k_origin = k_map_size / 2;

struct Census {
  int barracks = 0;
  int homes = 0;
  int farms = 0;
  int towers = 0;
  int walls = 0;
  int gates = 0;
  int markets = 0;
  int engines = 0;

  [[nodiscard]] auto total() const -> int {
    return barracks + homes + farms + towers + walls + markets + engines;
  }
};

struct Muster {
  int builders = 0;
  int civilians = 0;
  int fighters = 0;
  bool commander_alive = false;
};

struct Purse {
  int food = 0;
  int wood = 0;
  int stone = 0;
  int iron = 0;
  int gold = 0;

  int hauled = 0;
};

struct WorkSample {
  int working = 0;
  int idle = 0;

  int stuck = 0;
  int unreachable = 0;
  int target_lost = 0;
  int interrupted = 0;

  int gather_unreachable = 0;
  int gather_lost = 0;

  int loitering = 0;
};

struct Minute {
  float at = 0.0F;
  Census census;
  Muster muster;
  Purse purse;
  WorkSample work;
  std::string state;
};

class AiEstateEconomyTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_world_state();
    ASSERT_TRUE(Game::Systems::AI::load_default_ai_doctrine_catalog())
        << "the shipped assets/data/ai files did not load";
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

  static void lay_out_the_estate(Game::Map::MapDefinition& map) {
    const auto add = [&map](Game::Map::WorldProp::Type type, float x, float z) {
      const float grid_x = x + static_cast<float>(k_origin);
      const float grid_z = z + static_cast<float>(k_origin);
      if (grid_x < 2.0F || grid_z < 2.0F ||
          grid_x >= static_cast<float>(k_map_size - 2) ||
          grid_z >= static_cast<float>(k_map_size - 2)) {
        return;
      }
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = grid_x;
      prop.z = grid_z;
      map.world_props.push_back(prop);
    };

    const auto row = [&add](Game::Map::WorldProp::Type type,
                            int count,
                            float x,
                            float z,
                            float step_x,
                            float step_z) {
      for (int index = 0; index < count; ++index) {
        add(type,
            x + (step_x * static_cast<float>(index)),
            z + (step_z * static_cast<float>(index)));
      }
    };

    using Type = Game::Map::WorldProp::Type;
    row(Type::OliveTree, 5, -38.0F, -4.0F, -2.9F, -0.8F);
    row(Type::OliveTree, 4, -35.0F, -8.4F, -2.9F, -0.8F);
    row(Type::OliveTree, 4, -39.5F, -13.0F, -2.9F, -0.8F);
    row(Type::OliveTree, 4, -34.0F, 12.0F, -2.9F, 0.8F);
    row(Type::OliveTree, 4, -31.5F, 16.6F, -2.9F, 0.8F);
    row(Type::PineTree, 5, -34.0F, -30.0F, -2.9F, -0.8F);
    row(Type::PineTree, 4, -32.0F, -34.8F, -2.9F, -0.8F);
    row(Type::PineTree, 4, -36.0F, -39.4F, -2.9F, -0.8F);
    row(Type::CypressTree, 4, -42.0F, 20.0F, -2.9F, 0.8F);

    row(Type::Boulder, 5, -18.0F, 34.0F, -2.9F, 0.8F);
    row(Type::Boulder, 4, -16.5F, 38.2F, -2.9F, 0.8F);
    row(Type::Boulder, 4, 6.0F, 37.0F, 2.9F, 0.8F);
    row(Type::Boulder, 3, 7.0F, 41.0F, 2.9F, 0.8F);
    row(Type::Boulder, 4, 32.0F, 22.0F, 2.9F, 0.8F);

    row(Type::IronOre, 3, 38.0F, -6.0F, 2.9F, -0.8F);
    row(Type::IronOre, 3, 36.5F, -10.0F, 2.9F, -0.8F);
    row(Type::IronOre, 3, 40.0F, 8.0F, 2.9F, 0.8F);
    row(Type::IronOre, 3, 30.0F, -20.0F, 2.9F, -0.8F);
  }

  auto settle() -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);

    session.owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::AI, "hanno");
    session.owners().set_owner_team(k_owner, 1);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_owner, Game::Systems::NationID::Carthage);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    lay_out_the_estate(map_definition);
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());

    auto& economy = session.economy();
    economy.ensure_owner(k_owner);
    economy.set(k_owner, Game::Systems::ResourceType::Gold, 12000);
    economy.set(k_owner, Game::Systems::ResourceType::Food, 600);
    economy.set(k_owner, Game::Systems::ResourceType::Wood, 900);
    economy.set(k_owner, Game::Systems::ResourceType::Stone, 700);
    economy.set(k_owner, Game::Systems::ResourceType::Iron, 400);

    spawn(session, Game::Units::SpawnType::Barracks, world_of(0.0F, 0.0F));
    spawn(
        session, Game::Units::SpawnType::CarthageSpearCommander, world_of(0.0F, 6.0F));
    for (int index = 0; index < k_opening_builders; ++index) {
      spawn(session,
            Game::Units::SpawnType::Builder,
            world_of(-9.0F + (4.4F * static_cast<float>(index)), 9.0F));
    }

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      auto profile =
          Game::Systems::AI::doctrine_profile_for_owner(session.world(), k_owner);
      EXPECT_TRUE(profile.has_value()) << "Hanno has no authored doctrine";
      if (profile.has_value()) {
        ai->set_ai_profile(k_owner, *profile);
      }
    }
    return session;
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
    params.max_population = 160;
    params.enables_production = true;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, session.world(), params);
    return unit ? unit->id() : 0;
  }

  static auto world_of(float x, float z) -> QVector3D { return {x, 0.0F, z}; }

  static void run_for(SessionContext& session, double seconds) {
    const double step = session.clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      session.clock().advance(step);
      while (session.clock().consume_tick()) {
        session.world().update(static_cast<float>(step));
      }
    }
  }

  static auto census_of(SessionContext& session) -> Census {
    Census census;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          !Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      switch (unit.spawn_type) {
      case Game::Units::SpawnType::Barracks:
        ++census.barracks;
        break;
      case Game::Units::SpawnType::Home:
        ++census.homes;
        break;
      case Game::Units::SpawnType::Farm:
        ++census.farms;
        break;
      case Game::Units::SpawnType::DefenseTower:
        ++census.towers;
        break;
      case Game::Units::SpawnType::WallGate:
        ++census.walls;
        ++census.gates;
        break;
      case Game::Units::SpawnType::WallSegment:
        ++census.walls;
        break;
      case Game::Units::SpawnType::Marketplace:
        ++census.markets;
        break;
      case Game::Units::SpawnType::Catapult:
      case Game::Units::SpawnType::Ballista:
        ++census.engines;
        break;
      default:
        break;
      }
    }
    return census;
  }

  static auto muster_of(SessionContext& session) -> Muster {
    Muster muster;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      if (unit.spawn_type == Game::Units::SpawnType::Builder) {
        ++muster.builders;
        continue;
      }
      if (unit.spawn_type == Game::Units::SpawnType::Civilian) {
        ++muster.civilians;
        continue;
      }
      const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      if (troop.has_value() && Game::Units::is_commander_troop(*troop)) {
        muster.commander_alive = true;
        continue;
      }
      ++muster.fighters;
    }
    return muster;
  }

  static auto purse_of(SessionContext& session) -> Purse {
    auto& economy = session.economy();
    const auto carried = economy.get_harvested_all(k_owner);
    int hauled = 0;
    for (const auto type : Game::Systems::k_all_resource_types) {
      hauled += carried.get(type);
    }
    return Purse{economy.get(k_owner, Game::Systems::ResourceType::Food),
                 economy.get(k_owner, Game::Systems::ResourceType::Wood),
                 economy.get(k_owner, Game::Systems::ResourceType::Stone),
                 economy.get(k_owner, Game::Systems::ResourceType::Iron),
                 economy.get(k_owner, Game::Systems::ResourceType::Gold),
                 hauled};
  }

  static auto work_sample(SessionContext& session) -> WorkSample {
    WorkSample sample;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          unit.spawn_type != Game::Units::SpawnType::Builder) {
        continue;
      }
      const auto* builder =
          session.world().try_get<Engine::Core::BuilderProductionComponent>(id);
      if (builder != nullptr && builder->has_active_fault()) {
        const bool gathering =
            Game::Systems::is_gather_builder_product(builder->product_type);
        if (gathering &&
            builder->fault == Engine::Core::BuilderTaskFault::Unreachable) {
          ++sample.gather_unreachable;
        }
        if (gathering && builder->fault == Engine::Core::BuilderTaskFault::TargetLost) {
          ++sample.gather_lost;
        }
        switch (builder->fault) {
        case Engine::Core::BuilderTaskFault::Unreachable:
          ++sample.unreachable;
          break;
        case Engine::Core::BuilderTaskFault::TargetLost:
          ++sample.target_lost;
          break;
        case Engine::Core::BuilderTaskFault::Interrupted:
          ++sample.interrupted;
          break;
        case Engine::Core::BuilderTaskFault::None:
          break;
        }
      }
      if (builder != nullptr && builder->has_construction_site &&
          !builder->at_construction_site &&
          Game::Systems::is_gather_builder_product(builder->product_type) &&
          builder->site_approach_seconds > 5.0F) {
        const auto* transform =
            session.world().try_get<Engine::Core::TransformComponent>(id);
        if (transform != nullptr) {
          const float dx = transform->position.x - builder->construction_site_x;
          const float dz = transform->position.z - builder->construction_site_z;
          constexpr float k_arms_reach = 2.0F;
          if ((dx * dx) + (dz * dz) < k_arms_reach * k_arms_reach) {
            ++sample.loitering;
          }
        }
      }
      const auto activity = Game::Systems::classify_unit_activity(session.world(), id);
      if (activity.kind == Game::Systems::ActivityKind::Idle) {
        ++sample.idle;
      } else {
        ++sample.working;
      }
      if (activity.state == Game::Systems::ActivityState::Unavailable &&
          (builder == nullptr || !builder->has_active_fault())) {
        ++sample.stuck;
      }
    }
    return sample;
  }

  static auto ai_state(SessionContext& session) -> std::string {
    auto* ai = session.world().get_system<Game::Systems::AISystem>();
    const auto* plan = ai != nullptr ? ai->plan_for(k_owner) : nullptr;
    if (plan == nullptr) {
      return "none";
    }
    return Game::Systems::AI::AIStrategyFactory::state_to_string(plan->state)
        .toStdString();
  }

  static auto overlapping_buildings(SessionContext& session) -> int {
    struct Standing {
      float x = 0.0F;
      float z = 0.0F;
      float radius = 0.0F;
      bool is_rampart = false;
      std::string type;
    };
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
      const auto type = Game::Units::spawn_typeToString(unit.spawn_type);
      const auto size =
          Game::Systems::BuildingCollisionRegistry::get_building_size(type);
      town.push_back(Standing{transform->position.x,
                              transform->position.z,
                              0.5F * std::min(size.width, size.depth),
                              Game::Units::is_wall_network_spawn(unit.spawn_type),
                              type});
    }
    int overlaps = 0;
    for (std::size_t i = 0; i < town.size(); ++i) {
      for (std::size_t j = i + 1; j < town.size(); ++j) {

        if (town[i].is_rampart && town[j].is_rampart) {
          continue;
        }
        const float dx = town[i].x - town[j].x;
        const float dz = town[i].z - town[j].z;
        const float reach = town[i].radius + town[j].radius;
        if ((dx * dx) + (dz * dz) < reach * reach * 0.9F) {
          ++overlaps;
          if (qEnvironmentVariableIsSet("SOI_ESTATE_LOG")) {
            std::printf("   overlap: %s (%.1f, %.1f) into %s (%.1f, %.1f)\n",
                        town[i].type.c_str(),
                        static_cast<double>(town[i].x),
                        static_cast<double>(town[i].z),
                        town[j].type.c_str(),
                        static_cast<double>(town[j].x),
                        static_cast<double>(town[j].z));
          }
        }
      }
    }
    return overlaps;
  }

  auto watch(double minutes) -> std::vector<Minute> {
    auto& session = settle();
    std::vector<Minute> timeline;
    const bool log = qEnvironmentVariableIsSet("SOI_ESTATE_LOG");
    if (log) {
      std::printf("\n  min | bar home farm tower wall mkt | bld civ fight | "
                  "work idle stuck unreach lost intr | food wood stone iron gold"
                  " hauled | state\n");
    }
    for (int minute = 1; minute <= static_cast<int>(minutes); ++minute) {
      run_for(session, 60.0);
      Minute sample;
      sample.at = static_cast<float>(minute);
      sample.census = census_of(session);
      sample.muster = muster_of(session);
      sample.purse = purse_of(session);
      sample.work = work_sample(session);
      sample.state = ai_state(session);
      if (log) {
        std::printf("  %3d | %3d %4d %4d %5d %4d %3d | %3d %3d %5d | %4d %4d %5d "
                    "%7d %4d %4d | %4d %4d %5d %4d %5d %6d | %s\n",
                    minute,
                    sample.census.barracks,
                    sample.census.homes,
                    sample.census.farms,
                    sample.census.towers,
                    sample.census.walls,
                    sample.census.markets,
                    sample.muster.builders,
                    sample.muster.civilians,
                    sample.muster.fighters,
                    sample.work.working,
                    sample.work.idle,
                    sample.work.stuck,
                    sample.work.unreachable,
                    sample.work.target_lost,
                    sample.work.interrupted,
                    sample.purse.food,
                    sample.purse.wood,
                    sample.purse.stone,
                    sample.purse.iron,
                    sample.purse.gold,
                    sample.purse.hauled,
                    sample.state.c_str());
      }
      timeline.push_back(sample);
    }
    m_overlaps = overlapping_buildings(session);
    return timeline;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  int m_overlaps = 0;
};

TEST_F(AiEstateEconomyTest, ARichCommanderAloneOnItsLandRaisesAWorkingTown) {
  const auto timeline = watch(30.0);
  ASSERT_FALSE(timeline.empty());
  const auto& end = timeline.back();

  EXPECT_GE(end.census.barracks, 1) << "the estate lost its barracks";
  EXPECT_GE(end.census.homes, 6)
      << "an economic doctrine with a full purse raised only " << end.census.homes
      << " homes in half an hour";
  EXPECT_GE(end.census.farms, 3) << "a town that does not break its fields is a camp";
  EXPECT_GE(end.census.markets, 1) << "the trade town never raised its market";
  EXPECT_GE(end.census.towers, 2) << "the authored plan puts towers on the corners";
  EXPECT_GE(end.census.total(), 20)
      << "the whole estate came to " << end.census.total() << " buildings";
  EXPECT_EQ(m_overlaps, 0) << "buildings were raised standing inside each other";
}

TEST_F(AiEstateEconomyTest, ItsBuildersAreNeverLeftWithNothingToDo) {
  const auto timeline = watch(30.0);
  ASSERT_FALSE(timeline.empty());

  int idle_minutes = 0;
  int loitering_minutes = 0;
  int abandoned_harvest_minutes = 0;
  for (const auto& minute : timeline) {
    if (minute.at < 2.0F) {
      continue;
    }
    if (minute.work.working == 0 && minute.muster.builders > 0) {
      ++idle_minutes;
    }
    if (minute.work.loitering > 0) {
      ++loitering_minutes;
    }
    if (minute.work.gather_unreachable > 0) {
      ++abandoned_harvest_minutes;
    }
  }
  EXPECT_LE(idle_minutes, 3)
      << "the whole builder gang stood with no job in " << idle_minutes
      << " of the sampled minutes; an estate with this much ground and this much "
         "gold should always have a next thing to do";
  EXPECT_EQ(loitering_minutes, 0)
      << "a harvest crew stood within arm's reach of its own node without "
         "working it in "
      << loitering_minutes
      << " of the sampled minutes; that is the loitering this scene exists to "
         "keep fixed, and it should never be observable at all";

  EXPECT_LE(abandoned_harvest_minutes, 6)
      << "a harvest crew gave up on a node it could not reach in "
      << abandoned_harvest_minutes << " of the sampled minutes";
}

TEST_F(AiEstateEconomyTest, ItSpendsWhatItGathersInsteadOfHoardingIt) {
  const auto timeline = watch(30.0);
  ASSERT_FALSE(timeline.empty());
  const auto& end = timeline.back();

  EXPECT_LT(end.purse.wood, 4000)
      << "wood piled up to " << end.purse.wood << " unspent";
  EXPECT_LT(end.purse.stone, 4000)
      << "stone piled up to " << end.purse.stone << " unspent";
  EXPECT_GT(end.muster.fighters, 4)
      << "with 12000 gold and a barracks the garrison came to only "
      << end.muster.fighters;
}

} // namespace
