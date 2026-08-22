#include <cmath>
#include <functional>
#include <gtest/gtest.h>
#include <string>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_latency_probe.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "scene/camera.h"

namespace {

constexpr double k_render_frame_seconds = 1.0 / 144.0;
constexpr double k_simulation_tick_seconds = 1.0 / 60.0;

class CommanderDirectControlLatencyTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(64, 64);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto create_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    auto* movement = entity->add_component<Engine::Core::MovementComponent>();
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (transform == nullptr || unit == nullptr || movement == nullptr ||
        commander == nullptr) {
      return nullptr;
    }
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    commander->fpv_controlled = true;
    return entity;
  }

  using Injection = std::function<void(CommanderControlController&)>;

  static auto measure(const Injection& inject, double settle_seconds = 0.5)
      -> App::Core::CommanderLatencySample {
    Engine::Core::World world;
    auto* commander = create_commander(world);
    EXPECT_NE(commander, nullptr);
    if (commander == nullptr) {
      return {};
    }

    App::Core::CommanderLatencyProbe probe;
    CommanderControlController controller;
    controller.set_latency_probe(&probe);
    Render::GL::Camera camera;

    const auto commander_id = commander->get_id();
    double now = 0.0;
    double tick_accumulator = 0.0;
    bool injected = false;

    const auto run_frame = [&]() {
      probe.set_time(now);
      static_cast<void>(controller.sample_frame_intent(nullptr));
      controller.update_camera_presentation(
          world, commander_id, camera, static_cast<float>(k_render_frame_seconds));

      tick_accumulator += k_render_frame_seconds;
      while (tick_accumulator >= k_simulation_tick_seconds) {
        tick_accumulator -= k_simulation_tick_seconds;
        static_cast<void>(controller.update_simulation(
            world, commander_id, 1, static_cast<float>(k_simulation_tick_seconds)));
      }
      now += k_render_frame_seconds;
    };

    for (int warmup = 0; warmup < 30; ++warmup) {
      run_frame();
    }

    const auto settle_frames =
        static_cast<int>(std::ceil(settle_seconds / k_render_frame_seconds));
    for (int frame = 0; frame < settle_frames; ++frame) {
      if (!injected) {
        injected = true;
        probe.set_time(now);
        inject(controller);
      }
      run_frame();
    }

    return probe.sample();
  }

  static auto budget() -> App::Core::CommanderLatencyBudget {
    App::Core::CommanderLatencyBudget limits;
    limits.frame_seconds = k_render_frame_seconds;
    limits.tick_seconds = k_simulation_tick_seconds;
    limits.camera_frames = 1.0;
    limits.pose_frames = 1.0;
    limits.pose_ticks = 1.0;
    limits.movement_frames = 1.0;
    limits.movement_ticks = 1.0;
    limits.simulation_ticks = 1.0;
    return limits;
  }

  static void expect_within_budget(const App::Core::CommanderLatencySample& sample) {
    const auto verdict = App::Core::evaluate(sample, budget());
    std::string report;
    for (const auto& failure : verdict.failures) {
      report += "\n  " + failure;
    }
    EXPECT_TRUE(verdict.passed) << App::Core::describe(sample) << report;
  }
};

TEST_F(CommanderDirectControlLatencyTest,
       MouseLookReachesTheCameraWithinOneRenderedFrame) {
  const auto sample = measure(
      [](CommanderControlController& controller) { controller.mouse_move(24.0, 0.0); });

  ASSERT_TRUE(sample.has_input());
  ASSERT_GE(sample.camera_seconds, 0.0);
  EXPECT_LE(sample.camera_seconds - sample.input_seconds,
            k_render_frame_seconds + 1.0e-6)
      << App::Core::describe(sample);
}

TEST_F(CommanderDirectControlLatencyTest,
       LocomotionInputIsHonouredWithinOneSimulationTick) {
  const auto sample = measure(
      [](CommanderControlController& controller) { controller.key_down(Qt::Key_W); });

  ASSERT_TRUE(sample.has_input());
  ASSERT_GE(sample.simulation_seconds, 0.0);
  EXPECT_LE(sample.simulation_seconds - sample.input_seconds,
            k_simulation_tick_seconds + 1.0e-6)
      << App::Core::describe(sample);
  expect_within_budget(sample);
}

TEST_F(CommanderDirectControlLatencyTest,
       GuardBeginsRaisingWithinTheDirectControlBudget) {
  const auto sample = measure([](CommanderControlController& controller) {
    controller.secondary_action_down();
  });

  ASSERT_TRUE(sample.has_input());
  EXPECT_GE(sample.guard_start_seconds, 0.0) << App::Core::describe(sample);
  expect_within_budget(sample);
}

TEST_F(CommanderDirectControlLatencyTest, AttackStartsWithinTheDirectControlBudget) {
  const auto sample = measure(
      [](CommanderControlController& controller) { controller.primary_action_down(); });

  ASSERT_TRUE(sample.has_input());
  EXPECT_GE(sample.attack_start_seconds, 0.0) << App::Core::describe(sample);
  expect_within_budget(sample);
}

TEST_F(CommanderDirectControlLatencyTest, DodgeStartsWithinTheDirectControlBudget) {
  const auto sample = measure(
      [](CommanderControlController& controller) { controller.request_dodge(); });

  ASSERT_TRUE(sample.has_input());
  EXPECT_GE(sample.dodge_start_seconds, 0.0) << App::Core::describe(sample);
  expect_within_budget(sample);
}

TEST_F(CommanderDirectControlLatencyTest,
       DirectControlBudgetIsFarTighterThanTheRtsContract) {
  const auto limits = budget();
  constexpr double k_rts_response_budget_seconds = 0.20;

  EXPECT_LT(limits.camera_frames * limits.frame_seconds, k_rts_response_budget_seconds);
  EXPECT_LT(limits.simulation_ticks * limits.tick_seconds,
            k_rts_response_budget_seconds);
  EXPECT_LT((limits.pose_frames * limits.frame_seconds) +
                (limits.pose_ticks * limits.tick_seconds),
            k_rts_response_budget_seconds * 0.25);
}

TEST_F(CommanderDirectControlLatencyTest, MissingResponsesFailTheContract) {
  App::Core::CommanderLatencySample sample;
  sample.input_seconds = 0.0;
  sample.camera_seconds = 0.5;
  sample.simulation_seconds = 0.5;

  const auto verdict = App::Core::evaluate(sample, budget());
  EXPECT_FALSE(verdict.passed);
  EXPECT_EQ(verdict.failures.size(), 2U);
}

} // namespace
