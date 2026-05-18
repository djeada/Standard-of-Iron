#include <QVector3D>

#include <gtest/gtest.h>
#include <vector>

#include "app/utils/movement_utils.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/command_service.h"
#include "game/systems/production_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace {

TEST(CivilianDeliverySystemTest, HomeRecruitsCivilianUsingHomeManpowerPool) {
  Engine::Core::World world;

  auto* home = world.create_entity();
  auto* home_unit = home->add_component<Engine::Core::UnitComponent>();
  auto* home_prod = home->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(home_unit, nullptr);
  ASSERT_NE(home_prod, nullptr);

  home_unit->spawn_type = Game::Units::SpawnType::Home;
  home_unit->owner_id = 1;
  home_prod->product_type = Game::Units::TroopType::Civilian;
  home_prod->build_time = 5.0F;
  home_prod->max_units = 3;
  home_prod->manpower_available = 8;
  home_prod->villager_cost = 8;

  const std::vector<Engine::Core::EntityID> selected = {home->get_id()};
  auto result =
      Game::Systems::ProductionService::start_production_for_first_selected_home(
          world, selected, 1, Game::Units::TroopType::Civilian);

  EXPECT_EQ(result, Game::Systems::ProductionResult::Success);
  EXPECT_TRUE(home_prod->in_progress);
  EXPECT_EQ(home_prod->manpower_available, 0);
}

TEST(CivilianDeliverySystemTest, HomeCannotRecruitMoreThanThreeCivilians) {
  Engine::Core::World world;

  auto* home = world.create_entity();
  auto* home_unit = home->add_component<Engine::Core::UnitComponent>();
  auto* home_prod = home->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(home_unit, nullptr);
  ASSERT_NE(home_prod, nullptr);

  home_unit->spawn_type = Game::Units::SpawnType::Home;
  home_unit->owner_id = 1;
  home_prod->product_type = Game::Units::TroopType::Civilian;
  home_prod->build_time = 5.0F;
  home_prod->max_units = 3;
  home_prod->produced_count = 2;
  home_prod->in_progress = true;
  home_prod->manpower_available = 8;
  home_prod->villager_cost = 8;

  const std::vector<Engine::Core::EntityID> selected = {home->get_id()};
  auto result =
      Game::Systems::ProductionService::start_production_for_first_selected_home(
          world, selected, 1, Game::Units::TroopType::Civilian);

  EXPECT_EQ(result, Game::Systems::ProductionResult::PerBarracksLimitReached);
  EXPECT_TRUE(home_prod->in_progress);
  EXPECT_TRUE(home_prod->production_queue.empty());
  EXPECT_EQ(home_prod->manpower_available, 8);
}

TEST(CivilianDeliverySystemTest, CivilianEnteringBarracksTransfersManpower) {
  Engine::Core::World world;

  auto* barracks = world.create_entity();
  auto* barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* barracks_prod = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_prod, nullptr);

  barracks_transform->position = {20.0F, 0.0F, 20.0F};
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  barracks_prod->manpower_available = 0;

  auto* civilian = world.create_entity();
  auto* civilian_transform =
      civilian->add_component<Engine::Core::TransformComponent>();
  auto* civilian_unit = civilian->add_component<Engine::Core::UnitComponent>();
  auto* delivery = civilian->add_component<Engine::Core::CivilianDeliveryComponent>();
  ASSERT_NE(civilian_transform, nullptr);
  ASSERT_NE(civilian_unit, nullptr);
  ASSERT_NE(delivery, nullptr);

  civilian_transform->position = {20.5F, 0.0F, 20.2F};
  civilian_unit->spawn_type = Game::Units::SpawnType::Civilian;
  civilian_unit->owner_id = 1;
  delivery->target_barracks_id = barracks->get_id();

  Game::Systems::CivilianDeliverySystem system;
  system.update(&world, 0.016F);

  EXPECT_EQ(world.get_entity(civilian->get_id()), nullptr);
  EXPECT_EQ(barracks_prod->manpower_available,
            Game::Systems::k_civilian_delivery_population_grant);
}

TEST(CivilianDeliverySystemTest, DeliveryTargetOutsideBarracksStillTransfersManpower) {
  Game::Systems::BuildingCollisionRegistry::instance().clear();
  Game::Systems::CommandService::initialize(64, 64);
  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);

  Engine::Core::World world;

  auto* barracks = world.create_entity();
  auto* barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* barracks_prod = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_prod, nullptr);

  barracks_transform->position = {20.0F, 0.0F, 20.0F};
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  barracks_prod->manpower_available = 0;

  auto& collision_registry = Game::Systems::BuildingCollisionRegistry::instance();
  collision_registry.register_building(barracks->get_id(),
                                       "barracks",
                                       barracks_transform->position.x,
                                       barracks_transform->position.z,
                                       barracks_unit->owner_id);
  pathfinder->update_building_obstacles();

  float const civilian_radius = 0.5F;
  QVector3D const delivery_target = App::Utils::barracks_delivery_target_position(
      QVector3D(12.0F, 0.0F, 20.0F),
      QVector3D(barracks_transform->position.x, 0.0F, barracks_transform->position.z),
      civilian_radius);

  EXPECT_FALSE(collision_registry.is_circle_overlapping_building(
      delivery_target.x(), delivery_target.z(), civilian_radius));

  auto* civilian = world.create_entity();
  auto* civilian_transform =
      civilian->add_component<Engine::Core::TransformComponent>();
  auto* civilian_unit = civilian->add_component<Engine::Core::UnitComponent>();
  auto* delivery = civilian->add_component<Engine::Core::CivilianDeliveryComponent>();
  ASSERT_NE(civilian_transform, nullptr);
  ASSERT_NE(civilian_unit, nullptr);
  ASSERT_NE(delivery, nullptr);

  civilian_transform->position = {delivery_target.x(), 0.0F, delivery_target.z()};
  civilian_unit->spawn_type = Game::Units::SpawnType::Civilian;
  civilian_unit->owner_id = 1;
  delivery->target_barracks_id = barracks->get_id();

  Game::Systems::CivilianDeliverySystem system;
  system.update(&world, 0.016F);

  EXPECT_EQ(world.get_entity(civilian->get_id()), nullptr);
  EXPECT_EQ(barracks_prod->manpower_available,
            Game::Systems::k_civilian_delivery_population_grant);

  collision_registry.clear();
}

TEST(CivilianDeliverySystemTest,
     DiagonalDeliveryTargetOutsideBarracksStillTransfersManpower) {
  Game::Systems::BuildingCollisionRegistry::instance().clear();
  Game::Systems::CommandService::initialize(64, 64);
  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);

  Engine::Core::World world;

  auto* barracks = world.create_entity();
  auto* barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* barracks_prod = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_prod, nullptr);

  barracks_transform->position = {20.0F, 0.0F, 20.0F};
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  barracks_prod->manpower_available = 0;

  auto& collision_registry = Game::Systems::BuildingCollisionRegistry::instance();
  collision_registry.register_building(barracks->get_id(),
                                       "barracks",
                                       barracks_transform->position.x,
                                       barracks_transform->position.z,
                                       barracks_unit->owner_id);
  pathfinder->update_building_obstacles();

  float const civilian_radius = 0.5F;
  QVector3D const delivery_target = App::Utils::barracks_delivery_target_position(
      QVector3D(12.0F, 0.0F, 12.0F),
      QVector3D(barracks_transform->position.x, 0.0F, barracks_transform->position.z),
      civilian_radius);

  EXPECT_FALSE(collision_registry.is_circle_overlapping_building(
      delivery_target.x(), delivery_target.z(), civilian_radius));

  auto* civilian = world.create_entity();
  auto* civilian_transform =
      civilian->add_component<Engine::Core::TransformComponent>();
  auto* civilian_unit = civilian->add_component<Engine::Core::UnitComponent>();
  auto* delivery = civilian->add_component<Engine::Core::CivilianDeliveryComponent>();
  ASSERT_NE(civilian_transform, nullptr);
  ASSERT_NE(civilian_unit, nullptr);
  ASSERT_NE(delivery, nullptr);

  civilian_transform->position = {delivery_target.x(), 0.0F, delivery_target.z()};
  civilian_unit->spawn_type = Game::Units::SpawnType::Civilian;
  civilian_unit->owner_id = 1;
  delivery->target_barracks_id = barracks->get_id();

  Game::Systems::CivilianDeliverySystem system;
  system.update(&world, 0.016F);

  EXPECT_EQ(world.get_entity(civilian->get_id()), nullptr);
  EXPECT_EQ(barracks_prod->manpower_available,
            Game::Systems::k_civilian_delivery_population_grant);

  collision_registry.clear();
}

TEST(CivilianDeliverySystemTest, FullBarracksRejectsCivilianWithoutConsumingIt) {
  Engine::Core::World world;

  auto* barracks = world.create_entity();
  auto* barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* barracks_prod = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_prod, nullptr);

  barracks_transform->position = {20.0F, 0.0F, 20.0F};
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  barracks_prod->max_units = 100;
  barracks_prod->manpower_available = 100;

  auto* civilian = world.create_entity();
  auto const civilian_id = civilian->get_id();
  auto* civilian_transform =
      civilian->add_component<Engine::Core::TransformComponent>();
  auto* civilian_unit = civilian->add_component<Engine::Core::UnitComponent>();
  auto* delivery = civilian->add_component<Engine::Core::CivilianDeliveryComponent>();
  ASSERT_NE(civilian_transform, nullptr);
  ASSERT_NE(civilian_unit, nullptr);
  ASSERT_NE(delivery, nullptr);

  civilian_transform->position = {20.5F, 0.0F, 20.2F};
  civilian_unit->spawn_type = Game::Units::SpawnType::Civilian;
  civilian_unit->owner_id = 1;
  delivery->target_barracks_id = barracks->get_id();

  Game::Systems::CivilianDeliverySystem system;
  system.update(&world, 0.016F);

  auto* remaining_civilian = world.get_entity(civilian_id);
  ASSERT_NE(remaining_civilian, nullptr);
  EXPECT_EQ(remaining_civilian
                ->get_component<Engine::Core::CivilianDeliveryComponent>(),
            nullptr);
  EXPECT_EQ(barracks_prod->manpower_available, 100);
}

} // namespace
