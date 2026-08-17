#include <gtest/gtest.h>
#include <string>

#include "core/component.h"
#include "core/world.h"
#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/command/command_validator.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/dismantle_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_system.h"
#include "game/systems/resource_types.h"
#include "game/systems/unit_activity.h"
#include "game/units/spawn_type.h"
#include "save/serialization.h"

namespace {

constexpr int k_owner = 1;
constexpr int k_enemy = 2;

class DismantleTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_state();
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.register_owner_with_id(k_owner, Game::Systems::OwnerType::Player, "Player");
    owners.register_owner_with_id(k_enemy, Game::Systems::OwnerType::AI, "Rome");
  }

  void TearDown() override { reset_shared_state(); }

  static void reset_shared_state() {
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto add_building(Engine::Core::World& world,
                           Game::Units::SpawnType type,
                           int owner_id = k_owner) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {10.0F, 0.0F, 0.0F};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = type;
    unit->owner_id = owner_id;
    unit->health = 500;
    unit->max_health = 500;
    entity->add_component<Engine::Core::BuildingComponent>();
    return entity;
  }

  static auto add_builder(Engine::Core::World& world,
                          float x) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, 0.0F};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->owner_id = k_owner;
    unit->health = 100;
    unit->max_health = 100;
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::BuilderProductionComponent>();
    return entity;
  }

  static void order_dismantle(Engine::Core::World& world,
                              const std::vector<Engine::Core::EntityID>& builders,
                              Engine::Core::EntityID structure,
                              int owner_id = k_owner) {
    Game::Command::Command command;
    command.owner_id = owner_id;
    command.payload =
        Game::Command::DismantleStructure{.units = builders, .structure = structure};
    Game::Command::dispatch(world, command);
  }

  static void put_crew_to_work(Engine::Core::Entity* worker) {
    auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
    builder->at_construction_site = true;
    builder->in_progress = true;
  }

  static void tick(Engine::Core::World& world, float seconds) {
    Game::Systems::DismantleSystem system;
    system.update(&world, seconds);
  }

  static auto
  site_of(Engine::Core::Entity* structure) -> Engine::Core::DismantleSiteComponent* {
    return structure->get_component<Engine::Core::DismantleSiteComponent>();
  }
};

TEST_F(DismantleTest, ABuildingComesDownAndPaysPartOfItsCostBack) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  ASSERT_NE(site_of(tower), nullptr);
  put_crew_to_work(worker);

  const auto expected = Game::Systems::dismantle_refund("defense_tower");
  ASSERT_GT(expected.get(Game::Systems::ResourceType::Wood), 0);

  tick(world, site_of(tower)->duration + 0.1F);

  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  EXPECT_EQ(resources.get(k_owner, Game::Systems::ResourceType::Wood),
            expected.get(Game::Systems::ResourceType::Wood));
  EXPECT_EQ(resources.get(k_owner, Game::Systems::ResourceType::Stone),
            expected.get(Game::Systems::ResourceType::Stone));

  const auto* unit = tower->get_component<Engine::Core::UnitComponent>();
  EXPECT_LE(unit->health, 0) << "the building should be dead once the work finishes";
}

TEST_F(DismantleTest, ARefundNeverExceedsWhatTheBuildingCost) {
  for (const auto* item : {"defense_tower",
                           "home",
                           "wall_segment",
                           "wall_gate",
                           "temple",
                           "marketplace"}) {
    const auto cost = Game::Systems::construction_cost_info(item).resource_costs;
    const auto refund = Game::Systems::dismantle_refund(item);
    for (const auto resource_type : Game::Systems::k_all_resource_types) {
      EXPECT_LE(refund.get(resource_type), cost.get(resource_type))
          << item << " pays back more than it cost";
      EXPECT_GE(refund.get(resource_type), 0) << item;
    }
  }
}

TEST_F(DismantleTest, CallingTheCrewOffLeavesTheBuildingStandingAndPaysNothing) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  put_crew_to_work(worker);
  tick(world, site_of(tower)->duration * 0.5F);
  ASSERT_NE(site_of(tower), nullptr);
  ASSERT_GT(site_of(tower)->progress, 0.0F);

  auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
  builder->in_progress = false;
  builder->structure_task_entity_id = 0;

  tick(world, 0.1F);

  EXPECT_EQ(site_of(tower), nullptr) << "the job should be off";
  const auto* unit = tower->get_component<Engine::Core::UnitComponent>();
  EXPECT_EQ(unit->health, unit->max_health) << "the building is untouched";
  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  EXPECT_EQ(resources.get(k_owner, Game::Systems::ResourceType::Wood), 0);
  EXPECT_EQ(resources.get(k_owner, Game::Systems::ResourceType::Stone), 0);
}

TEST_F(DismantleTest, AProtectedBuildingIsNeverOpenedUp) {
  ASSERT_FALSE(Game::Systems::dismantle_info("barracks").allowed);

  Engine::Core::World world;
  auto* barracks = add_building(world, Game::Units::SpawnType::Barracks);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, barracks->get_id());

  EXPECT_EQ(site_of(barracks), nullptr);
  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_NE(builder->product_type,
            std::string(Game::Systems::k_builder_product_dismantle));
}

TEST_F(DismantleTest, SomebodyElsesBuildingIsNotYoursToTakeDown) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower, k_enemy);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, tower->get_id());

  EXPECT_EQ(site_of(tower), nullptr);
}

TEST_F(DismantleTest, AThirdBuilderStillHelpsButAFourthDoesNot) {
  Engine::Core::World alone;
  auto* solo_tower = add_building(alone, Game::Units::SpawnType::DefenseTower);
  auto* solo = add_builder(alone, 8.0F);
  order_dismantle(alone, {solo->get_id()}, solo_tower->get_id());
  put_crew_to_work(solo);
  const float duration = site_of(solo_tower)->duration;
  tick(alone, duration * 0.25F);
  const float one_worker = site_of(solo_tower)->progress;

  Engine::Core::World crewed;
  auto* crew_tower = add_building(crewed, Game::Units::SpawnType::DefenseTower);
  std::vector<Engine::Core::EntityID> crew;
  std::vector<Engine::Core::Entity*> workers;
  for (int i = 0; i < 5; ++i) {
    auto* worker = add_builder(crewed, 8.0F + static_cast<float>(i));
    crew.push_back(worker->get_id());
    workers.push_back(worker);
  }
  order_dismantle(crewed, crew, crew_tower->get_id());
  for (auto* worker : workers) {
    put_crew_to_work(worker);
  }
  tick(crewed, duration * 0.25F);
  const float five_workers = site_of(crew_tower)->progress;

  EXPECT_GT(five_workers, one_worker) << "more hands should be faster";
  EXPECT_NEAR(five_workers,
              one_worker *
                  static_cast<float>(Game::Systems::DismantleSystem::k_max_crew),
              0.01F)
      << "past the crew limit the extra hands should not count";
}

TEST_F(DismantleTest, TheBuildingStopsWorkingOnceTheCrewStarts) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* production = tower->add_component<Engine::Core::ProductionComponent>();
  production->in_progress = true;
  production->time_remaining = 5.0F;
  auto* worker = add_builder(world, 8.0F);

  Game::Systems::ProductionSystem production_system;
  production_system.update(&world, 1.0F);
  const float before = production->time_remaining;
  EXPECT_LT(before, 5.0F) << "it should be recruiting before the order";

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  production_system.update(&world, 1.0F);

  EXPECT_FLOAT_EQ(production->time_remaining, before)
      << "a building being taken apart should not keep recruiting";
}

TEST_F(DismantleTest, TheWorkerReportsWhatItIsDoing) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  put_crew_to_work(worker);

  const auto activity = Game::Systems::classify_unit_activity(*worker);
  EXPECT_EQ(activity.kind, Game::Systems::ActivityKind::Dismantle);
  EXPECT_EQ(Game::Systems::activity_kind_id(activity.kind), "dismantle");
}

TEST_F(DismantleTest, ADeadCrewLeavesTheBuildingAlone) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  put_crew_to_work(worker);
  tick(world, site_of(tower)->duration * 0.4F);
  ASSERT_NE(site_of(tower), nullptr);

  worker->get_component<Engine::Core::UnitComponent>()->health = 0;
  tick(world, 0.1F);

  EXPECT_EQ(site_of(tower), nullptr);
  const auto* unit = tower->get_component<Engine::Core::UnitComponent>();
  EXPECT_EQ(unit->health, unit->max_health);
}

TEST_F(DismantleTest, HalfFinishedWorkSurvivesSaveAndLoad) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  put_crew_to_work(worker);
  tick(world, site_of(tower)->duration * 0.5F);
  const float progress = site_of(tower)->progress;
  const float duration = site_of(tower)->duration;
  ASSERT_GT(progress, 0.0F);
  ASSERT_LT(progress, 1.0F);

  const QJsonObject saved = Engine::Core::Serialization::serialize_entity(tower);

  Engine::Core::World restored_world;
  auto* restored = restored_world.create_entity();
  Engine::Core::Serialization::deserialize_entity(restored, saved);

  const auto* site = restored->get_component<Engine::Core::DismantleSiteComponent>();
  ASSERT_NE(site, nullptr) << "a job in progress must come back with the save";
  EXPECT_FLOAT_EQ(site->progress, progress);
  EXPECT_FLOAT_EQ(site->duration, duration);
}

TEST_F(DismantleTest, AFinishedBuildingLetsGoOfCollisionAndItsCrew) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  auto& collision = Game::Systems::BuildingCollisionRegistry::instance();
  collision.register_building(tower->get_id(), "defense_tower", 10.0F, 0.0F, k_owner);
  ASSERT_NE(collision.find_building(tower->get_id()), nullptr);

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  put_crew_to_work(worker);
  tick(world, site_of(tower)->duration + 0.1F);

  EXPECT_EQ(collision.find_building(tower->get_id()), nullptr)
      << "the footprint must not outlive the building";
  EXPECT_TRUE(tower->has_component<Engine::Core::PendingRemovalComponent>())
      << "the building should go out through the normal removal path";

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_FALSE(builder->in_progress);
  EXPECT_EQ(builder->structure_task_entity_id, 0U);
}

TEST_F(DismantleTest, TheGroundUnderAFinishedBuildingOpensBackUp) {
  Game::Systems::NavGrid::initialize(48, 48);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);

  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  auto& collision = Game::Systems::BuildingCollisionRegistry::instance();
  collision.register_building(tower->get_id(), "defense_tower", 10.0F, 0.0F, k_owner);
  pathfinder->update_navigation_grid();

  const auto footprint = Game::Systems::NavGrid::world_to_grid(10.0F, 0.0F);
  ASSERT_FALSE(pathfinder->is_walkable(footprint.x, footprint.y))
      << "the tower should be blocking its own tile to begin with";

  order_dismantle(world, {worker->get_id()}, tower->get_id());
  put_crew_to_work(worker);
  tick(world, site_of(tower)->duration + 0.1F);

  pathfinder->update_navigation_grid();
  EXPECT_TRUE(pathfinder->is_walkable(footprint.x, footprint.y))
      << "troops should be able to walk over the cleared ground";
}

TEST_F(DismantleTest, TheComputerCannotOrderADismantleByItself) {
  Engine::Core::World world;
  auto* tower = add_building(world, Game::Units::SpawnType::DefenseTower);
  auto* worker = add_builder(world, 8.0F);

  Game::Command::Command from_ai;
  from_ai.source = Game::Command::Source::AI;
  from_ai.owner_id = k_owner;
  from_ai.payload = Game::Command::DismantleStructure{.units = {worker->get_id()},
                                                      .structure = tower->get_id()};

  const auto ruling = Game::Command::validate(world, from_ai);
  EXPECT_FALSE(ruling.accepted());
  EXPECT_EQ(ruling.rejection, Game::Command::Rejection::NotPermittedForSource);

  Game::Command::Command from_player = from_ai;
  from_player.source = Game::Command::Source::LocalPlayer;
  EXPECT_TRUE(Game::Command::validate(world, from_player).accepted());
}

} // namespace
