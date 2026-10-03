#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/wall_walk_geometry.h"
#include "core/world.h"
#include "map/map_transformer.h"
#include "map/terrain_service.h"
#include "systems/building_collision_registry.h"
#include "systems/movement/command_service.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/wall_walk_path.h"
#include "systems/owner_registry.h"
#include "systems/siege_tower_system.h"
#include "units/factory.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

constexpr int k_defender = 1;
constexpr int k_attacker = 2;

class SiegeTowerSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    NavGrid::initialize(33, 33);
    auto& owners = OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(k_defender, OwnerType::Player, "Defender");
    owners.register_owner_with_id(k_attacker, OwnerType::AI, "Attacker");
    owners.set_owner_team(k_defender, 1);
    owners.set_owner_team(k_attacker, 2);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    OwnerRegistry::instance().clear();
    BuildingCollisionRegistry::instance().clear();
  }

  static auto
  make_wall(World& world, float x, float z, int owner, bool stair = false) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    entity->add_component<RenderableComponent>();
    auto* unit = entity->add_component<UnitComponent>(800, 800, 0.0F, 0.0F);
    unit->owner_id = owner;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    entity->add_component<BuildingComponent>();
    auto* wall = entity->add_component<WallSegmentComponent>();
    wall->connection_mask = 0b1010U;
    wall->inner_z = 1;
    wall->has_stair = stair;
    return entity;
  }

  auto make_swordsman(World& world, float x, float z, int owner) -> Entity* {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = owner;
    params.spawn_type = Game::Units::SpawnType::Swordsman;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    params.is_initial_spawn = false;
    auto unit = m_factory->create(params.spawn_type, world, params);
    return unit ? world.get_entity(unit->id()) : nullptr;
  }

  auto make_tower(World& world, float x, float z) -> Entity* {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = k_attacker;
    params.spawn_type = Game::Units::SpawnType::SiegeTower;
    params.nation_id = Game::Systems::NationID::Carthage;
    params.is_initial_spawn = false;
    auto unit = m_factory->create(params.spawn_type, world, params);
    return unit ? world.get_entity(unit->id()) : nullptr;
  }

  static auto find_walker(World& world) -> Entity* {
    for (auto [id, walker] : world.view<WallWalkerComponent>()) {
      (void)walker;
      return world.get_entity(id);
    }
    return nullptr;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

} // namespace

namespace WW = Game::Systems::WallWalk;

TEST_F(SiegeTowerSystemTest, TowerDrivesUpBetweenTwoPostsAndDropsItsBridge) {
  World world;
  for (int i = 0; i < 6; ++i) {
    make_wall(world, -6.0F + 2.0F * static_cast<float>(i), 0.0F, k_defender);
  }
  auto* tower = make_tower(world, -3.2F, -4.0F);
  ASSERT_NE(tower, nullptr);

  SiegeTowerSystem system;
  system.update(&world, 0.1F);

  auto* state = tower->get_component<SiegeTowerComponent>();
  ASSERT_NE(state, nullptr);
  EXPECT_EQ(state->state, SiegeTowerComponent::State::Approaching);
  EXPECT_NEAR(state->dock_x, -3.0F, 1.0e-4F);
  EXPECT_NEAR(state->dock_z, -1.8F, 1.0e-4F);
  EXPECT_TRUE(state->garrison_aboard);
  EXPECT_EQ(find_walker(world), nullptr);

  auto* transform = tower->get_component<TransformComponent>();
  transform->position.x = state->dock_x;
  transform->position.z = state->dock_z;
  system.update(&world, 0.1F);
  EXPECT_EQ(state->state, SiegeTowerComponent::State::Docked);

  for (int i = 0; i < 40; ++i) {
    system.update(&world, 0.1F);
  }
  EXPECT_FLOAT_EQ(state->ramp, 1.0F);
  EXPECT_FALSE(state->garrison_aboard);

  auto* walker = find_walker(world);
  ASSERT_NE(walker, nullptr);
  EXPECT_EQ(walker->get_component<UnitComponent>()->owner_id, k_attacker);
  auto const* wall_walk = walker->get_component<WallWalkerComponent>();
  EXPECT_EQ(wall_walk->phase, WallWalkerComponent::Phase::Boarding);
  EXPECT_NEAR(wall_walk->crest_x, -3.0F, 1.0e-3F);
  EXPECT_NEAR(wall_walk->crest_z, 0.0F, 1.0e-3F);
  auto const* lands = walker->get_component<TransformComponent>();
  EXPECT_NEAR(lands->position.z, WW::k_deck_lane, 1.0e-3F);

  system.update(&world, 0.1F);
  int walkers = 0;
  for (auto [id, w] : world.view<WallWalkerComponent>()) {
    (void)id;
    (void)w;
    ++walkers;
  }
  EXPECT_EQ(walkers, 1);
}

TEST_F(SiegeTowerSystemTest, TowerIgnoresItsOwnWalls) {
  World world;
  for (int i = 0; i < 4; ++i) {
    make_wall(world, -4.0F + 2.0F * static_cast<float>(i), 0.0F, k_attacker);
  }
  auto* tower = make_tower(world, -3.0F, -2.6F);
  ASSERT_NE(tower, nullptr);
  SiegeTowerSystem system;
  system.update(&world, 0.1F);
  EXPECT_EQ(tower->get_component<SiegeTowerComponent>()->state,
            SiegeTowerComponent::State::Rolling);
  EXPECT_EQ(find_walker(world), nullptr);
}

TEST_F(SiegeTowerSystemTest, TowerInsideTheTownDoesNotDock) {
  World world;
  for (int i = 0; i < 4; ++i) {
    make_wall(world, -4.0F + 2.0F * static_cast<float>(i), 0.0F, k_defender);
  }
  auto* tower = make_tower(world, -3.0F, 2.6F);
  ASSERT_NE(tower, nullptr);
  SiegeTowerSystem system;
  system.update(&world, 0.1F);
  EXPECT_EQ(tower->get_component<SiegeTowerComponent>()->state,
            SiegeTowerComponent::State::Rolling);
}

TEST_F(SiegeTowerSystemTest, WallWalkerFollowsTheBalconyAndLeavesByTheStair) {
  World world;
  for (int i = 0; i < 8; ++i) {
    make_wall(world, -8.0F + 2.0F * static_cast<float>(i), 0.0F, k_defender, i == 6);
  }
  auto* troop = make_swordsman(world, -6.0F, WW::k_deck_lane, k_defender);
  ASSERT_NE(troop, nullptr);
  auto* walker = troop->add_component<WallWalkerComponent>();
  walker->wall_id = 0;
  for (auto [id, unit] : world.view<UnitComponent>()) {
    if (unit.spawn_type == Game::Units::SpawnType::WallSegment &&
        std::abs(world.try_get<TransformComponent>(id)->position.x + 6.0F) < 0.01F) {
      walker->wall_id = id;
    }
  }
  ASSERT_NE(walker->wall_id, 0U);

  WallWalkSystem walkers;
  auto* movement = troop->get_component<MovementComponent>();
  auto* transform = troop->get_component<TransformComponent>();
  movement->engage_manual_move(2.0F, WW::k_deck_lane);
  for (int i = 0; i < 200; ++i) {
    walkers.update(&world, 0.1F);
  }
  EXPECT_NEAR(transform->position.x, 2.0F, 0.05F);
  EXPECT_NEAR(transform->position.z, WW::k_deck_lane, 0.01F);
  EXPECT_FLOAT_EQ(troop->get_component<WallWalkerComponent>()->elevation,
                  WW::k_deck_height);
  EXPECT_EQ(troop->get_component<UnitComponent>()->formation_files_override, 0)
      << "a walker that never climbed keeps whatever files it was given";

  Game::Systems::CommandService::move_unit(world, troop->get_id(), {4.0F, 0.0F, 14.0F});
  bool descended = false;
  float lowest = WW::k_deck_height;
  for (int i = 0; i < 400 && troop->has_component<WallWalkerComponent>(); ++i) {
    walkers.update(&world, 0.1F);
    if (auto const* w = troop->get_component<WallWalkerComponent>()) {
      descended = descended || w->phase == WallWalkerComponent::Phase::Descending;
      lowest = std::min(lowest, w->elevation);
    }
  }
  EXPECT_TRUE(descended);
  EXPECT_FALSE(troop->has_component<WallWalkerComponent>());
  EXPECT_LT(lowest, 0.2F);
  auto const foot = WW::stair_foot(4.0F, 0.0F, 0, 1);
  EXPECT_NEAR(transform->position.x, foot.x, 0.05F);
  EXPECT_NEAR(transform->position.z, foot.z, 0.05F);
}

TEST_F(SiegeTowerSystemTest, TroopOrderedOntoItsOwnWallClimbsTheStair) {
  World world;
  for (int i = 0; i < 8; ++i) {
    make_wall(world, -8.0F + 2.0F * static_cast<float>(i), 0.0F, k_defender, i == 2);
  }
  auto* troop = make_swordsman(world, 3.0F, 6.0F, k_defender);
  ASSERT_NE(troop, nullptr);
  Game::Systems::CommandService::move_unit(
      world, troop->get_id(), {3.0F, 0.0F, WW::k_deck_lane});

  WallWalkSystem walkers;
  walkers.update(&world, 0.1F);
  auto* walker = troop->get_component<WallWalkerComponent>();
  ASSERT_NE(walker, nullptr);
  EXPECT_EQ(walker->phase, WallWalkerComponent::Phase::Approaching);

  auto const foot = WW::stair_foot(-4.0F, 0.0F, 0, 1);
  auto* transform = troop->get_component<TransformComponent>();
  transform->position.x = foot.x;
  transform->position.z = foot.z;
  walkers.update(&world, 0.1F);
  EXPECT_EQ(walker->phase, WallWalkerComponent::Phase::Climbing);
  EXPECT_EQ(troop->get_component<UnitComponent>()->formation_files_override, 1);

  for (int i = 0; i < 300; ++i) {
    walkers.update(&world, 0.1F);
  }
  walker = troop->get_component<WallWalkerComponent>();
  ASSERT_NE(walker, nullptr);
  EXPECT_EQ(walker->phase, WallWalkerComponent::Phase::OnDeck);
  EXPECT_FLOAT_EQ(walker->elevation, WW::k_deck_height);
  EXPECT_NEAR(transform->position.x, 2.0F, 0.15F);
  EXPECT_NEAR(transform->position.z, WW::k_deck_lane, 0.01F);
}

TEST(WallWalkGeometryTest, BalconyOrdersCoverTheRunButNotTheFieldOutside) {
  EXPECT_TRUE(WW::is_wall_walk_order(0.0F, 0.0F, 0, 1, 0.9F, WW::k_deck_lane));
  EXPECT_TRUE(WW::is_wall_walk_order(0.0F, 0.0F, 0, 1, -0.3F, 0.1F));
  EXPECT_FALSE(WW::is_wall_walk_order(0.0F, 0.0F, 0, 1, 0.0F, -1.2F));
  EXPECT_FALSE(WW::is_wall_walk_order(0.0F, 0.0F, 0, 1, 0.0F, 2.0F));
  EXPECT_FALSE(WW::is_wall_walk_order(0.0F, 0.0F, 0, 1, 1.6F, WW::k_deck_lane));
}

TEST(WallWalkGeometryTest, ProjectionCarriesTheWalkingHeight) {
  std::vector<WallWalkSegment> path{{0.0F, 0.0F, 0.0F, 0.0F, 2.0F, 2.0F},
                                    {0.0F, 2.0F, 2.0F, 4.0F, 2.0F, 2.0F}};
  auto const mid = WW::project_onto_path(path, 0.4F, 1.0F);
  ASSERT_TRUE(mid.valid);
  EXPECT_NEAR(mid.x, 0.0F, 1.0e-5F);
  EXPECT_NEAR(mid.y, 1.0F, 1.0e-5F);
  auto const deck = WW::project_onto_path(path, 3.0F, 2.6F);
  EXPECT_NEAR(deck.z, 2.0F, 1.0e-5F);
  EXPECT_NEAR(deck.y, 2.0F, 1.0e-5F);
}
