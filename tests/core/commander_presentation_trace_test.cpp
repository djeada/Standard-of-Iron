#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_presentation_trace.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "scene/camera.h"

namespace {

class CommanderPresentationTraceTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto create_commander(Engine::Core::World& world,
                               float x,
                               float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
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
    return entity;
  }
};

TEST_F(CommanderPresentationTraceTest, TraceStaysEmptyUntilItIsEnabled) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  controller.input().forward = true;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));

  EXPECT_FALSE(controller.presentation_trace_enabled());
  EXPECT_FALSE(controller.presentation_trace().valid);
  EXPECT_EQ(controller.presentation_trace().sequence, 0U);
}

TEST_F(CommanderPresentationTraceTest, WalkingRecordsMotorPoseVelocityAndSource) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  constexpr float k_dt = 1.0F / 60.0F;
  for (int frame = 0; frame < 30; ++frame) {
    controller.input().forward = true;
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }

  auto const& trace = controller.presentation_trace();
  ASSERT_TRUE(trace.valid);
  EXPECT_EQ(trace.sequence, 30U);
  EXPECT_EQ(trace.motor.displacement_source,
            App::Core::CommanderDisplacementSource::Walk);
  EXPECT_GT(trace.motor.requested_speed, 0.0F);
  EXPECT_GT(trace.motor.actual_velocity.length(), 0.0F);
  EXPECT_NEAR(trace.motor.dt, k_dt, 1.0e-6F);

  QVector3D const stepped = trace.motor.position - trace.motor.previous_position;
  EXPECT_NEAR(stepped.length(), trace.motor.actual_velocity.length() * k_dt, 1.0e-4F);
}

TEST_F(CommanderPresentationTraceTest, BlockedGroundIsRecordedAsBlockedNotAsMotion) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (int offset = -2; offset <= 2; ++offset) {
    auto const blocked =
        Game::Systems::NavGrid::world_to_grid(static_cast<float>(offset), 1.0F);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  constexpr float k_dt = 1.0F / 60.0F;
  bool saw_blocked = false;
  for (int frame = 0; frame < 120; ++frame) {
    controller.input().forward = true;
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    saw_blocked = saw_blocked || controller.presentation_trace().motor.blocked;
  }

  EXPECT_TRUE(saw_blocked)
      << "a wall the commander walks into must appear in the motor trace";
}

TEST_F(CommanderPresentationTraceTest,
       OnePressAdvancesThePressAndConsumedSequenceOnce) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  EXPECT_EQ(controller.input_edges().primary_press_sequence, 0U);
  controller.primary_action_down();
  EXPECT_EQ(controller.input_edges().primary_press_sequence, 1U);

  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));
  EXPECT_EQ(controller.input_edges().primary_consumed_sequence, 1U);
  EXPECT_EQ(controller.input_edges().primary_dropped_sequence, 0U);

  controller.primary_action_up();
  EXPECT_EQ(controller.input_edges().primary_release_sequence, 1U);
}

TEST_F(CommanderPresentationTraceTest, PressAndReleaseBetweenTicksIsRecordedAsDropped) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  controller.primary_action_down();
  controller.primary_action_up();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));

  auto const& edges = controller.input_edges();
  EXPECT_EQ(edges.primary_press_sequence, 1U);
  EXPECT_EQ(edges.primary_release_sequence, 1U);
  EXPECT_EQ(edges.primary_consumed_sequence + edges.primary_dropped_sequence, 1U)
      << "an edge is either consumed or dropped; it may not vanish unaccounted for";
}

TEST_F(CommanderPresentationTraceTest, DodgeAndJumpRequestsAreCountedAndResolved) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  controller.request_dodge();
  controller.request_jump();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));

  auto const& edges = controller.input_edges();
  EXPECT_EQ(edges.dodge_request_sequence, 1U);
  EXPECT_EQ(edges.dodge_consumed_sequence + edges.dodge_refused_sequence, 1U);
  EXPECT_EQ(edges.jump_request_sequence, 1U);
  EXPECT_EQ(edges.jump_consumed_sequence + edges.jump_refused_sequence, 1U);
}

TEST_F(CommanderPresentationTraceTest, CameraTraceCarriesBothTheDesiredAndResolvedEye) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  for (int frame = 0; frame < 10; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));
  }

  auto const& shot = controller.presentation_trace().camera;
  ASSERT_TRUE(shot.valid);
  EXPECT_GT(shot.boom_unconstrained, 0.0F);
  EXPECT_GT(shot.boom_resolved, 0.0F);
  EXPECT_LE(shot.boom_resolved, shot.boom_unconstrained + 1.0e-3F)
      << "collision may only retract the boom, never extend it";
  EXPECT_NEAR(shot.boom_clear_fraction, 1.0F, 1.0e-3F)
      << "no building was registered, so nothing may report as blocking";
  EXPECT_NE(shot.eye_resolved, shot.target_resolved);
}

TEST_F(CommanderPresentationTraceTest, DisablingTheTraceClearsIt) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));
  ASSERT_TRUE(controller.presentation_trace().valid);

  controller.set_presentation_trace_enabled(false);
  EXPECT_FALSE(controller.presentation_trace().valid);
  EXPECT_EQ(controller.presentation_trace().sequence, 0U);
}

TEST_F(CommanderPresentationTraceTest, EveryPressEdgeIsAccountedForDuringFlagRally) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  commander_data->flag_rally_in_progress = true;
  commander_data->fpv_controlled = false;

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;

  controller.primary_action_down();
  controller.request_dodge();
  controller.request_jump();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 1.0F / 60.0F));

  auto const& edges = controller.input_edges();
  EXPECT_EQ(edges.dodge_consumed_sequence + edges.dodge_refused_sequence, 1U)
      << "a dodge discarded while placing a rally flag must still be accounted for";
  EXPECT_EQ(edges.jump_consumed_sequence + edges.jump_refused_sequence, 1U)
      << "a jump discarded while placing a rally flag must still be accounted for";
  EXPECT_EQ(edges.primary_consumed_sequence + edges.primary_dropped_sequence, 1U)
      << "an attack discarded while placing a rally flag must still be accounted for";
}

TEST_F(CommanderPresentationTraceTest, AHeldPressSurvivesTicksTheBodyCannotAct) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  controller.set_presentation_trace_enabled(true);
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.request_dodge();
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  ASSERT_TRUE(controller.is_dodge_rolling())
      << "the press has to arrive while the body is refusing input";

  controller.primary_action_down();
  for (int frame = 0; frame < 3; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
    ASSERT_TRUE(controller.is_dodge_rolling());
    EXPECT_EQ(controller.input_edges().primary_consumed_sequence, 0U);
    EXPECT_EQ(controller.input_edges().primary_dropped_sequence, 0U);
  }

  for (int frame = 0; frame < 60 && controller.is_dodge_rolling(); ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }
  ASSERT_FALSE(controller.is_dodge_rolling());
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));

  auto const& edges = controller.input_edges();
  EXPECT_EQ(edges.primary_press_sequence, 1U);
  EXPECT_EQ(edges.primary_consumed_sequence, 1U)
      << "a press held across a refusing tick is consumed once the body frees up";
  EXPECT_EQ(edges.primary_dropped_sequence, 0U);
}

TEST_F(CommanderPresentationTraceTest, ATickConsumesEachEdgeAtMostOnce) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  CommanderControlController controller;
  Render::GL::Camera camera;
  constexpr float k_dt = 1.0F / 60.0F;

  controller.request_dodge();
  controller.request_jump();
  for (int frame = 0; frame < 20; ++frame) {
    ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, k_dt));
  }

  auto const& edges = controller.input_edges();
  EXPECT_EQ(edges.dodge_request_sequence, 1U);
  EXPECT_EQ(edges.dodge_consumed_sequence + edges.dodge_refused_sequence, 1U)
      << "the snapshot moves an edge out of the producer, so only one tick sees it";
  EXPECT_EQ(edges.jump_request_sequence, 1U);
  EXPECT_EQ(edges.jump_consumed_sequence + edges.jump_refused_sequence, 1U);
}

TEST(CommanderDisplacementSourceTest, EverySourceHasAStableName) {
  using App::Core::CommanderDisplacementSource;
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::None), "none");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::Walk), "walk");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::DodgeRoll),
               "dodge_roll");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::DodgeRecover),
               "dodge_recover");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::StrikeLunge),
               "strike_lunge");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::BodySeparation),
               "body_separation");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::JumpRecovery),
               "jump_recovery");
  EXPECT_STREQ(displacement_source_name(CommanderDisplacementSource::Airborne),
               "airborne");
}

} // namespace
