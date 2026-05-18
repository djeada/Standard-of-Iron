#include <gtest/gtest.h>

#include "app/core/minimap_manager.h"
#include "app/core/runtime_frame_orchestrator.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/selection_system.h"
#include "render/gl/camera.h"

namespace {

auto make_test_map(int width = 12, int height = 12) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.name = "Frame Orchestrator Test";
  map.grid.width = width;
  map.grid.height = height;
  map.grid.tile_size = 1.0F;
  map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);
  map.biome.grass_secondary = QVector3D(0.44F, 0.7F, 0.32F);
  map.biome.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
  return map;
}

auto add_unit(Engine::Core::World& world, float x, float z, int owner_id)
    -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  (void)entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit->owner_id = owner_id;
  return entity;
}

TEST(RuntimeFrameOrchestratorTest, SimulationRunsBeforeMinimapNotifier) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());

  (void)add_unit(world, 1.0F, 1.0F, 1);

  MinimapManager minimap_manager;
  minimap_manager.generate_for_map(make_test_map());
  (void)minimap_manager.consume_dirty_flag();

  Render::GL::Camera camera;
  camera.set_perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
  camera.look_at(QVector3D(0.0F, 10.0F, 10.0F),
                 QVector3D(0.0F, 0.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));

  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.local_owner_id = 1, .viewport_width = 800, .viewport_height = 600};
  EntityCache entity_cache;
  bool simulation_ran = false;
  int minimap_notifications = 0;

  orchestrator.update(
      AppSceneContext{.world = &world,
                      .active_camera = &camera,
                      .minimap_manager = &minimap_manager},
      state,
      entity_cache,
      nullptr,
      QString(),
      0.016F,
      FrameUpdateCallbacks{
          .on_minimap_image_changed =
              [&]() {
                EXPECT_TRUE(simulation_ran);
                ++minimap_notifications;
              }},
      [&](float) { simulation_ran = true; });

  EXPECT_TRUE(simulation_ran);
  EXPECT_EQ(minimap_notifications, 1);
}

TEST(RuntimeFrameOrchestratorTest, SelectionRefreshNotifierFiresAtThreshold) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection_system = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection_system, nullptr);

  auto* unit = add_unit(world, 0.0F, 0.0F, 1);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{
      .local_owner_id = 1, .selection_refresh_enabled = true, .selection_refresh_counter = 14};
  EntityCache entity_cache;
  int selection_notifications = 0;

  orchestrator.update(AppSceneContext{.world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      0.016F,
                      FrameUpdateCallbacks{
                          .on_selected_units_data_changed =
                              [&]() { ++selection_notifications; }},
                      [](float) {});

  EXPECT_EQ(selection_notifications, 1);
  EXPECT_EQ(state.selection_refresh_counter, 0);
}

} // namespace
