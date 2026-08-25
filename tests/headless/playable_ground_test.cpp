

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/walkability.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Map::WorldProp;
using Game::Session::SessionContext;
using Game::Systems::BodyProfile;
using Game::Systems::CommandService;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_player = 1;
constexpr int k_enemy = 2;
constexpr int k_map = 48;

class PlayableGroundTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    NavGrid::initialize(k_map, k_map);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  auto field(const std::vector<WorldProp>& props = {}) -> SessionContext& {
    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    map.world_props = props;

    m_scope.reset();
    m_session.reset();
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_player, Game::Systems::OwnerType::Player, "carthage");
    m_session->owners().set_owner_team(k_player, 1);
    m_session->owners().register_owner_with_id(
        k_enemy, Game::Systems::OwnerType::AI, "rome");
    m_session->owners().set_owner_team(k_enemy, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    if (auto* pathfinder = NavGrid::get_pathfinder()) {
      pathfinder->mark_navigation_grid_dirty();
      pathfinder->update_navigation_grid();
    }
    return *m_session;
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return NavGrid::grid_to_world(Point(grid_x, grid_z));
  }

  auto
  spawn(Game::Units::SpawnType type, const QVector3D& position, int owner) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner;
    params.spawn_type = type;
    params.nation_id = owner == k_player ? Game::Systems::NationID::Carthage
                                         : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void run_for(double seconds) {
    const double step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
    }
  }

  auto position_of(EntityID id) -> QVector3D {
    auto* entity = m_session->world().get_entity(id);
    if (entity == nullptr) {
      return {};
    }
    const auto* transform = entity->get_component<TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  static auto body_of(EntityID) -> BodyProfile {
    BodyProfile profile;
    return profile;
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

auto ruins_at(int grid_x, int grid_z) -> WorldProp {
  WorldProp prop;
  prop.type = WorldProp::Type::Ruins;
  prop.x = static_cast<float>(grid_x);
  prop.z = static_cast<float>(grid_z);
  return prop;
}

} // namespace

TEST_F(PlayableGroundTest, DuellistsNeverFightOnTopOfARuin) {
  field({ruins_at(24, 24)});

  const EntityID ours =
      spawn(Game::Units::SpawnType::Knight, world_of(22, 29), k_player);
  const EntityID theirs =
      spawn(Game::Units::SpawnType::Knight, world_of(26, 29), k_enemy);
  ASSERT_NE(ours, 0U);
  ASSERT_NE(theirs, 0U);

  bool ever_locked = false;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 20.0; elapsed += step) {
    run_for(step);
    for (const EntityID id : {ours, theirs}) {
      auto* entity = m_session->world().get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      const auto* attack = entity->get_component<AttackComponent>();
      ever_locked = ever_locked || (attack != nullptr && attack->in_melee_lock);
      const QVector3D at = position_of(id);
      ASSERT_TRUE(Game::Systems::Walkability::can_stand(at, body_of(id)))
          << "a fighter stood on blocked ground at " << at.x() << ", " << at.z()
          << " after " << elapsed << "s";
    }
  }
  EXPECT_TRUE(ever_locked) << "the two never engaged, so nothing was proved";
}

TEST_F(PlayableGroundTest, ANearbyOrderNeverStartsAwayFromItsGoal) {
  field();

  std::vector<EntityID> army;
  for (int index = 0; index < 6; ++index) {
    const EntityID id = spawn(Game::Units::SpawnType::Spearman,
                              world_of(20 + (index % 3), 22 + (index / 3)),
                              k_player);
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }
  run_for(0.5);

  const QVector3D target = world_of(28, 23);
  std::vector<QVector3D> starts;
  starts.reserve(army.size());
  for (const EntityID id : army) {
    starts.push_back(position_of(id));
  }

  std::vector<QVector3D> targets(army.size(), target);
  CommandService::move_units(m_session->world(), army, targets);

  run_for(0.8);

  for (std::size_t index = 0; index < army.size(); ++index) {
    const QVector3D start = starts[index];
    const QVector3D now = position_of(army[index]);
    const QVector3D intended = target - start;
    const QVector3D travelled = now - start;
    const float progress =
        (travelled.x() * intended.x()) + (travelled.z() * intended.z());
    EXPECT_GE(progress, 0.0F) << "unit " << index
                              << " set off away from its goal: started at ("
                              << start.x() << ", " << start.z() << "), now at ("
                              << now.x() << ", " << now.z() << "), goal (" << target.x()
                              << ", " << target.z() << ")";
  }
}

TEST_F(PlayableGroundTest, NoUnitSettlesOnGroundItMayNotStandOn) {
  field({ruins_at(24, 24)});

  std::vector<EntityID> army;
  for (int index = 0; index < 8; ++index) {
    const EntityID id = spawn(Game::Units::SpawnType::Spearman,
                              world_of(18 + (index % 4), 22 + (index / 4)),
                              k_player);
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }

  std::vector<QVector3D> targets(army.size(), world_of(32, 24));
  CommandService::move_units(m_session->world(), army, targets);
  run_for(25.0);

  for (std::size_t index = 0; index < army.size(); ++index) {
    const QVector3D at = position_of(army[index]);
    EXPECT_TRUE(Game::Systems::Walkability::can_stand(at, body_of(army[index])))
        << "unit " << index << " came to rest on blocked ground at " << at.x() << ", "
        << at.z();
  }
}

TEST_F(PlayableGroundTest, AnOrderIsOnlyAbandonedWhenTheGoalIsTrulyUnreachable) {
  field();

  const EntityID id =
      spawn(Game::Units::SpawnType::Spearman, world_of(20, 24), k_player);
  ASSERT_NE(id, 0U);

  for (int index = 0; index < 6; ++index) {
    ASSERT_NE(spawn(Game::Units::SpawnType::Spearman,
                    world_of(30 + (index % 3), 23 + (index / 3)),
                    k_player),
              0U);
  }

  CommandService::move_unit(m_session->world(), id, world_of(31, 24));
  run_for(20.0);

  auto* entity = m_session->world().get_entity(id);
  ASSERT_NE(entity, nullptr);
  const auto* facts = entity->get_component<Engine::Core::MovementFactsComponent>();
  ASSERT_NE(facts, nullptr);
  EXPECT_NE(facts->progress.state, Engine::Core::MovementOrderState::Unreachable)
      << "a crowded but reachable destination was declared impossible";
  EXPECT_LT((position_of(id) - world_of(31, 24)).length(), 6.0F)
      << "the unit never got near its goal";
}

TEST_F(PlayableGroundTest, AFormationKeepsItsOrderThroughANarrowLane) {
  field();

  constexpr int k_wall_x = 26;
  constexpr int k_gap_z = 24;
  constexpr int k_gap_width = 4;
  for (int grid_z = 4; grid_z < k_map - 4; ++grid_z) {
    if (grid_z >= k_gap_z && grid_z < k_gap_z + k_gap_width) {
      continue;
    }
    const QVector3D at = world_of(k_wall_x, grid_z);
    auto* wall = m_session->world().create_entity();
    wall->add_component<TransformComponent>(at.x(), 0.0F, at.z());
    auto* unit = wall->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_player;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    wall->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        wall->get_id(),
        "wall_segment",
        at.x(),
        at.z(),
        k_player,
        {.width = 1.0F, .depth = 1.0F});
  }
  if (auto* pathfinder = NavGrid::get_pathfinder()) {
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  std::vector<EntityID> army;
  for (int index = 0; index < 9; ++index) {
    const EntityID id = spawn(
        Game::Units::SpawnType::Spearman, world_of(14, 12 + (index * 3)), k_player);
    ASSERT_NE(id, 0U);
    if (auto* entity = m_session->world().get_entity(id)) {
      if (auto* unit = entity->get_component<UnitComponent>()) {
        unit->render_individuals_per_unit_override = 12;
      }
    }
    army.push_back(id);
  }
  run_for(0.5);

  auto const plan = CommandService::plan_ground_move(
      m_session->world(), army, world_of(38, k_gap_z + 1), true);
  CommandService::issue_ground_move(m_session->world(), army, plan);
  if (std::getenv("SOI_DBG_ORDER") != nullptr) {
    for (std::size_t i = 0; i < plan.member_slots.size(); ++i) {
      std::fprintf(stderr,
                   "slot %zu -> z=%.2f (start z=%.2f)\n",
                   i,
                   plan.member_slots[i].position.z(),
                   position_of(army[i]).z());
    }
  }

  run_for(60.0);

  int arrived = 0;
  for (const EntityID id : army) {
    if (position_of(id).x() > world_of(k_wall_x + 4, k_gap_z).x()) {
      ++arrived;
    }
  }
  ASSERT_GE(arrived, 8) << "only " << arrived << " of 9 troops cleared the lane";

  float previous = -std::numeric_limits<float>::max();
  for (std::size_t index = 0; index < army.size(); ++index) {
    float const z = position_of(army[index]).z();
    EXPECT_GT(z + 1.5F, previous)
        << "troop " << index << " came out of the lane on the wrong side of its "
        << "neighbour (z=" << z << ", previous=" << previous << ")";
    previous = z;
  }
}

TEST_F(PlayableGroundTest, ABodyStrandedInsideGeometryWalksItselfOut) {
  const QVector3D ruin = world_of(24, 24);
  field({ruins_at(24, 24)});

  const EntityID id =
      spawn(Game::Units::SpawnType::Spearman, world_of(30, 30), k_player);
  ASSERT_NE(id, 0U);

  auto* entity = m_session->world().get_entity(id);
  ASSERT_NE(entity, nullptr);
  auto* transform = entity->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = ruin.x();
  transform->position.z = ruin.z();
  ASSERT_FALSE(Game::Systems::Walkability::can_stand(ruin, body_of(id)))
      << "the fixture failed to strand the unit";

  run_for(10.0);

  EXPECT_TRUE(Game::Systems::Walkability::can_stand(position_of(id), body_of(id)))
      << "the unit was still inside the ruin at " << position_of(id).x() << ", "
      << position_of(id).z();
}

TEST_F(PlayableGroundTest, ADuellistStrandedInsideGeometryStillWalksOut) {
  const QVector3D ruin = world_of(24, 24);
  field({ruins_at(24, 24)});

  const EntityID ours =
      spawn(Game::Units::SpawnType::Knight, world_of(21, 24), k_player);
  const EntityID theirs =
      spawn(Game::Units::SpawnType::Knight, world_of(27, 24), k_enemy);
  ASSERT_NE(ours, 0U);
  ASSERT_NE(theirs, 0U);

  run_for(6.0);

  auto* entity = m_session->world().get_entity(ours);
  ASSERT_NE(entity, nullptr);
  auto* transform = entity->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = ruin.x();
  transform->position.z = ruin.z();

  run_for(10.0);

  EXPECT_TRUE(Game::Systems::Walkability::can_stand(position_of(ours), body_of(ours)))
      << "a fighter shoved into a ruin never got out";
}

TEST_F(PlayableGroundTest, AFightBesideARuinIsNotBrokenUpByIt) {
  field({ruins_at(24, 24)});

  const EntityID ours =
      spawn(Game::Units::SpawnType::Knight, world_of(20, 27), k_player);
  const EntityID theirs =
      spawn(Game::Units::SpawnType::Knight, world_of(21, 27), k_enemy);
  ASSERT_NE(ours, 0U);
  ASSERT_NE(theirs, 0U);

  int locked_ticks = 0;
  int unlocked_after_first_lock = 0;
  bool ever_locked = false;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 20.0; elapsed += step) {
    run_for(step);
    auto* entity = m_session->world().get_entity(ours);
    if (entity == nullptr) {
      break;
    }
    const auto* attack = entity->get_component<AttackComponent>();
    bool const locked = attack != nullptr && attack->in_melee_lock;
    if (locked) {
      ever_locked = true;
      ++locked_ticks;
    } else if (ever_locked) {
      ++unlocked_after_first_lock;
    }
  }

  ASSERT_TRUE(ever_locked) << "the two never engaged, so nothing was proved";

  EXPECT_GT(locked_ticks, unlocked_after_first_lock)
      << "the ruin kept separating two fighters standing next to each other";
}

TEST_F(PlayableGroundTest, AnUnreachableOrderStillMovesTheGroupAsFarAsItCan) {
  field();

  constexpr int k_wall_x = 30;
  for (int grid_z = 0; grid_z < k_map; ++grid_z) {
    const QVector3D at = world_of(k_wall_x, grid_z);
    auto* wall = m_session->world().create_entity();
    wall->add_component<TransformComponent>(at.x(), 0.0F, at.z());
    auto* unit = wall->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_player;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    wall->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        wall->get_id(),
        "wall_segment",
        at.x(),
        at.z(),
        k_player,
        {.width = 1.0F, .depth = 1.0F});
  }
  if (auto* pathfinder = NavGrid::get_pathfinder()) {
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  std::vector<EntityID> army;
  for (int index = 0; index < 4; ++index) {
    const EntityID id =
        spawn(Game::Units::SpawnType::Spearman, world_of(14, 22 + index), k_player);
    ASSERT_NE(id, 0U);
    army.push_back(id);
  }
  run_for(0.5);

  std::vector<QVector3D> starts;
  starts.reserve(army.size());
  for (const EntityID id : army) {
    starts.push_back(position_of(id));
  }

  auto const plan = CommandService::plan_ground_move(
      m_session->world(), army, world_of(40, 24), true);
  CommandService::issue_ground_move(m_session->world(), army, plan);
  run_for(25.0);

  int advanced = 0;
  for (std::size_t index = 0; index < army.size(); ++index) {
    if (position_of(army[index]).x() > starts[index].x() + 2.0F) {
      ++advanced;
    }
  }
  EXPECT_GE(advanced, 3) << "only " << advanced
                         << " of 4 troops moved towards an unreachable goal";

  for (std::size_t index = 0; index < army.size(); ++index) {
    const QVector3D at = position_of(army[index]);
    EXPECT_TRUE(Game::Systems::Walkability::can_stand(at, body_of(army[index])))
        << "troop " << index << " ended up inside the wall";
    EXPECT_LT(at.x(), world_of(k_wall_x, 24).x())
        << "troop " << index << " walked through a solid wall";
  }
}

TEST_F(PlayableGroundTest, AMarchingSquadTurnsOnSomethingAttackingOneOfThem) {
  field();

  std::vector<EntityID> squad;
  for (int index = 0; index < 4; ++index) {
    const EntityID id =
        spawn(Game::Units::SpawnType::Spearman, world_of(20, 22 + index), k_player);
    ASSERT_NE(id, 0U);
    squad.push_back(id);
  }
  const EntityID raider =
      spawn(Game::Units::SpawnType::Knight, world_of(19, 22), k_enemy);
  ASSERT_NE(raider, 0U);

  std::vector<QVector3D> targets(squad.size(), world_of(40, 24));
  CommandService::move_units(m_session->world(), squad, targets);
  run_for(0.4);
  CommandService::attack_target(m_session->world(), {raider}, squad.front());

  int responders = 0;
  const double step = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 20.0 && responders < 2; elapsed += step) {
    run_for(step);
    responders = 0;
    for (const EntityID id : squad) {
      auto* entity = m_session->world().get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      const auto* target = entity->get_component<Engine::Core::AttackTargetComponent>();
      if (target != nullptr && target->target_id == raider) {
        ++responders;
      }
    }
  }

  EXPECT_GE(responders, 2)
      << "only " << responders
      << " of the marching squad turned on what was attacking one of them";
}

TEST_F(PlayableGroundTest, AHeldUnitFightsBackWithoutLeavingItsPost) {
  field();

  const EntityID victim =
      spawn(Game::Units::SpawnType::Spearman, world_of(24, 24), k_player);
  const EntityID holder =
      spawn(Game::Units::SpawnType::Spearman, world_of(26, 24), k_player);
  const EntityID raider =
      spawn(Game::Units::SpawnType::Knight, world_of(23, 24), k_enemy);
  ASSERT_NE(victim, 0U);
  ASSERT_NE(holder, 0U);
  ASSERT_NE(raider, 0U);

  auto* holder_entity = m_session->world().get_entity(holder);
  ASSERT_NE(holder_entity, nullptr);
  auto* hold = Engine::Core::get_or_add_component<Engine::Core::HoldModeComponent>(
      holder_entity);
  ASSERT_NE(hold, nullptr);
  hold->active = true;

  QVector3D const post = position_of(holder);
  CommandService::attack_target(m_session->world(), {raider}, victim);
  run_for(6.0);

  auto const* target =
      holder_entity->get_component<Engine::Core::AttackTargetComponent>();
  EXPECT_TRUE(target != nullptr && target->target_id != 0U)
      << "a unit holding the line ignored a fight at arm's length";
  EXPECT_LT((position_of(holder) - post).length(), 2.0F)
      << "a unit told to hold its position left it to chase";
}
