#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "game/command/command_dispatcher.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/farm_system.h"
#include "game/systems/food_targets.h"
#include "game/systems/gather_loop_system.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/interaction_targeting.h"
#include "game/systems/movement_pipeline.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/resource_delivery_system.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/unit_activity.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog_loader.h"
#include "game/wildlife/wildlife_species.h"

namespace {

using Game::Systems::InteractionAction;

constexpr int k_owner = 1;

class FoodEconomyTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_state();
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.register_owner_with_id(k_owner, Game::Systems::OwnerType::Player, "Player");

    Game::Map::MapDefinition map_def;
    map_def.grid.width = 24;
    map_def.grid.height = 24;
    map_def.grid.tile_size = 1.0F;
    map_def.biome.procedural_trees_enabled = false;
    map_def.biome.procedural_boulders_enabled = false;
    map_def.biome.procedural_iron_ore_enabled = false;
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Map::VisibilityService::instance().initialize(24, 24, 1.0F);
    Game::Map::VisibilityService::instance().reveal_all();
    Game::Systems::NavGrid::initialize(24, 24);
    if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
      pathfinder->update_navigation_grid();
    }

    auto registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
    Game::Map::MapTransformer::setFactoryRegistry(std::move(registry));
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    reset_shared_state();
  }

  static void reset_shared_state() {
    Game::Map::TerrainService::instance().clear();
    Game::Map::VisibilityService::instance().reset();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
  }

  static auto add_farm(Engine::Core::World& world,
                       float x,
                       float z,
                       float growth,
                       int owner = k_owner) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Farm;
    unit->owner_id = owner;
    unit->health = 600;
    unit->max_health = 600;
    entity->add_component<Engine::Core::BuildingComponent>();
    auto* farm = entity->add_component<Engine::Core::FarmComponent>();
    farm->growth = growth;
    farm->cycle_seconds = 60.0F;
    return entity;
  }

  static auto
  add_sheep(Engine::Core::World& world, float x, float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Sheep;
    unit->owner_id = 0;
    unit->health = 60;
    unit->max_health = 60;
    entity->add_component<Engine::Core::MovementComponent>();
    auto* wildlife = entity->add_component<Engine::Core::WildlifeComponent>();
    wildlife->species = Game::Wildlife::Species::Sheep;
    return entity;
  }

  static auto
  add_builder(Engine::Core::World& world, float x, float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    entity->add_component<Engine::Core::MovementComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->owner_id = k_owner;
    unit->health = 100;
    unit->max_health = 100;
    entity->add_component<Engine::Core::BuilderProductionComponent>();
    return entity;
  }

  static void put_to_work(Engine::Core::Entity& worker,
                          Engine::Core::Entity& target,
                          std::string_view product) {
    auto* builder = worker.get_component<Engine::Core::BuilderProductionComponent>();
    auto* transform = worker.get_component<Engine::Core::TransformComponent>();
    auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
    builder->product_type = std::string(product);
    builder->structure_task_entity_id = target.get_id();
    builder->has_construction_site = true;
    builder->construction_site_x = transform->position.x;
    builder->construction_site_z = transform->position.z;
    builder->at_construction_site = true;
    builder->in_progress = true;
    builder->build_time = 4.0F;
    builder->time_remaining = 0.0F;
    builder->task_target_x = target_transform->position.x;
    builder->task_target_z = target_transform->position.z;
  }
};

TEST_F(FoodEconomyTest, AFarmRipensOverItsCycleAndThenWaits) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 6.0F, 6.0F, 0.0F);
  auto* farm = farm_entity->get_component<Engine::Core::FarmComponent>();

  Game::Systems::FarmSystem system;
  system.update(&world, 30.0F);
  EXPECT_NEAR(farm->growth, 0.5F, 0.001F);
  EXPECT_FALSE(farm->ripe());
  EXPECT_EQ(farm->growth_stage(), 2);

  system.update(&world, 40.0F);
  EXPECT_TRUE(farm->ripe());
  EXPECT_EQ(farm->growth_stage(), Engine::Core::k_farm_growth_stage_count - 1);

  system.update(&world, 40.0F);
  EXPECT_FLOAT_EQ(farm->growth, 1.0F) << "a ripe field holds until it is reaped";
}

TEST_F(FoodEconomyTest, ANeutralOrDeadFarmGrowsNothing) {
  Engine::Core::World world;
  auto* neutral = add_farm(world, 6.0F, 6.0F, 0.0F, Game::Core::NEUTRAL_OWNER_ID);
  auto* burnt = add_farm(world, 12.0F, 6.0F, 0.2F);
  burnt->get_component<Engine::Core::UnitComponent>()->health = 0;

  Game::Systems::FarmSystem system;
  system.update(&world, 30.0F);
  EXPECT_FLOAT_EQ(neutral->get_component<Engine::Core::FarmComponent>()->growth, 0.0F);
  EXPECT_FLOAT_EQ(burnt->get_component<Engine::Core::FarmComponent>()->growth, 0.2F);
}

TEST_F(FoodEconomyTest, ReapingARipeFarmLoadsFoodAndSowsTheFieldAgain) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 8.0F, 8.0F, 1.0F);
  auto* worker = add_builder(world, 5.0F, 8.0F);
  put_to_work(*worker, *farm_entity, Game::Systems::k_builder_product_harvest_grain);

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  const auto* carry = worker->get_component<Engine::Core::ResourceCarryComponent>();
  ASSERT_NE(carry, nullptr);
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Food),
            Game::Systems::k_harvest_grain_food_reward);
  EXPECT_EQ(carry->food_form, Engine::Core::CarriedFoodForm::Grain)
      << "a reaped field is carried home as a sheaf";

  const auto* farm = farm_entity->get_component<Engine::Core::FarmComponent>();
  EXPECT_FLOAT_EQ(farm->growth, 0.0F) << "the reaped field starts its next cycle";
  EXPECT_EQ(farm->harvests, 1);

  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_FALSE(builder->in_progress);
  EXPECT_EQ(builder->structure_task_entity_id, 0U);
  EXPECT_TRUE(builder->has_gather_order) << "reaping starts a standing farm round";
  EXPECT_EQ(builder->gather_product_type,
            Game::Systems::k_builder_product_harvest_grain);
  EXPECT_FLOAT_EQ(builder->gather_anchor_x, 8.0F);

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                k_owner, Game::Systems::ResourceType::Food),
            Game::Systems::k_harvest_grain_food_reward)
      << "with no barracks anywhere the load is credited where the worker stands";
}

TEST_F(FoodEconomyTest, AFieldSomebodyElseReapedFirstIsALostTarget) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 8.0F, 8.0F, 0.4F);
  auto* worker = add_builder(world, 5.0F, 8.0F);
  put_to_work(*worker, *farm_entity, Game::Systems::k_builder_product_harvest_grain);

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_FALSE(worker->has_component<Engine::Core::ResourceCarryComponent>());
  const auto* builder =
      worker->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_EQ(builder->fault, Engine::Core::BuilderTaskFault::TargetLost);
  EXPECT_FALSE(builder->in_progress);
  EXPECT_EQ(builder->structure_task_entity_id, 0U);
}

TEST_F(FoodEconomyTest, SlaughteringASheepKillsItAndLoadsFood) {
  Engine::Core::World world;
  auto* sheep = add_sheep(world, 8.0F, 8.0F);
  auto* worker = add_builder(world, 7.2F, 8.0F);
  put_to_work(*worker, *sheep, Game::Systems::k_builder_product_slaughter_sheep);

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  const auto* carry = worker->get_component<Engine::Core::ResourceCarryComponent>();
  ASSERT_NE(carry, nullptr);
  EXPECT_EQ(carry->amounts.get(Game::Systems::ResourceType::Food),
            Game::Systems::k_slaughter_sheep_food_reward);

  EXPECT_EQ(carry->food_form, Engine::Core::CarriedFoodForm::Meat)
      << "a butchered sheep is carried home as meat, not as a sheaf of grain";

  EXPECT_EQ(sheep->get_component<Engine::Core::UnitComponent>()->health, 0);
  const auto* death = sheep->get_component<Engine::Core::DeathAnimationComponent>();
  ASSERT_NE(death, nullptr) << "the carcass plays the animal death sequence";
  EXPECT_EQ(death->profile, Engine::Core::DeathSequenceProfile::Horse);
  EXPECT_FALSE(Game::Systems::sheep_is_slaughterable(*sheep));
}

TEST_F(FoodEconomyTest, AButcheredSheepIsDeliveredToTheStockpileAsFood) {
  Engine::Core::World world;
  auto* sheep = add_sheep(world, 8.0F, 8.0F);
  auto* worker = add_builder(world, 7.2F, 8.0F);
  put_to_work(*worker, *sheep, Game::Systems::k_builder_product_slaughter_sheep);

  Game::Systems::ProductionSystem production;
  production.update(&world, 0.1F);

  Game::Systems::ResourceDeliverySystem delivery;
  delivery.update(&world, 0.1F);

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                k_owner, Game::Systems::ResourceType::Food),
            Game::Systems::k_slaughter_sheep_food_reward);
  EXPECT_EQ(worker->get_component<Engine::Core::ResourceCarryComponent>(), nullptr)
      << "the hauler puts the carcass down once it is credited";
}

TEST_F(FoodEconomyTest, ASheepBeingButcheredIsHeldStill) {
  Engine::Core::World world;
  auto* sheep = add_sheep(world, 8.0F, 8.0F);
  auto* worker = add_builder(world, 7.2F, 8.0F);
  put_to_work(*worker, *sheep, Game::Systems::k_builder_product_slaughter_sheep);
  worker->get_component<Engine::Core::BuilderProductionComponent>()->time_remaining =
      3.0F;

  Game::Systems::ProductionSystem system;
  system.update(&world, 0.1F);

  EXPECT_GT(sheep->get_component<Engine::Core::WildlifeComponent>()->held_timer, 0.0F);
  EXPECT_GT(sheep->get_component<Engine::Core::UnitComponent>()->health, 0);
}

TEST_F(FoodEconomyTest, TheStandingFarmRoundWaitsForTheNextCropInsteadOfRetiring) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 8.0F, 8.0F, 0.3F);
  auto* worker = add_builder(world, 5.0F, 8.0F);
  auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
  builder->has_gather_order = true;
  builder->gather_product_type =
      std::string(Game::Systems::k_builder_product_harvest_grain);
  builder->gather_anchor_x = 8.0F;
  builder->gather_anchor_z = 8.0F;

  Game::Systems::GatherLoopSystem loop;
  loop.update(&world, 1.0F);
  EXPECT_TRUE(builder->has_gather_order) << "the crop is still growing";
  EXPECT_EQ(builder->structure_task_entity_id, 0U);

  farm_entity->get_component<Engine::Core::FarmComponent>()->growth = 1.0F;
  loop.update(&world, 1.0F);
  EXPECT_EQ(builder->structure_task_entity_id, farm_entity->get_id());
  EXPECT_EQ(builder->product_type, Game::Systems::k_builder_product_harvest_grain);
  EXPECT_TRUE(builder->has_construction_site);
}

TEST_F(FoodEconomyTest, TwoWorkersNeverClaimTheSameRipeFarm) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 8.0F, 8.0F, 1.0F);
  auto* first = add_builder(world, 5.0F, 8.0F);
  auto* second = add_builder(world, 5.0F, 9.0F);
  for (auto* worker : {first, second}) {
    auto* builder = worker->get_component<Engine::Core::BuilderProductionComponent>();
    builder->has_gather_order = true;
    builder->gather_product_type =
        std::string(Game::Systems::k_builder_product_harvest_grain);
    builder->gather_anchor_x = 8.0F;
    builder->gather_anchor_z = 8.0F;
  }

  Game::Systems::GatherLoopSystem loop;
  loop.update(&world, 1.0F);
  int claims = 0;
  for (auto* worker : {first, second}) {
    if (worker->get_component<Engine::Core::BuilderProductionComponent>()
            ->structure_task_entity_id == farm_entity->get_id()) {
      ++claims;
    }
  }
  EXPECT_EQ(claims, 1);
  EXPECT_TRUE(Game::Systems::food_target_claimed(world, farm_entity->get_id()));
}

TEST_F(FoodEconomyTest, TheStartHarvestOrderSendsABuilderToAFarmOrASheep) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 12.0F, 12.0F, 1.0F);
  auto* sheep = add_sheep(world, 4.0F, 12.0F);
  auto* reaper = add_builder(world, 12.0F, 6.0F);
  auto* butcher = add_builder(world, 4.0F, 6.0F);

  Game::Command::dispatch(
      world,
      Game::Command::Command{.source = Game::Command::Source::LocalPlayer,
                             .owner_id = k_owner,
                             .payload = Game::Command::StartHarvest{
                                 .units = {reaper->get_id()},
                                 .construction_type = std::string(
                                     Game::Systems::k_builder_product_harvest_grain),
                                 .resource_target = farm_entity->get_id(),
                                 .site = QVector3D(12.0F, 0.0F, 12.0F)}});
  Game::Command::dispatch(
      world,
      Game::Command::Command{.source = Game::Command::Source::LocalPlayer,
                             .owner_id = k_owner,
                             .payload = Game::Command::StartHarvest{
                                 .units = {butcher->get_id()},
                                 .construction_type = std::string(
                                     Game::Systems::k_builder_product_slaughter_sheep),
                                 .resource_target = sheep->get_id(),
                                 .site = QVector3D(4.0F, 0.0F, 12.0F)}});

  const auto* reaper_builder =
      reaper->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_EQ(reaper_builder->product_type,
            Game::Systems::k_builder_product_harvest_grain);
  EXPECT_EQ(reaper_builder->structure_task_entity_id, farm_entity->get_id());
  EXPECT_TRUE(reaper_builder->has_construction_site);

  EXPECT_GT(std::hypot(reaper_builder->construction_site_x - 12.0F,
                       reaper_builder->construction_site_z - 12.0F),
            1.0F);
  EXPECT_EQ(Game::Systems::activity_for_builder_product(reaper_builder->product_type),
            Game::Systems::ActivityKind::HarvestGrain);

  const auto* butcher_builder =
      butcher->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_EQ(butcher_builder->product_type,
            Game::Systems::k_builder_product_slaughter_sheep);
  EXPECT_EQ(butcher_builder->structure_task_entity_id, sheep->get_id());
  EXPECT_EQ(Game::Systems::activity_for_builder_product(butcher_builder->product_type),
            Game::Systems::ActivityKind::SlaughterSheep);

  EXPECT_NEAR(butcher_builder->construction_site_x, 4.0F, 0.0001F);
  EXPECT_NEAR(butcher_builder->construction_site_z, 12.0F, 0.0001F);
}

TEST_F(FoodEconomyTest, TheButchersWalkOntoTheSheepTheyTake) {

  Engine::Core::World world;
  auto* sheep = add_sheep(world, 12.0F, 12.0F);
  auto* butcher = add_builder(world, 4.0F, 12.0F);
  auto* transform = butcher->get_component<Engine::Core::TransformComponent>();
  butcher->get_component<Engine::Core::UnitComponent>()->speed = 2.0F;

  Game::Command::dispatch(
      world,
      Game::Command::Command{.source = Game::Command::Source::LocalPlayer,
                             .owner_id = k_owner,
                             .payload = Game::Command::StartHarvest{
                                 .units = {butcher->get_id()},
                                 .construction_type = std::string(
                                     Game::Systems::k_builder_product_slaughter_sheep),
                                 .resource_target = sheep->get_id(),
                                 .site = QVector3D(12.0F, 0.0F, 12.0F)}});

  auto* builder = butcher->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder, nullptr);
  ASSERT_TRUE(builder->has_construction_site);
  builder->build_time = 1000.0F;
  builder->time_remaining = builder->build_time;

  Game::Systems::MovementPipeline movement;
  Game::Systems::ProductionSystem production;
  bool arrived = false;
  for (int step = 0; step < 600 && !arrived; ++step) {
    movement.update(&world, 0.05F);
    production.update(&world, 0.05F);
    arrived = builder->at_construction_site;
  }

  ASSERT_TRUE(arrived) << "the butchers never closed on the animal";

  const auto* grazing = sheep->get_component<Engine::Core::TransformComponent>();
  EXPECT_NEAR(transform->position.x, grazing->position.x, 0.15F);
  EXPECT_NEAR(transform->position.z, grazing->position.z, 0.15F);
}

TEST_F(FoodEconomyTest, AnUnripeFarmRefusesTheHarvestOrder) {
  Engine::Core::World world;
  auto* farm_entity = add_farm(world, 12.0F, 12.0F, 0.5F);
  auto* reaper = add_builder(world, 12.0F, 6.0F);

  Game::Command::dispatch(
      world,
      Game::Command::Command{.source = Game::Command::Source::LocalPlayer,
                             .owner_id = k_owner,
                             .payload = Game::Command::StartHarvest{
                                 .units = {reaper->get_id()},
                                 .construction_type = std::string(
                                     Game::Systems::k_builder_product_harvest_grain),
                                 .resource_target = farm_entity->get_id(),
                                 .site = QVector3D(12.0F, 0.0F, 12.0F)}});

  const auto* builder =
      reaper->get_component<Engine::Core::BuilderProductionComponent>();
  EXPECT_TRUE(builder->product_type.empty());
  EXPECT_EQ(builder->structure_task_entity_id, 0U);
}

TEST_F(FoodEconomyTest, BuildersAreOfferedRipeFarmsAndSheepButNotGreenFields) {
  Engine::Core::World world;
  add_farm(world, 6.0F, 6.0F, 1.0F);
  add_farm(world, 16.0F, 6.0F, 0.5F);
  add_sheep(world, 6.0F, 16.0F);

  Game::Systems::InteractionTargetingRequest request;
  request.world = &world;
  request.local_owner_id = k_owner;
  request.has_builders = true;
  request.anchor_x = 12.0F;
  request.anchor_z = 12.0F;
  request.max_distance = Game::Systems::k_interaction_highlight_max_distance;
  request.max_markers = Game::Systems::k_interaction_highlight_max_markers;

  const auto highlights = Game::Systems::collect_interaction_target_highlights(request);
  int harvest = 0;
  int slaughter = 0;
  for (const auto& marker : highlights.markers) {
    harvest += marker.action == InteractionAction::Harvest ? 1 : 0;
    slaughter += marker.action == InteractionAction::Slaughter ? 1 : 0;
  }
  EXPECT_EQ(harvest, 1);
  EXPECT_EQ(slaughter, 1);
  EXPECT_EQ(Game::Systems::interaction_action_key(InteractionAction::Harvest),
            "harvest");
  EXPECT_EQ(Game::Systems::interaction_action_key(InteractionAction::Slaughter),
            "slaughter");
}

TEST_F(FoodEconomyTest, RecruitingACivilianAtAHomeSpendsFood) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());
  Game::Systems::TroopProfileService::instance().clear();

  Engine::Core::World world;
  auto* home = world.create_entity();
  auto* unit = home->add_component<Engine::Core::UnitComponent>();
  auto* production = home->add_component<Engine::Core::ProductionComponent>();
  unit->spawn_type = Game::Units::SpawnType::Home;
  unit->owner_id = k_owner;
  production->max_units = 3;
  production->manpower_available = 24;

  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  const auto civilian_food =
      Game::Systems::TroopProfileService::instance()
          .get_profile(Game::Systems::NationID::RomanRepublic,
                       Game::Units::TroopType::Civilian)
          .production.resource_costs.get(Game::Systems::ResourceType::Food);
  ASSERT_GT(civilian_food, 0) << "the civilian must carry a food price";

  auto result = Game::Systems::ProductionService::start_production(
      world, home->get_id(), Game::Units::TroopType::Civilian);
  EXPECT_EQ(result, Game::Systems::ProductionResult::InsufficientResources);
  EXPECT_FALSE(production->in_progress);

  resources.set(k_owner, Game::Systems::ResourceType::Food, civilian_food);
  result = Game::Systems::ProductionService::start_production(
      world, home->get_id(), Game::Units::TroopType::Civilian);
  EXPECT_EQ(result, Game::Systems::ProductionResult::Success);
  EXPECT_TRUE(production->in_progress);
  EXPECT_EQ(resources.get(k_owner, Game::Systems::ResourceType::Food), 0);
}

TEST_F(FoodEconomyTest, FoodIsAGatherableResourceWithItsOwnYields) {
  EXPECT_TRUE(Game::Systems::is_gatherable_resource(Game::Systems::ResourceType::Food));
  EXPECT_EQ(Game::Systems::harvest_yield(Game::Systems::ResourceType::Food),
            Game::Systems::k_harvest_grain_food_reward);
  EXPECT_EQ(Game::Systems::resource_for_harvest_product(
                Game::Systems::k_builder_product_slaughter_sheep),
            Game::Systems::ResourceType::Food);
  EXPECT_TRUE(Game::Systems::is_gather_builder_product(
      Game::Systems::k_builder_product_harvest_grain));
  EXPECT_FALSE(Game::Systems::is_harvest_builder_product(
      Game::Systems::k_builder_product_harvest_grain))
      << "food jobs target entities, not world props, and must not be reserved through "
         "the terrain service";
}

} // namespace
