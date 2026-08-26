#include <algorithm>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

// Nothing in the repo watched the computer open a skirmish. The one AI scenario
// in `battlefield_gameplay_verifier` hands it twelve knights already spawned,
// so it exercises the fighting AI and never the economy - which is why every AI
// test was green while a watched computer-only match went nowhere: five or six
// units and two or three buildings inside forty seconds, then nothing at all
// for the next eight minutes of simulated time. No gold or food was ever
// gathered, no army was ever built, and no side ever attacked.
//
// This is a skirmish opening exactly as the shipped maps author one: a barracks,
// a builder, a healer and a commander per side, a starting stock of resources,
// and two rival camps. It asserts that the opening actually goes somewhere.
namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_map_size = 96;
constexpr int k_left = 1;
constexpr int k_right = 2;

struct ArmySnapshot {
  int troops = 0;
  int fighting_troops = 0;
  int buildings = 0;
  int population = 0;
};

class AiSkirmishOpeningTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    // Completed recruits are spawned through the map transformer's registry,
    // not the one a test holds.
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::TerrainService::instance().clear();
  }

  auto make_match() -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);

    auto& owners = session.owners();
    owners.register_owner_with_id(k_left, Game::Systems::OwnerType::AI, "left");
    owners.register_owner_with_id(k_right, Game::Systems::OwnerType::AI, "right");
    owners.set_owner_team(k_left, 1);
    owners.set_owner_team(k_right, 2);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_left, Game::Systems::NationID::RomanRepublic);
    session.nations().set_player_nation(k_right, Game::Systems::NationID::Carthage);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;

    // A camp with nothing to cut or quarry has no economy at all, and every
    // shipped map scatters both around a start. Without these the harness
    // measures a resource drought rather than the computer's plan.
    scatter_resources(map_definition, 20);
    scatter_resources(map_definition, k_map_size - 20);
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());
    session.troop_counts().initialize();

    // What every shipped skirmish map hands a side at the opening bell.
    for (const int owner : {k_left, k_right}) {
      auto& economy = session.economy();
      economy.ensure_owner(owner);
      economy.set(owner, Game::Systems::ResourceType::Gold, 250);
      economy.set(owner, Game::Systems::ResourceType::Food, 200);
      economy.set(owner, Game::Systems::ResourceType::Wood, 250);
      economy.set(owner, Game::Systems::ResourceType::Stone, 120);
      economy.set(owner, Game::Systems::ResourceType::Iron, 80);
    }

    seat_camp(session, k_left, 20);
    seat_camp(session, k_right, k_map_size - 20);

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      for (const int owner : {k_left, k_right}) {
        Game::Systems::AI::AIPlayerProfile profile;
        profile.strategy = Game::Systems::AI::AIStrategy::Expansionist;
        profile.posture = Game::Systems::AI::AIPosture::Field;
        profile.personality.aggression = 0.6F;
        profile.personality.defense = 0.4F;
        profile.personality.harassment = 0.35F;
        ai->set_ai_profile(owner, profile);
      }
    }
    return session;
  }

  static void scatter_resources(Game::Map::MapDefinition& map, int grid_x) {
    const int grid_z = k_map_size / 2;
    // Shipped maps author world props in grid coordinates and so does the
    // default MapDefinition, so these have to be grid too. Authored in world
    // coordinates they are converted a second time on load and land far outside
    // the map, which is a resource drought dressed up as a scatter.
    const auto add = [&map](Game::Map::WorldProp::Type type, int x, int z) {
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = static_cast<float>(x);
      prop.z = static_cast<float>(z);
      map.world_props.push_back(prop);
    };
    for (int ring = 0; ring < 6; ++ring) {
      const int offset = 8 + ring * 2;
      add(Game::Map::WorldProp::Type::PineTree, grid_x, grid_z + offset);
      add(Game::Map::WorldProp::Type::PineTree, grid_x, grid_z - offset);
      add(Game::Map::WorldProp::Type::OliveTree, grid_x + offset, grid_z);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset, grid_z + 4);
      add(Game::Map::WorldProp::Type::Boulder, grid_x - offset, grid_z - 4);
    }
  }

  void seat_camp(SessionContext& session, int owner, int grid_x) {
    const int grid_z = k_map_size / 2;
    spawn(session, Game::Units::SpawnType::Barracks, owner, world_of(grid_x, grid_z));
    spawn(session,
          Game::Units::SpawnType::Builder,
          owner,
          world_of(grid_x + 3, grid_z + 2));
    spawn(session,
          Game::Units::SpawnType::Healer,
          owner,
          world_of(grid_x + 3, grid_z - 2));
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
    params.nation_id = session.nations().get_nation_for_player(owner_id) != nullptr
                           ? session.nations().get_nation_for_player(owner_id)->id
                           : Game::Systems::NationID::RomanRepublic;
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

  static auto survey(SessionContext& session, int owner) -> ArmySnapshot {
    ArmySnapshot snapshot;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != owner || unit.health <= 0) {
        continue;
      }
      if (Game::Units::is_building_spawn(unit.spawn_type)) {
        ++snapshot.buildings;
        continue;
      }
      ++snapshot.troops;
      if (unit.spawn_type != Game::Units::SpawnType::Builder &&
          unit.spawn_type != Game::Units::SpawnType::Healer &&
          unit.spawn_type != Game::Units::SpawnType::Civilian) {
        ++snapshot.fighting_troops;
      }
    }
    snapshot.population = session.troop_counts().get_troop_count(owner);
    return snapshot;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

} // namespace

TEST_F(AiSkirmishOpeningTest, TheComputerFieldsFightingTroops) {
  auto& session = make_match();

  const auto opening = survey(session, k_left);
  ASSERT_EQ(opening.buildings, 1);
  ASSERT_EQ(opening.troops, 2);
  ASSERT_EQ(opening.fighting_troops, 0);

  run_for(session, 120.0);

  const auto after = survey(session, k_left);
  EXPECT_GT(after.troops, opening.troops)
      << "the computer recruited nothing at all in two minutes";
  EXPECT_GE(after.fighting_troops, 1)
      << "the computer fielded " << after.fighting_troops
      << " fighting units in two minutes; a builder gang is not an army";
}

// The computer used to spend its opening manpower on builders and then have no
// way to get any more: it never staffed a home, never walked a civilian to a
// barracks, and treated the home-only lifetime cap as though it retired the
// barracks. This watches the loop that undoes all three - the reserve has to
// come back up after it is spent.
TEST_F(AiSkirmishOpeningTest, TheComputerRefillsItsRecruitmentReserve) {
  auto& session = make_match();

  const auto reserve_of = [&session](int owner) {
    int total = 0;
    for (auto [id, prod] : session.world().view<Engine::Core::ProductionComponent>()) {
      const auto* unit = session.world().try_get<Engine::Core::UnitComponent>(id);
      if (unit == nullptr || unit->owner_id != owner ||
          unit->spawn_type != Game::Units::SpawnType::Barracks) {
        continue;
      }
      total += prod.manpower_available;
    }
    return total;
  };

  const int opening_reserve = reserve_of(k_left);
  ASSERT_GT(opening_reserve, 0);

  int lowest = opening_reserve;
  int recovered = 0;
  for (int slice = 0; slice < 30; ++slice) {
    run_for(session, 10.0);
    const int now = reserve_of(k_left);
    lowest = std::min(lowest, now);
    recovered = std::max(recovered, now - lowest);
  }

  EXPECT_LT(lowest, opening_reserve) << "the computer never spent its opening manpower";
  EXPECT_GT(recovered, 0)
      << "the computer spent its manpower down to " << lowest
      << " and never got any of it back: no civilian ever reached a barracks";
}

TEST_F(AiSkirmishOpeningTest, TheComputerStaffsItsHomes) {
  auto& session = make_match();
  run_for(session, 180.0);

  int homes = 0;
  int civilians_raised = 0;
  for (auto [id, prod] : session.world().view<Engine::Core::ProductionComponent>()) {
    const auto* unit = session.world().try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || unit->owner_id != k_left ||
        unit->spawn_type != Game::Units::SpawnType::Home) {
      continue;
    }
    ++homes;
    civilians_raised += prod.produced_count;
  }

  EXPECT_GT(homes, 0) << "the computer built no homes";
  EXPECT_GT(civilians_raised, 0) << "the computer built " << homes
                                 << " homes and never put a single family in one";
}

TEST_F(AiSkirmishOpeningTest, TheComputerSpendsWhatItGathers) {
  auto& session = make_match();
  auto& economy = session.economy();

  const int opening_wood = economy.get(k_left, Game::Systems::ResourceType::Wood);
  const int opening_iron = economy.get(k_left, Game::Systems::ResourceType::Iron);

  run_for(session, 180.0);

  const int spent_wood =
      opening_wood - economy.get(k_left, Game::Systems::ResourceType::Wood);
  const int spent_iron =
      opening_iron - economy.get(k_left, Game::Systems::ResourceType::Iron);

  EXPECT_GT(spent_wood + spent_iron, 0)
      << "the computer sat on its whole opening stock for three minutes";
}

// Every nation lists its commanders at the top of its priority table, and a
// barracks refuses to recruit one. The computer asked for the highest-priority
// troop it could name and had that request dropped on every single decision,
// which is how Rome went a whole match without an army while looking busy.
TEST_F(AiSkirmishOpeningTest, TheComputerDoesNotPlanRecruitsTheWorldWillRefuse) {
  auto& session = make_match();
  auto* ai = session.world().get_system<Game::Systems::AISystem>();
  ASSERT_NE(ai, nullptr);

  run_for(session, 120.0);

  const auto applied = ai->applied_command_count();
  const auto refused = ai->refused_command_count();
  ASSERT_GT(applied, 0U) << "the computer issued no commands at all";

  EXPECT_LT(refused * 4U, applied)
      << refused << " of " << applied
      << " commands were refused outright; the plan is mostly requests the world "
         "will never grant";
}

// `produced_count` is a home's civilian tally. `ProductionService` caps a home
// by it and a barracks by nothing, and `ProductionSystem` stops only a home
// when it is reached - so a barracks that has spent its opening budget may
// still recruit as soon as a civilian refills it. The computer used to apply
// the home rule to its barracks and retire the building for good.
TEST_F(AiSkirmishOpeningTest, ABarracksIsNotRetiredByItsLifetimeTally) {
  auto& session = make_match();

  Engine::Core::EntityID barracks_id = 0;
  for (auto [id, unit] : session.world().view<Engine::Core::UnitComponent>()) {
    if (unit.owner_id == k_left &&
        unit.spawn_type == Game::Units::SpawnType::Barracks) {
      barracks_id = id;
      break;
    }
  }
  ASSERT_NE(barracks_id, 0U);

  auto* production =
      session.world().try_get<Engine::Core::ProductionComponent>(barracks_id);
  ASSERT_NE(production, nullptr);

  // Spent its whole lifetime budget, then refilled by a civilian.
  production->produced_count = production->max_units + 100;
  production->manpower_available = 280;

  EXPECT_EQ(Game::Systems::ProductionService::can_start_production(
                session.world(), barracks_id, Game::Units::TroopType::Archer),
            Game::Systems::ProductionResult::Success)
      << "the world would refuse this recruit, so the computer is right to skip it";
}

namespace {

constexpr int k_ward = 3;

// A third camp pitched right beside the left one, close enough to sit inside
// its 30 m defence radius, owned by whichever nation the caller names.
void seat_neighbour(SessionContext& session,
                    Game::Systems::NationID nation,
                    const std::function<EntityID(int, QVector3D)>& spawn) {
  auto& owners = session.owners();
  owners.register_owner_with_id(k_ward, Game::Systems::OwnerType::AI, "ward");
  owners.set_owner_team(k_ward, 3);
  session.nations().set_player_nation(k_ward, nation);

  for (int step = 0; step < 3; ++step) {
    spawn(k_ward,
          Game::Systems::NavGrid::grid_to_world(
              Game::Systems::Point(20 + 12, (k_map_size / 2) + step - 1)));
  }
}

} // namespace

// The Iron Sepulcher is a bonus nation: it has no economy, so the state machine
// never lets it attack or expand and its troops hold their own ground for the
// whole match. A camp pitched near a tomb used to read that garrison as an
// incursion on every single update, latch `Defending`, and sit there for the
// rest of the game - which is exactly what a watched computer-only match
// showed. A faction that will never come at you is not a threat. One that
// actually strikes a building still is, and that arrives on the
// BuildingAttacked event rather than by proximity, so nothing is lost.
TEST_F(AiSkirmishOpeningTest, AGarrisonNationCampedNextDoorIsNotAStandingThreat) {
  auto& session = make_match();
  auto* ai = session.world().get_system<Game::Systems::AISystem>();
  ASSERT_NE(ai, nullptr);

  seat_neighbour(session,
                 Game::Systems::NationID::IronSepulcher,
                 [this, &session](int owner, QVector3D position) {
                   return spawn(
                       session, Game::Units::SpawnType::Knight, owner, position);
                 });

  run_for(session, 60.0);

  const auto* plan = ai->plan_for(k_left);
  ASSERT_NE(plan, nullptr);
  EXPECT_EQ(plan->nearby_threat_count, 0)
      << "a garrison that cannot march was counted as " << plan->nearby_threat_count
      << " incursions";
  EXPECT_FALSE(plan->barracks_under_threat)
      << "the barracks was declared under threat by neighbours who never attacked it";
  EXPECT_NE(plan->state, Game::Systems::AI::AIState::Defending)
      << "the computer is defending against a faction that will never come at it";
}

// The other half of the same rule: this must exclude only the nations that
// genuinely cannot march. Put a marching nation's troops in the identical spot
// and the camp has to notice them, or the fix above is just the threat check
// switched off.
TEST_F(AiSkirmishOpeningTest, AMarchingNationCampedNextDoorStillReadsAsAThreat) {
  auto& session = make_match();
  auto* ai = session.world().get_system<Game::Systems::AISystem>();
  ASSERT_NE(ai, nullptr);

  seat_neighbour(session,
                 Game::Systems::NationID::Carthage,
                 [this, &session](int owner, QVector3D position) {
                   return spawn(
                       session, Game::Units::SpawnType::Knight, owner, position);
                 });

  run_for(session, 60.0);

  const auto* plan = ai->plan_for(k_left);
  ASSERT_NE(plan, nullptr);
  EXPECT_GT(plan->nearby_threat_count, 0)
      << "three enemy knights twelve metres from the barracks went unnoticed";
}

// A tree, a boulder and an ore seam are all stamped out of the navigation grid,
// so a worker sent to one's centre is held out by its own footprint - a few
// centimetres short of the fifteen the arrival check wants. It never arrives,
// and because arriving was the only way out of the task, it never failed
// either: five builders stood around a wood shortage for an entire match while
// the stockpile sat frozen. Work the prop from beside it.
TEST_F(AiSkirmishOpeningTest, AHarvestOrderSendsTheWorkerBesideThePropNotOntoIt) {
  auto& session = make_match();
  auto& terrain = session.terrain();

  const Game::Map::WorldProp* tree = nullptr;
  for (const auto& prop : terrain.world_props()) {
    if (Game::Map::is_tree_world_prop_type(prop.type)) {
      tree = &prop;
      break;
    }
  }
  ASSERT_NE(tree, nullptr);

  Engine::Core::EntityID builder_id = 0;
  for (auto [id, unit] : session.world().view<UnitComponent>()) {
    if (unit.owner_id == k_left && unit.spawn_type == Game::Units::SpawnType::Builder) {
      builder_id = id;
      break;
    }
  }
  ASSERT_NE(builder_id, 0U);

  const QVector3D placed = terrain.world_prop_world_position(*tree);
  Game::Command::submit(
      session.world(),
      Game::Command::Source::AI,
      k_left,
      Game::Command::StartHarvest{.units = {builder_id},
                                  .construction_type = "cut_tree",
                                  .resource_target = tree->id,
                                  .site = QVector3D(placed.x(), 0.0F, placed.z())});
  run_for(session, 0.2);

  const auto* builder =
      session.world().try_get<Engine::Core::BuilderProductionComponent>(builder_id);
  ASSERT_NE(builder, nullptr);
  ASSERT_TRUE(builder->has_construction_site);

  const float standoff = std::hypot(builder->construction_site_x - placed.x(),
                                    builder->construction_site_z - placed.z());
  EXPECT_GE(standoff, Game::Map::world_prop_ground_radius(tree->type, tree->scale))
      << "the worker was sent to stand " << standoff
      << " m from the tree's centre, inside the footprint that blocks it";
  EXPECT_TRUE(
      Game::Systems::NavGrid::is_grid_walkable(Game::Systems::NavGrid::world_to_grid(
          builder->construction_site_x, builder->construction_site_z)))
      << "the worker was sent to stand on ground it cannot hold";

  // The prop itself is still what gets harvested; only where the worker stands
  // has moved.
  EXPECT_FLOAT_EQ(builder->task_target_x, placed.x());
  EXPECT_FLOAT_EQ(builder->task_target_z, placed.z());
}

// The other half: a work site the worker genuinely cannot reach has to end in
// something. The bypass walk is a straight line that knows nothing about what
// is in the way, so a site behind a building pins the worker against the wall
// while the unstick pass shoves it back - and in the old code that was the rest
// of the match, because the task had no exit but arrival. The movement trace
// reported it for what it was: an order active for ninety seconds with no
// terminal outcome, repeating forever on the same body.
TEST_F(AiSkirmishOpeningTest, AWorkerThatCannotReachItsSiteGivesUpInsteadOfHanging) {
  auto& session = make_match();

  Engine::Core::EntityID builder_id = 0;
  for (auto [id, unit] : session.world().view<UnitComponent>()) {
    if (unit.owner_id == k_left && unit.spawn_type == Game::Units::SpawnType::Builder) {
      builder_id = id;
      break;
    }
  }
  ASSERT_NE(builder_id, 0U);

  auto* builder =
      session.world().try_get<Engine::Core::BuilderProductionComponent>(builder_id);
  ASSERT_NE(builder, nullptr);

  // A site under the camp's own barracks: the worker is pushed out of the
  // footprint every tick it walks into it, so it can never stand there.
  builder->product_type = "cut_tree";
  builder->build_time = 6.0F;
  builder->time_remaining = 6.0F;
  builder->has_construction_site = true;
  builder->at_construction_site = false;
  builder->in_progress = false;
  builder->site_approach_seconds = 0.0F;
  const QVector3D barracks = world_of(20, k_map_size / 2);
  builder->construction_site_x = barracks.x();
  builder->construction_site_z = barracks.z();

  // Just past the limit, while the fault is still on display.
  run_for(session, 31.0);

  EXPECT_FALSE(builder->has_construction_site)
      << "the worker is still holding a site it has spent half a minute failing to "
         "reach";
  EXPECT_EQ(builder->fault, Engine::Core::BuilderTaskFault::Unreachable)
      << "the task ended without saying why, so nothing can react to it";
}

// The opening stock is spent in the first minute; everything after that is
// income. A computer whose workers hang on their first task has no income at
// all, which is what a watched match showed - every resource frozen from t=60
// to the end, and an army that stopped at two fighting units because a barracks
// with no manpower cannot recruit.
TEST_F(AiSkirmishOpeningTest, TheComputerKeepsGatheringAfterItsOpeningStockIsGone) {
  auto& session = make_match();
  auto& economy = session.economy();
  const auto gathered = [&economy]() {
    return economy.get(k_left, Game::Systems::ResourceType::Wood) +
           economy.get(k_left, Game::Systems::ResourceType::Stone) +
           economy.get(k_left, Game::Systems::ResourceType::Iron);
  };

  run_for(session, 100.0);
  const int after_the_opening = gathered();
  run_for(session, 200.0);
  const int later = gathered();

  EXPECT_GT(later, after_the_opening)
      << "the stockpile went from " << after_the_opening << " to " << later
      << " over three minutes; the workers are not bringing anything in";
}
