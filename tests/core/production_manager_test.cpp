#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "app/core/input_command_handler.h"
#define private public
#include "app/core/production_manager.h"
#undef private
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/picking_service.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/selection_system.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "render/gl/camera.h"

namespace {

class ProductionManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    auto& resources = Game::Systems::PlayerResourceRegistry::instance();
    resources.set(1, Game::Systems::ResourceType::Wood, 1000);
    resources.set(1, Game::Systems::ResourceType::Stone, 1000);
    resources.set(1, Game::Systems::ResourceType::Iron, 1000);
    Game::Systems::CommandService::initialize(32, 32);

    auto registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
    Game::Map::MapTransformer::setFactoryRegistry(std::move(registry));

    world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
    selection = world.get_system<Game::Systems::SelectionSystem>();
    ASSERT_NE(selection, nullptr);

    camera.set_perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
    camera.look_at(QVector3D(0.0F, 10.0F, 10.0F),
                   QVector3D(0.0F, 0.0F, 0.0F),
                   QVector3D(0.0F, 1.0F, 0.0F));
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::TerrainService::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  auto add_selected_builder(float x = 4.0F, float z = 6.0F) -> Engine::Core::Entity* {
    auto* builder = world.create_entity();
    builder->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit =
        builder->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 10.0F);
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    builder->add_component<Engine::Core::BuilderProductionComponent>();
    selection->select_unit(builder->get_id());
    return builder;
  }

  auto add_selected_production_building(Game::Units::SpawnType spawn_type,
                                        float x,
                                        float z,
                                        int owner_id = 1) -> Engine::Core::Entity* {
    auto* building = world.create_entity();
    if (building == nullptr) {
      return nullptr;
    }
    auto* transform =
        building->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit =
        building->add_component<Engine::Core::UnitComponent>(500, 500, 0.0F, 0.0F);
    auto* production = building->add_component<Engine::Core::ProductionComponent>();
    if (transform == nullptr || unit == nullptr || production == nullptr) {
      return nullptr;
    }
    unit->owner_id = owner_id;
    unit->spawn_type = spawn_type;
    selection->select_unit(building->get_id());
    return building;
  }

  auto world_to_screen(const QVector3D& world_pos) -> QPointF {
    QPointF screen_pos;
    EXPECT_TRUE(
        camera.world_to_screen(world_pos, viewport.width, viewport.height, screen_pos));
    return screen_pos;
  }

  auto preview_entities() -> std::vector<Engine::Core::Entity*> {
    return world.get_entities_with<Engine::Core::ConstructionPreviewComponent>();
  }

  auto find_spawned_unit(Game::Units::SpawnType spawn_type) -> Engine::Core::Entity* {
    for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->spawn_type == spawn_type) {
        return entity;
      }
    }
    return nullptr;
  }

  auto find_wall_construction_site() -> Engine::Core::Entity* {
    auto sites = world.get_entities_with<Engine::Core::WallConstructionSiteComponent>();
    return sites.empty() ? nullptr : sites.front();
  }

  void initialize_collect_map(Game::Map::WorldProp::Type prop_type) {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = 32;
    map_def.grid.height = 32;
    map_def.grid.tile_size = 1.0F;
    map_def.world_props.push_back({.type = prop_type, .x = 16.0F, .z = 16.0F});
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);
  }

  void initialize_collect_hill_map(Game::Map::WorldProp::Type prop_type) {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = 32;
    map_def.grid.height = 32;
    map_def.grid.tile_size = 1.0F;
    Game::Map::apply_ground_type_defaults(map_def.biome,
                                          Game::Map::GroundType::SoilRocky);
    map_def.biome.height_noise_amplitude = 0.0F;
    map_def.biome.ground_irregularity_enabled = false;
    map_def.terrain.push_back({.type = Game::Map::TerrainType::Hill,
                               .center_x = 0.5F,
                               .center_z = 0.5F,
                               .width = 9.0F,
                               .depth = 9.0F,
                               .height = 8.0F});
    map_def.world_props.push_back({.type = prop_type, .x = 16.0F, .z = 16.0F});
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);
  }

  auto find_collect_target(Game::Map::WorldProp::Type prop_type)
      -> std::optional<Game::Map::WorldPropTarget> {
    auto& terrain = Game::Map::TerrainService::instance();
    switch (prop_type) {
    case Game::Map::WorldProp::Type::PineTree:
      return terrain.find_tree_near_world(0.0F, 0.0F, 100.0F);
    case Game::Map::WorldProp::Type::Boulder:
      return terrain.find_boulder_near_world(0.0F, 0.0F, 100.0F);
    case Game::Map::WorldProp::Type::IronOre:
      return terrain.find_iron_ore_near_world(0.0F, 0.0F, 100.0F);
    default:
      return std::nullopt;
    }
  }

  auto elevated_collect_click_screen(const Game::Map::WorldPropTarget& target,
                                     float minimum_ground_distance) -> QPointF {
    float const minimum_ground_distance_sq =
        minimum_ground_distance * minimum_ground_distance;
    float const surface_y = collect_surface_y(target);
    for (float const height : {4.0F, 6.0F, 8.0F, 10.0F, 12.0F}) {
      QPointF const screen =
          world_to_screen(QVector3D(target.x, surface_y + height, target.z));
      QVector3D ground_hit;
      if (!Game::Systems::PickingService::screen_to_ground(
              QPointF(screen.x(), screen.y()),
              camera,
              viewport.width,
              viewport.height,
              ground_hit)) {
        continue;
      }

      float const dx = ground_hit.x() - target.x;
      float const dz = ground_hit.z() - target.z;
      if (dx * dx + dz * dz > minimum_ground_distance_sq) {
        return screen;
      }
    }

    ADD_FAILURE()
        << "Failed to find an elevated click that misses the ground fallback window";
    return world_to_screen(QVector3D(target.x, 0.0F, target.z));
  }

  auto collect_surface_y(const Game::Map::WorldPropTarget& target) -> float {
    return Game::Map::TerrainService::instance().resolve_surface_world_y(target.x,
                                                                         target.z);
  }

  void look_at_collect_target(const Game::Map::WorldPropTarget& target,
                              const QVector3D& camera_position) {
    camera.look_at(camera_position,
                   QVector3D(target.x, collect_surface_y(target), target.z),
                   QVector3D(0.0F, 1.0F, 0.0F));
  }

  void expect_collect_order_from_elevated_click(Game::Map::WorldProp::Type prop_type,
                                                const char* expected_product_type) {
    initialize_collect_map(prop_type);
    auto target = find_collect_target(prop_type);
    ASSERT_TRUE(target.has_value());

    auto* builder = add_selected_builder(-2.0F, 2.0F);
    ProductionManager manager(&world, &picking_service, &camera);
    manager.start_builder_construction(QStringLiteral("collect"));

    QPointF const screen = elevated_collect_click_screen(*target, 1.6F);
    manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

    ASSERT_TRUE(manager.construction_preview_active());
    EXPECT_TRUE(manager.construction_preview_valid());

    manager.on_construction_pointer_released(screen.x(), screen.y(), viewport);

    const auto* builder_prod =
        builder->get_component<Engine::Core::BuilderProductionComponent>();
    ASSERT_NE(builder_prod, nullptr);
    EXPECT_FALSE(manager.is_placing_construction());
    EXPECT_FALSE(builder_prod->is_placement_preview);
    EXPECT_TRUE(builder_prod->has_construction_site);
    EXPECT_TRUE(builder_prod->has_task_target);
    EXPECT_EQ(builder_prod->task_target_id, target->id);
    EXPECT_NEAR(builder_prod->construction_site_x, target->x, 0.0001F);
    EXPECT_NEAR(builder_prod->construction_site_z, target->z, 0.0001F);
    EXPECT_STREQ(builder_prod->product_type.c_str(), expected_product_type);
  }

  Engine::Core::World world;
  Game::Systems::SelectionSystem* selection = nullptr;
  Game::Systems::PickingService picking_service;
  Render::GL::Camera camera;
  ViewportState viewport{800, 600};
};

TEST_F(ProductionManagerTest, NonWallBuilderConstructionStartsWithoutGhostPreview) {
  auto* builder = add_selected_builder();
  ProductionManager manager(&world, &picking_service, &camera);

  manager.start_builder_construction(QStringLiteral("defense_tower"));

  const auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);
  EXPECT_TRUE(manager.is_placing_construction());
  EXPECT_FALSE(manager.construction_preview_active());
  EXPECT_FALSE(manager.construction_preview_valid());
  EXPECT_TRUE(builder_prod->is_placement_preview);
  EXPECT_FALSE(builder_prod->has_construction_site);
  EXPECT_TRUE(preview_entities().empty());
}

TEST_F(ProductionManagerTest, WallConstructionPreviewAppearsOnHoverAndRotates) {
  auto* builder = add_selected_builder();
  ProductionManager manager(&world, &picking_service, &camera);

  manager.start_builder_construction(QStringLiteral("wall_segment"));

  const auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);
  EXPECT_TRUE(manager.is_placing_construction());
  EXPECT_FALSE(manager.construction_preview_active());
  EXPECT_FALSE(manager.construction_preview_valid());
  EXPECT_EQ(manager.construction_preview_segment_count(), 0);
  EXPECT_FALSE(builder_prod->is_placement_preview);
  EXPECT_FALSE(builder_prod->has_construction_site);

  const QPointF screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

  auto previews = preview_entities();
  ASSERT_EQ(previews.size(), 1U);
  EXPECT_TRUE(manager.construction_preview_active());
  EXPECT_TRUE(manager.construction_preview_valid());
  EXPECT_TRUE(manager.construction_preview_rotatable());
  EXPECT_EQ(manager.construction_preview_segment_count(), 1);
  auto* preview_transform =
      previews.front()->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(preview_transform, nullptr);
  EXPECT_FLOAT_EQ(preview_transform->rotation.y, 0.0F);

  manager.on_construction_scroll(1.0F);

  previews = preview_entities();
  ASSERT_EQ(previews.size(), 1U);
  preview_transform =
      previews.front()->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(preview_transform, nullptr);
  EXPECT_FLOAT_EQ(preview_transform->rotation.y, 90.0F);
}

TEST_F(ProductionManagerTest, WallConstructionKeepsRotatedSingleSegmentOrientation) {
  add_selected_builder();
  ProductionManager manager(&world, &picking_service, &camera);

  manager.start_builder_construction(QStringLiteral("wall_segment"));

  const QPointF screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);
  manager.on_construction_scroll(1.0F);
  manager.on_construction_pointer_pressed(screen.x(), screen.y(), viewport);
  manager.on_construction_pointer_released(screen.x(), screen.y(), viewport);

  EXPECT_FALSE(manager.is_placing_construction());

  auto* site = find_wall_construction_site();
  ASSERT_NE(site, nullptr);
  auto* transform = site->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_FLOAT_EQ(transform->rotation.y, 90.0F);
}

TEST_F(ProductionManagerTest, DirectBuildingPlacementUsesGhostPreviewAndRotation) {
  ProductionManager manager(&world, &picking_service, &camera);

  manager.start_building_placement(QStringLiteral("defense_tower"), 1);

  EXPECT_TRUE(manager.is_placing_construction());
  EXPECT_FALSE(manager.construction_preview_active());
  EXPECT_TRUE(preview_entities().empty());

  const QPointF screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

  auto previews = preview_entities();
  ASSERT_EQ(previews.size(), 1U);
  EXPECT_TRUE(manager.construction_preview_active());
  EXPECT_TRUE(manager.construction_preview_valid());
  auto* preview_transform =
      previews.front()->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(preview_transform, nullptr);
  EXPECT_FLOAT_EQ(preview_transform->rotation.y, 0.0F);

  manager.on_construction_scroll(2.0F);

  previews = preview_entities();
  ASSERT_EQ(previews.size(), 1U);
  preview_transform =
      previews.front()->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(preview_transform, nullptr);
  EXPECT_FLOAT_EQ(preview_transform->rotation.y, 10.0F);

  manager.on_construction_confirm();

  EXPECT_FALSE(manager.is_placing_construction());
  EXPECT_FALSE(manager.construction_preview_active());
  EXPECT_TRUE(preview_entities().empty());

  auto* tower = find_spawned_unit(Game::Units::SpawnType::DefenseTower);
  ASSERT_NE(tower, nullptr);
  auto* tower_transform = tower->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(tower_transform, nullptr);
  EXPECT_FLOAT_EQ(tower_transform->rotation.y, 10.0F);
}

TEST_F(ProductionManagerTest, BuilderConstructionPreviewRotationCarriesIntoQueuedSite) {
  auto* builder = add_selected_builder();
  ProductionManager manager(&world, &picking_service, &camera);

  manager.start_builder_construction(QStringLiteral("defense_tower"));

  const QPointF screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

  auto previews = preview_entities();
  ASSERT_EQ(previews.size(), 1U);

  manager.on_construction_scroll(3.0F);
  manager.on_construction_confirm();

  const auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);
  EXPECT_FALSE(manager.is_placing_construction());
  EXPECT_FALSE(builder_prod->is_placement_preview);
  EXPECT_TRUE(builder_prod->has_construction_site);
  EXPECT_FLOAT_EQ(builder_prod->construction_site_rotation_y, 15.0F);
  EXPECT_TRUE(preview_entities().empty());
}

TEST_F(ProductionManagerTest, DirectBuildingPlacementRejectsConfirmWithoutResources) {
  ProductionManager manager(&world, &picking_service, &camera);
  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  resources.set(1, Game::Systems::ResourceType::Wood, 0);
  resources.set(1, Game::Systems::ResourceType::Stone, 0);

  manager.start_building_placement(QStringLiteral("defense_tower"), 1);

  const QPointF screen = world_to_screen(QVector3D(0.0F, 0.0F, 0.0F));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);
  ASSERT_TRUE(manager.construction_preview_valid());

  manager.on_construction_confirm();

  EXPECT_TRUE(manager.is_placing_construction());
  EXPECT_EQ(find_spawned_unit(Game::Units::SpawnType::DefenseTower), nullptr);

  resources.set(1, Game::Systems::ResourceType::Wood, 90);
  resources.set(1, Game::Systems::ResourceType::Stone, 120);

  manager.on_construction_confirm();

  EXPECT_FALSE(manager.is_placing_construction());
  EXPECT_NE(find_spawned_unit(Game::Units::SpawnType::DefenseTower), nullptr);
}

TEST_F(ProductionManagerTest, CollectPreviewSnapsToResourceCenterWhenNearby) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 4242U;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);
  Game::Map::TerrainService::instance().initialize(map_def);
  Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);

  auto& terrain = Game::Map::TerrainService::instance();
  auto target = terrain.find_boulder_near_world(0.0F, 0.0F, 100.0F);
  if (!target.has_value()) {
    target = terrain.find_iron_ore_near_world(0.0F, 0.0F, 100.0F);
  }
  ASSERT_TRUE(target.has_value());

  auto* builder = add_selected_builder();
  ProductionManager manager(&world, &picking_service, &camera);
  manager.start_builder_construction(QStringLiteral("collect"));

  const QPointF screen = world_to_screen(QVector3D(target->x + 0.9F, 0.0F, target->z));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

  const auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);
  EXPECT_TRUE(manager.is_placing_construction());
  EXPECT_TRUE(manager.construction_preview_active());
  EXPECT_TRUE(manager.construction_preview_valid());
  EXPECT_TRUE(builder_prod->is_placement_preview);
  EXPECT_NEAR(manager.m_construction_placement_position.x(), target->x, 0.0001F);
  EXPECT_NEAR(manager.m_construction_placement_position.z(), target->z, 0.0001F);
}

TEST_F(ProductionManagerTest,
       CollectPreviewStaysInvalidWhenPointerIsTooFarFromResource) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 4242U;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);
  Game::Map::TerrainService::instance().initialize(map_def);
  Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);

  auto& terrain = Game::Map::TerrainService::instance();
  auto target = terrain.find_boulder_near_world(0.0F, 0.0F, 100.0F);
  if (!target.has_value()) {
    target = terrain.find_iron_ore_near_world(0.0F, 0.0F, 100.0F);
  }
  ASSERT_TRUE(target.has_value());

  auto* builder = add_selected_builder();
  ProductionManager manager(&world, &picking_service, &camera);
  manager.start_builder_construction(QStringLiteral("collect"));

  const QPointF screen = world_to_screen(QVector3D(target->x + 4.6F, 0.0F, target->z));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

  const auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);
  EXPECT_TRUE(manager.is_placing_construction());
  EXPECT_FALSE(manager.construction_preview_active());
  EXPECT_FALSE(manager.construction_preview_valid());
  EXPECT_TRUE(builder_prod->is_placement_preview);
  EXPECT_TRUE(preview_entities().empty());
}

TEST_F(ProductionManagerTest, GenericCollectPicksTreeFromElevatedClick) {
  expect_collect_order_from_elevated_click(Game::Map::WorldProp::Type::PineTree,
                                           "cut_tree");
}

TEST_F(ProductionManagerTest, GenericCollectPicksBoulderFromElevatedClick) {
  expect_collect_order_from_elevated_click(Game::Map::WorldProp::Type::Boulder,
                                           "collect_stone");
}

TEST_F(ProductionManagerTest, GenericCollectPicksIronOreFromElevatedClick) {
  expect_collect_order_from_elevated_click(Game::Map::WorldProp::Type::IronOre,
                                           "collect_iron_ore");
}

TEST_F(ProductionManagerTest, GenericCollectPreviewStaysValidOnRaisedTerrain) {
  initialize_collect_hill_map(Game::Map::WorldProp::Type::PineTree);
  auto target = find_collect_target(Game::Map::WorldProp::Type::PineTree);
  ASSERT_TRUE(target.has_value());

  look_at_collect_target(*target, QVector3D(0.0F, 10.0F, 10.0F));

  auto* builder = add_selected_builder(-2.0F, 2.0F);
  ProductionManager manager(&world, &picking_service, &camera);
  manager.start_builder_construction(QStringLiteral("collect"));

  QPointF const screen = world_to_screen(
      QVector3D(target->x, collect_surface_y(*target) + 1.0F, target->z));
  manager.on_construction_mouse_move(screen.x(), screen.y(), viewport);

  EXPECT_TRUE(manager.construction_preview_active());
  EXPECT_EQ(manager.m_pending_harvest_target_id, target->id);
  EXPECT_NEAR(manager.m_construction_placement_position.x(), target->x, 0.0001F);
  EXPECT_NEAR(manager.m_construction_placement_position.z(), target->z, 0.0001F);
  EXPECT_TRUE(manager.construction_preview_valid());

  manager.on_construction_pointer_released(screen.x(), screen.y(), viewport);

  const auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);
  EXPECT_FALSE(manager.is_placing_construction());
  EXPECT_TRUE(builder_prod->has_task_target);
  EXPECT_EQ(builder_prod->task_target_id, target->id);
  EXPECT_NEAR(builder_prod->construction_site_x, target->x, 0.0001F);
  EXPECT_NEAR(builder_prod->construction_site_z, target->z, 0.0001F);
  EXPECT_STREQ(builder_prod->product_type.c_str(), "cut_tree");
}

TEST_F(ProductionManagerTest, RestartingCollectReleasesPreviousResourceReservation) {
  initialize_collect_map(Game::Map::WorldProp::Type::PineTree);
  auto target = find_collect_target(Game::Map::WorldProp::Type::PineTree);
  ASSERT_TRUE(target.has_value());

  auto* builder = add_selected_builder(-2.0F, 2.0F);
  auto* builder_prod =
      builder->get_component<Engine::Core::BuilderProductionComponent>();
  ASSERT_NE(builder_prod, nullptr);

  auto& terrain = Game::Map::TerrainService::instance();
  ASSERT_TRUE(terrain.reserve_world_prop(target->id));
  builder_prod->has_task_target = true;
  builder_prod->task_target_id = target->id;
  builder_prod->task_target_x = target->x;
  builder_prod->task_target_z = target->z;
  builder_prod->task_target_reserved = true;

  ProductionManager manager(&world, &picking_service, &camera);
  manager.start_builder_construction(QStringLiteral("collect"));

  EXPECT_FALSE(terrain.is_world_prop_reserved(target->id));
  QPointF const screen = elevated_collect_click_screen(*target, 3.6F);
  manager.on_construction_pointer_released(screen.x(), screen.y(), viewport);
  EXPECT_TRUE(terrain.is_world_prop_reserved(target->id));
  EXPECT_FALSE(manager.is_placing_construction());
}

TEST_F(ProductionManagerTest, SetRallyAtScreenTargetsOnlySelectedBarracks) {
  auto* barracks_a =
      add_selected_production_building(Game::Units::SpawnType::Barracks, -4.0F, 0.0F);
  auto* barracks_b =
      add_selected_production_building(Game::Units::SpawnType::Barracks, -2.0F, -1.5F);
  auto* home =
      add_selected_production_building(Game::Units::SpawnType::Home, -6.0F, 1.0F);
  ASSERT_NE(barracks_a, nullptr);
  ASSERT_NE(barracks_b, nullptr);
  ASSERT_NE(home, nullptr);

  auto* home_production = home->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(home_production, nullptr);
  home_production->rally_x = -9.0F;
  home_production->rally_z = 7.5F;
  home_production->rally_set = true;

  ProductionManager manager(&world, &picking_service, &camera);
  QPointF const screen = world_to_screen(QVector3D(6.0F, 0.0F, 4.0F));

  EXPECT_TRUE(manager.set_rally_at_screen(screen.x(), screen.y(), 1, viewport));

  for (auto* barracks : {barracks_a, barracks_b}) {
    auto* production = barracks->get_component<Engine::Core::ProductionComponent>();
    ASSERT_NE(production, nullptr);
    EXPECT_TRUE(production->rally_set);
    EXPECT_NEAR(production->rally_x, 6.0F, 0.05F);
    EXPECT_NEAR(production->rally_z, 4.0F, 0.05F);
  }

  EXPECT_TRUE(home_production->rally_set);
  EXPECT_FLOAT_EQ(home_production->rally_x, -9.0F);
  EXPECT_FLOAT_EQ(home_production->rally_z, 7.5F);
}

} // namespace
