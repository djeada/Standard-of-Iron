#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/map_transformer.h"
#include "map/terrain_service.h"
#include "systems/building_collision_registry.h"
#include "systems/navigation/nav_grid.h"
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

  static auto make_wall(World& world, float x, float z, int owner) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    entity->add_component<RenderableComponent>();
    auto* unit = entity->add_component<UnitComponent>(800, 800, 0.0F, 0.0F);
    unit->owner_id = owner;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    entity->add_component<BuildingComponent>();
    entity->add_component<WallSegmentComponent>();
    return entity;
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

TEST_F(SiegeTowerSystemTest, TowerDocksAtHostileWallAndUnloadsOntoIt) {
  World world;
  for (int i = 0; i < 6; ++i) {
    make_wall(world, -6.0F + 2.0F * static_cast<float>(i), 0.0F, k_defender);
  }
  auto* tower = make_tower(world, -3.0F, -2.6F);
  ASSERT_NE(tower, nullptr);

  SiegeTowerSystem system;
  system.update(&world, 0.1F);

  auto const* state = tower->get_component<SiegeTowerComponent>();
  ASSERT_NE(state, nullptr);
  EXPECT_EQ(state->state, SiegeTowerComponent::State::Docked);
  EXPECT_FALSE(state->garrison_aboard);

  auto* walker = find_walker(world);
  ASSERT_NE(walker, nullptr);
  EXPECT_EQ(walker->get_component<UnitComponent>()->owner_id, k_attacker);

  system.update(&world, 2.0F);
  EXPECT_FLOAT_EQ(state->ramp, 1.0F);
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

TEST_F(SiegeTowerSystemTest, WallWalkerFollowsTheRunAndDropsOffWhenSentAway) {
  World world;
  for (int i = 0; i < 8; ++i) {
    make_wall(world, -8.0F + 2.0F * static_cast<float>(i), 0.0F, k_defender);
  }
  auto* tower = make_tower(world, -7.0F, -2.6F);
  ASSERT_NE(tower, nullptr);
  SiegeTowerSystem towers;
  WallWalkSystem walkers;
  towers.update(&world, 0.1F);
  auto* walker = find_walker(world);
  ASSERT_NE(walker, nullptr);

  auto* movement = walker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  auto* transform = walker->get_component<TransformComponent>();
  float const start_x = transform->position.x;

  movement->engage_manual_move(4.0F, 0.5F);
  for (int i = 0; i < 400; ++i) {
    walkers.update(&world, 0.1F);
  }
  EXPECT_GT(transform->position.x, start_x + 4.0F);
  EXPECT_NEAR(transform->position.z, 0.0F, 0.01F);
  EXPECT_TRUE(walker->has_component<WallWalkerComponent>());

  movement->engage_manual_move(4.0F, 14.0F);
  for (int i = 0; i < 400; ++i) {
    walkers.update(&world, 0.1F);
  }
  EXPECT_FALSE(walker->has_component<WallWalkerComponent>());
}
