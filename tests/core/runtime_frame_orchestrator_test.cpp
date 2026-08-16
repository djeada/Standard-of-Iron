#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/core/minimap_manager.h"
#include "app/core/runtime_frame_orchestrator.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/command/command_system.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/session/world_digest.h"
#include "game/systems/movement_system.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"
#include "scene/camera.h"

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

auto add_unit(Engine::Core::World& world,
              float x,
              float z,
              int owner_id) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  (void)entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit->owner_id = owner_id;
  return entity;
}

TEST(RuntimeFrameOrchestratorTest, SimulationRunsBeforeMinimapNotifier) {
  Game::Session::SessionContext session;
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
  RuntimeFrameState state{
      .local_owner_id = 1, .viewport_width = 800, .viewport_height = 600};
  EntityCache entity_cache;
  bool simulation_ran = false;
  int minimap_notifications = 0;

  orchestrator.update(AppSceneContext{.session = &session,
                                      .world = &world,
                                      .active_camera = &camera,
                                      .minimap_manager = &minimap_manager},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      1.0F / 60.0F,
                      FrameUpdateCallbacks{.on_minimap_image_changed =
                                               [&]() {
                                                 EXPECT_TRUE(simulation_ran);
                                                 ++minimap_notifications;
                                               }},
                      [&](float) { simulation_ran = true; });

  EXPECT_TRUE(simulation_ran);
  EXPECT_EQ(minimap_notifications, 1);
}

TEST(RuntimeFrameOrchestratorTest, SimulationUsesFixedSixtyHertzSteps) {
  Game::Session::SessionContext session;
  Engine::Core::World world;
  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state;
  EntityCache entity_cache;
  std::vector<float> steps;
  const auto simulation_step = [&](float dt) {
    steps.push_back(dt);
  };

  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      0.01F,
                      {},
                      simulation_step);
  EXPECT_TRUE(steps.empty());

  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      0.01F,
                      {},
                      simulation_step);
  ASSERT_EQ(steps.size(), 1U);
  EXPECT_NEAR(steps.front(), 1.0F / 60.0F, 0.000001F);

  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      0.05F,
                      {},
                      simulation_step);
  EXPECT_EQ(steps.size(), 4U);
  EXPECT_TRUE(std::all_of(steps.begin(), steps.end(), [](float step) {
    return std::abs(step - 1.0F / 60.0F) < 0.000001F;
  }));
}

TEST(RuntimeFrameOrchestratorTest, HigherSpeedsRunProportionallyMoreFixedSteps) {
  struct Case {
    float speed;
    int expected_steps;
  };
  const std::vector<Case> cases{
      {0.5F, 30}, {1.0F, 60}, {2.0F, 120}, {3.0F, 180}, {4.0F, 240}};

  for (const Case& scenario : cases) {
    Game::Session::SessionContext session;
    Engine::Core::World world;
    RuntimeFrameOrchestrator orchestrator;
    RuntimeFrameState state{.simulation_time_scale = scenario.speed};
    EntityCache entity_cache;
    int steps = 0;
    std::uint64_t dropped = 0;

    for (int frame = 0; frame < 60; ++frame) {
      orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                          state,
                          entity_cache,
                          nullptr,
                          QString(),
                          1.0F / 60.0F,
                          {},
                          [&](float) { ++steps; });
      dropped += state.dropped_simulation_ticks;
    }

    EXPECT_EQ(steps, scenario.expected_steps) << "at " << scenario.speed << "x";
    EXPECT_EQ(dropped, 0U) << "at " << scenario.speed << "x";
  }
}

TEST(RuntimeFrameOrchestratorTest, AStutteringFrameStillKeepsUpAtQuadrupleSpeed) {
  Game::Session::SessionContext session;
  Engine::Core::World world;
  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.simulation_time_scale = 4.0F};
  EntityCache entity_cache;
  int steps = 0;

  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      1.0F / 8.0F,
                      {},
                      [&](float) { ++steps; });

  EXPECT_EQ(steps, 30);
  EXPECT_EQ(state.dropped_simulation_ticks, 0U);
}

TEST(RuntimeFrameOrchestratorTest, ASimulationOverrunIsReportedRatherThanSwallowed) {
  Game::Session::SessionContext session;
  Engine::Core::World world;
  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.simulation_time_scale = 4.0F};
  EntityCache entity_cache;
  int steps = 0;

  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      2.0F,
                      {},
                      [&](float) { ++steps; });

  EXPECT_EQ(steps, 32);
  EXPECT_EQ(state.dropped_simulation_ticks, 28U + 420U);
}

TEST(RuntimeFrameOrchestratorTest, AZeroTimeScaleStopsTheSimulationWithoutBanking) {
  Game::Session::SessionContext session;
  Engine::Core::World world;
  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.simulation_time_scale = 0.0F};
  EntityCache entity_cache;
  int steps = 0;
  const auto count = [&](float) { ++steps; };

  for (int frame = 0; frame < 120; ++frame) {
    orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                        state,
                        entity_cache,
                        nullptr,
                        QString(),
                        1.0F / 60.0F,
                        {},
                        count);
  }
  EXPECT_EQ(steps, 0);
  EXPECT_EQ(state.dropped_simulation_ticks, 0U);

  state.simulation_time_scale = 1.0F;
  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      1.0F / 60.0F,
                      {},
                      count);
  EXPECT_EQ(steps, 1);
}

TEST(RuntimeFrameOrchestratorTest, MovingUnitMarkersUpdateAtMinimapCadence) {
  Game::Session::SessionContext session;
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* unit = add_unit(world, 1.0F, 1.0F, 1);
  ASSERT_NE(unit, nullptr);

  MinimapManager minimap_manager;
  minimap_manager.generate_for_map(make_test_map());
  (void)minimap_manager.consume_dirty_flag();

  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.local_owner_id = 1};
  EntityCache entity_cache;
  int minimap_notifications = 0;
  const AppSceneContext scene{.world = &world, .minimap_manager = &minimap_manager};
  const FrameUpdateCallbacks callbacks{.on_minimap_image_changed = [&]() {
    ++minimap_notifications;
  }};

  orchestrator.update(
      scene, state, entity_cache, nullptr, QString(), 0.016F, callbacks, [](float) {});
  EXPECT_EQ(minimap_notifications, 1);

  auto* transform = unit->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->position.x = 2.0F;

  for (int frame = 0; frame < 3; ++frame) {
    orchestrator.update(
        scene, state, entity_cache, nullptr, QString(), 0.01F, callbacks, [](float) {});
  }
  EXPECT_EQ(minimap_notifications, 1);

  orchestrator.update(
      scene, state, entity_cache, nullptr, QString(), 0.01F, callbacks, [](float) {});
  EXPECT_EQ(minimap_notifications, 2);
}

struct PacedRun {
  std::uint64_t digest = 0;
  std::uint64_t ticks = 0;
};

auto run_paced_battle(float speed, float real_dt, int frames) -> PacedRun {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  session.owners().register_owner_with_id(
      1, Game::Systems::OwnerType::Player, "commander");
  Game::Systems::NavGrid::initialize(64, 64);

  auto& world = session.world();
  world.add_system(std::make_unique<Game::Command::CommandSystem>());
  world.add_system(std::make_unique<Game::Systems::MovementSystem>());

  std::vector<Engine::Core::EntityID> squad;
  for (int index = 0; index < 4; ++index) {
    auto* entity = world.create_entity();
    (void)entity->add_component<Engine::Core::TransformComponent>(
        static_cast<float>(index), 0.0F, 0.0F);
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    (void)entity->add_component<Engine::Core::MovementComponent>();
    squad.push_back(entity->get_id());
  }

  const auto order_at = [&](std::uint64_t tick, float target_x, float target_z) {
    Game::Command::Move move;
    move.units = squad;
    for (std::size_t index = 0; index < squad.size(); ++index) {
      move.targets.emplace_back(
          target_x + static_cast<float>(index), 0.0F, target_z);
    }
    session.commands().submit(
        Game::Command::Source::LocalPlayer, 1, Game::Command::Payload{move});
    (void)tick;
  };

  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.simulation_time_scale = speed};
  EntityCache entity_cache;

  for (int frame = 0; frame < frames; ++frame) {
    orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                        state,
                        entity_cache,
                        nullptr,
                        QString(),
                        real_dt,
                        {},
                        [&](float step) {
                          const std::uint64_t tick = session.clock().tick();
                          if (tick == 30) {
                            order_at(tick, 20.0F, 14.0F);
                          }
                          if (tick == 150) {
                            order_at(tick, 4.0F, 2.0F);
                          }
                          world.update(step);
                        });
  }

  return {Game::Session::session_digest(session), session.clock().tick()};
}

TEST(RuntimeFrameOrchestratorTest, TheSameBattleTimeLandsIdenticallyAtEverySpeed) {
  const PacedRun normal = run_paced_battle(1.0F, 1.0F / 60.0F, 240);
  const PacedRun doubled = run_paced_battle(2.0F, 1.0F / 60.0F, 120);
  const PacedRun quadruple = run_paced_battle(4.0F, 1.0F / 60.0F, 60);
  const PacedRun choppy = run_paced_battle(4.0F, 1.0F / 20.0F, 20);

  EXPECT_EQ(normal.ticks, 240U);
  EXPECT_EQ(doubled.ticks, normal.ticks);
  EXPECT_EQ(quadruple.ticks, normal.ticks);
  EXPECT_EQ(choppy.ticks, normal.ticks);

  EXPECT_NE(normal.digest, 0U);
  EXPECT_EQ(doubled.digest, normal.digest);
  EXPECT_EQ(quadruple.digest, normal.digest);
  EXPECT_EQ(choppy.digest, normal.digest);
}

TEST(RuntimeFrameOrchestratorTest, SelectionRefreshNotifierFiresAtThreshold) {
  Game::Session::SessionContext session;
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection_system = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection_system, nullptr);

  auto* unit = add_unit(world, 0.0F, 0.0F, 1);
  ASSERT_NE(unit, nullptr);
  selection_system->select_unit(unit->get_id());

  RuntimeFrameOrchestrator orchestrator;
  RuntimeFrameState state{.local_owner_id = 1,
                          .selection_refresh_enabled = true,
                          .selection_refresh_counter = 14};
  EntityCache entity_cache;
  int selection_notifications = 0;

  orchestrator.update(AppSceneContext{.session = &session, .world = &world},
                      state,
                      entity_cache,
                      nullptr,
                      QString(),
                      0.016F,
                      FrameUpdateCallbacks{.on_selected_units_data_changed =
                                               [&]() {
                                                 ++selection_notifications;
                                               }},
                      [](float) {});

  EXPECT_EQ(selection_notifications, 1);
  EXPECT_EQ(state.selection_refresh_counter, 0);
}

} // namespace
