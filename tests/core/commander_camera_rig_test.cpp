#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_camera_rig.h"
#include "app/commander/rts_camera_bookmark.h"
#include "core/component_commander.h"
#include "game/accessibility/commander_input_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "scene/camera.h"

using App::Core::CommanderCameraInputs;
using App::Core::CommanderCameraRig;
using App::Core::CommanderFramingState;
using Engine::Core::FightContext;

namespace {

auto default_inputs() -> CommanderCameraInputs {
  CommanderCameraInputs inputs;
  inputs.dt = 1.0F / 60.0F;
  inputs.fight_context = FightContext::None;
  return inputs;
}

void settle(CommanderCameraRig& rig,
            Render::GL::Camera& camera,
            const CommanderCameraInputs& inputs,
            int steps = 240) {
  for (int i = 0; i < steps; ++i) {
    rig.update(camera, inputs);
  }
}

} // namespace

TEST(CommanderCameraRig, BowAimOverridesEveryOtherFraming) {
  EXPECT_EQ(CommanderCameraRig::select_framing(true, true, FightContext::Duel),
            CommanderFramingState::BowAim);
  EXPECT_EQ(CommanderCameraRig::select_framing(true, false, FightContext::None),
            CommanderFramingState::BowAim);
}

TEST(CommanderCameraRig, DuelLockNeedsBothALockAndADuelContext) {
  EXPECT_EQ(CommanderCameraRig::select_framing(false, true, FightContext::Duel),
            CommanderFramingState::DuelLock);
  EXPECT_EQ(CommanderCameraRig::select_framing(false, false, FightContext::Duel),
            CommanderFramingState::Melee);
  EXPECT_EQ(CommanderCameraRig::select_framing(false, true, FightContext::Skirmish),
            CommanderFramingState::Melee);
}

TEST(CommanderCameraRig, NoRingMeansExploreFraming) {
  EXPECT_EQ(CommanderCameraRig::select_framing(false, false, FightContext::None),
            CommanderFramingState::Explore);
}

TEST(CommanderCameraRig, AimingTightensTheFieldOfView) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const hip_fov = rig.fov();

  inputs.aiming_bow = true;
  settle(rig, camera, inputs);
  float const aim_fov = rig.fov();

  EXPECT_GT(hip_fov, 60.0F);
  EXPECT_LT(aim_fov, 55.0F);
  EXPECT_GT(rig.aim_blend(), 0.9F);
  EXPECT_EQ(rig.framing_state(), CommanderFramingState::BowAim);
}

TEST(CommanderCameraRig, ImpactKickDecaysBackToRest) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const rest_fov = rig.fov();

  rig.add_impact_kick(1.0F);
  rig.update(camera, inputs);
  float const kicked_fov = rig.fov();
  EXPECT_GT(kicked_fov, rest_fov);

  settle(rig, camera, inputs);
  EXPECT_NEAR(rig.fov(), rest_fov, 0.1F);
}

TEST(CommanderCameraRig, ImpactKickRespectsItsAmplitudeAndVelocityBudget) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const rest_fov = rig.fov();

  rig.add_impact_kick(20.0F);
  float previous = rest_fov;
  float peak = rest_fov;
  for (int frame = 0; frame < 60; ++frame) {
    rig.update(camera, inputs);
    float const now = rig.fov();
    EXPECT_LE(std::abs(now - previous), (45.0F * inputs.dt) + 1.0e-3F)
        << "the impulse may not move the FOV faster than its velocity budget";
    peak = std::max(peak, now);
    previous = now;
  }

  EXPECT_LE(peak - rest_fov, 3.0F + 1.0e-3F)
      << "an oversized impact must still clamp to the authored amplitude budget";
}

TEST(CommanderCameraRig, ImpactKickIsOffWhenTheCameraImpulseIsDisabled) {
  Game::Accessibility::CommanderInput::reset_to_defaults();
  Game::Accessibility::CommanderInput::set_camera_impulse_enabled(false);

  CommanderCameraRig rig;
  Render::GL::Camera camera;
  auto inputs = default_inputs();
  settle(rig, camera, inputs);
  float const rest_fov = rig.fov();

  rig.add_impact_kick(1.0F);
  rig.update(camera, inputs);
  EXPECT_NEAR(rig.fov(), rest_fov, 0.05F);

  Game::Accessibility::CommanderInput::reset_to_defaults();
}

TEST(CommanderCameraRig, MeleeFramingLooksDownAtTheFightButAimingDoesNot) {
  CommanderCameraRig melee_rig;
  CommanderCameraRig explore_rig;
  Render::GL::Camera camera;

  auto explore = default_inputs();
  settle(explore_rig, camera, explore);

  auto melee = default_inputs();
  melee.fight_context = FightContext::Skirmish;
  settle(melee_rig, camera, melee);

  ASSERT_EQ(melee_rig.framing_state(), CommanderFramingState::Melee);
  EXPECT_LT(melee_rig.forward().y(), explore_rig.forward().y())
      << "melee framing must tilt toward the fight so it fills the frame";

  CommanderCameraRig aim_rig;
  auto aiming = default_inputs();
  aiming.fight_context = FightContext::Skirmish;
  aiming.aiming_bow = true;
  settle(aim_rig, camera, aiming);

  ASSERT_EQ(aim_rig.framing_state(), CommanderFramingState::BowAim);
  EXPECT_NEAR(aim_rig.forward().y(), explore_rig.forward().y(), 0.02F)
      << "the bow reticle is the camera axis; aiming may never be tilted off it";
}

TEST(CommanderCameraRig, ResetClearsSmoothedState) {
  CommanderCameraRig rig;
  Render::GL::Camera camera;

  auto inputs = default_inputs();
  inputs.move_speed = 2.0F;
  settle(rig, camera, inputs, 30);
  EXPECT_TRUE(rig.eye_valid());

  rig.reset();
  EXPECT_FALSE(rig.eye_valid());
  EXPECT_EQ(rig.aim_blend(), 0.0F);
  EXPECT_EQ(rig.bob_amplitude(), 0.0F);
  EXPECT_EQ(rig.framing_state(), CommanderFramingState::Explore);
}

namespace {

auto rig_inputs_at(float yaw,
                   const QVector3D& position,
                   float dt) -> App::Core::CommanderCameraInputs {
  App::Core::CommanderCameraInputs inputs;
  inputs.dt = dt;
  inputs.view_yaw_degrees = yaw;
  inputs.view_pitch_degrees = 0.0F;
  inputs.commander_position = position;
  return inputs;
}

} // namespace

TEST(CommanderCameraRigFeelTest, MouseRotationIsAppliedWithoutSmoothingLag) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;
  const QVector3D anchor(0.0F, 0.0F, 0.0F);

  for (int settle = 0; settle < 240; ++settle) {
    static_cast<void>(rig.update(camera, rig_inputs_at(0.0F, anchor, 1.0F / 60.0F)));
  }
  const QVector3D settled_eye = camera.get_position();

  static_cast<void>(rig.update(camera, rig_inputs_at(90.0F, anchor, 1.0F / 60.0F)));
  const QVector3D turned_eye = camera.get_position();

  EXPECT_GT((turned_eye - settled_eye).length(), 2.0F)
      << "a 90 degree turn barely moved the eye, so rotation is still being smoothed";

  static_cast<void>(rig.update(camera, rig_inputs_at(90.0F, anchor, 1.0F / 60.0F)));
  EXPECT_LT((camera.get_position() - turned_eye).length(), 0.05F)
      << "the eye kept drifting after the turn finished";
}

TEST(CommanderCameraRigFeelTest, TheVisualAnchorLagsTheSimulationPositionButIsBounded) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;

  static_cast<void>(rig.update(
      camera, rig_inputs_at(0.0F, QVector3D(0.0F, 0.0F, 0.0F), 1.0F / 60.0F)));
  EXPECT_TRUE(rig.state().anchor_valid);
  EXPECT_FLOAT_EQ(rig.state().visual_anchor.x(), 0.0F);

  static_cast<void>(rig.update(
      camera, rig_inputs_at(0.0F, QVector3D(4.0F, 0.0F, 0.0F), 1.0F / 60.0F)));
  const float lagged_x = rig.state().visual_anchor.x();
  EXPECT_GT(lagged_x, 0.0F) << "the anchor never followed the commander";
  EXPECT_LT(lagged_x, 4.0F) << "the anchor snapped instead of easing";
  EXPECT_GE(lagged_x, 4.0F - 0.31F) << "the anchor lagged further than the cap allows";

  for (int frame = 0; frame < 120; ++frame) {
    static_cast<void>(rig.update(
        camera, rig_inputs_at(0.0F, QVector3D(4.0F, 0.0F, 0.0F), 1.0F / 60.0F)));
  }
  EXPECT_NEAR(rig.state().visual_anchor.x(), 4.0F, 0.01F);
}

TEST(CommanderCameraRigFeelTest, LookVelocityIsReportedForTheFrame) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;
  const QVector3D anchor(0.0F, 0.0F, 0.0F);

  static_cast<void>(rig.update(camera, rig_inputs_at(0.0F, anchor, 1.0F / 60.0F)));
  static_cast<void>(rig.update(camera, rig_inputs_at(6.0F, anchor, 1.0F / 60.0F)));
  EXPECT_NEAR(rig.state().yaw_velocity, 360.0F, 1.0F);
  EXPECT_FLOAT_EQ(rig.state().yaw, 6.0F);

  static_cast<void>(rig.update(camera, rig_inputs_at(6.0F, anchor, 1.0F / 60.0F)));
  EXPECT_FLOAT_EQ(rig.state().yaw_velocity, 0.0F);
}

TEST(CommanderCameraRigFeelTest, YawVelocityCrossesTheSeamWithoutASpike) {
  App::Core::CommanderCameraRig rig;
  Render::GL::Camera camera;
  const QVector3D anchor(0.0F, 0.0F, 0.0F);

  static_cast<void>(rig.update(camera, rig_inputs_at(358.0F, anchor, 1.0F / 60.0F)));
  static_cast<void>(rig.update(camera, rig_inputs_at(2.0F, anchor, 1.0F / 60.0F)));
  EXPECT_NEAR(rig.state().yaw_velocity, 240.0F, 1.0F);
}

TEST(RtsCameraBookmarkTest, EnteringAndLeavingCommanderModeKeepsTheStrategicView) {
  Render::GL::Camera rts;
  rts.set_perspective(52.0F, 16.0F / 9.0F, 0.2F, 240.0F);
  rts.look_at(QVector3D(12.0F, 20.0F, -8.0F),
              QVector3D(12.0F, 0.0F, 4.0F),
              QVector3D(0.0F, 1.0F, 0.0F));

  const auto bookmark = App::Core::RtsCameraBookmark::capture(rts);
  ASSERT_TRUE(bookmark.valid);

  rts.look_at(QVector3D(0.0F, 2.0F, 0.0F),
              QVector3D(0.0F, 2.0F, 1.0F),
              QVector3D(0.0F, 1.0F, 0.0F));
  rts.set_perspective(68.0F, 16.0F / 9.0F, 0.05F, 200.0F);

  bookmark.restore(rts);
  EXPECT_NEAR(rts.get_position().x(), 12.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_position().y(), 20.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_position().z(), -8.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_target().z(), 4.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_fov(), 52.0F, 1.0e-4F);
  EXPECT_NEAR(rts.get_far(), 240.0F, 1.0e-4F);
}

TEST(RtsCameraBookmarkTest, AnEmptyBookmarkLeavesTheCameraAlone) {
  Render::GL::Camera camera;
  camera.look_at(QVector3D(3.0F, 4.0F, 5.0F),
                 QVector3D(0.0F, 0.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));

  const App::Core::RtsCameraBookmark empty;
  empty.restore(camera);

  EXPECT_NEAR(camera.get_position().x(), 3.0F, 1.0e-4F);
  EXPECT_NEAR(camera.get_position().z(), 5.0F, 1.0e-4F);
}

namespace {

class CommanderCameraCollisionTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static void
  load_flat_ground_with_props(const std::vector<Game::Map::WorldProp>& props) {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = 64;
    map_def.grid.height = 64;
    map_def.grid.tile_size = 1.0F;
    map_def.coordSystem = Game::Map::CoordSystem::World;
    map_def.biome.procedural_trees_enabled = false;
    map_def.biome.procedural_boulders_enabled = false;
    map_def.biome.procedural_iron_ore_enabled = false;
    map_def.world_props = props;
    Game::Map::TerrainService::instance().initialize(map_def);
  }

  static auto
  prop_at(Game::Map::WorldProp::Type type, float x, float z) -> Game::Map::WorldProp {
    Game::Map::WorldProp prop;
    prop.type = type;
    prop.x = x;
    prop.z = z;
    prop.scale = 1.0F;
    return prop;
  }

  static auto looking_south(const QVector3D& position,
                            bool with_terrain) -> CommanderCameraInputs {
    CommanderCameraInputs inputs;
    inputs.dt = 1.0F / 60.0F;
    inputs.view_yaw_degrees = 180.0F;
    inputs.commander_position = position;
    inputs.fight_context = FightContext::None;
    inputs.buildings = &Game::Systems::BuildingCollisionRegistry::instance();
    inputs.terrain = with_terrain ? &Game::Map::TerrainService::instance() : nullptr;
    return inputs;
  }

  static auto settled_trace(const CommanderCameraInputs& inputs)
      -> App::Core::CommanderCameraTrace {
    CommanderCameraRig rig;
    Render::GL::Camera camera;
    for (int frame = 0; frame < 120; ++frame) {
      static_cast<void>(rig.update(camera, inputs));
    }
    return rig.trace();
  }
};

} // namespace

TEST_F(CommanderCameraCollisionTest, ATallPropRetractsTheBoomAndAShortOneIsFlownOver) {
  load_flat_ground_with_props({prop_at(Game::Map::WorldProp::Type::Ruins, 0.9F, 3.1F)});
  auto const behind_ruins =
      settled_trace(looking_south(QVector3D(0.0F, 0.0F, 0.0F), true));
  EXPECT_LT(behind_ruins.boom_resolved, behind_ruins.boom_unconstrained - 0.5F)
      << "ruins stand nearly six metres tall and the boom went straight through "
         "them";
  EXPECT_TRUE(behind_ruins.sight_line_clear);

  load_flat_ground_with_props(
      {prop_at(Game::Map::WorldProp::Type::Boulder, 0.9F, 3.1F)});
  auto const behind_boulder =
      settled_trace(looking_south(QVector3D(0.0F, 0.0F, 0.0F), true));
  EXPECT_NEAR(behind_boulder.boom_resolved, behind_boulder.boom_unconstrained, 1.0e-3F)
      << "the eye rides 2.6 m up and a boulder tops out below one, so nothing "
         "about it may move the camera";
}

TEST_F(CommanderCameraCollisionTest, AFallenLogIsNotAnObstructionToAChaseLens) {
  load_flat_ground_with_props(
      {prop_at(Game::Map::WorldProp::Type::DeadTree, 0.9F, 3.1F)});
  auto const trace = settled_trace(looking_south(QVector3D(0.0F, 0.0F, 0.0F), true));
  EXPECT_NEAR(trace.boom_resolved, trace.boom_unconstrained, 1.0e-3F);
  EXPECT_TRUE(trace.sight_line_clear);
}

TEST_F(CommanderCameraCollisionTest, TheBoomRetractsAtOnceButIsReleasedInMetres) {
  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      1U, "temple", 0.0F, 0.0F, 1, 0.0F);

  CommanderCameraRig rig;
  Render::GL::Camera camera;
  auto const blocked = looking_south(QVector3D(0.0F, 0.0F, -4.0F), false);
  for (int frame = 0; frame < 120; ++frame) {
    static_cast<void>(rig.update(camera, blocked));
  }
  float const retracted = rig.trace().boom_resolved;
  ASSERT_LT(retracted, 2.0F) << "the temple never retracted the boom at all";

  Game::Systems::BuildingCollisionRegistry::instance().clear();
  auto const clear = looking_south(QVector3D(0.0F, 0.0F, -4.0F), false);
  float previous = retracted;
  float worst_step = 0.0F;
  for (int frame = 0; frame < 120; ++frame) {
    static_cast<void>(rig.update(camera, clear));
    float const boom = rig.trace().boom_resolved;
    worst_step = std::max(worst_step, boom - previous);
    previous = boom;
  }
  EXPECT_GT(previous, retracted + 1.0F) << "the boom never came back out";
  EXPECT_LT(worst_step, 0.12F)
      << "the boom extended " << worst_step
      << " m in one frame; release is damped in metres so an obstruction "
         "leaving the frame does not pop the camera";
}

TEST_F(CommanderCameraCollisionTest, AWallOnlyEverShortensTheBoomAlongItsOwnAxis) {
  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      1U, "home", 0.0F, 0.0F, 1, 0.0F);

  auto const trace = settled_trace(looking_south(QVector3D(0.0F, 0.0F, -3.0F), false));
  ASSERT_TRUE(trace.valid);
  EXPECT_LT(trace.boom_resolved, trace.boom_unconstrained - 0.5F);

  QVector3D const wanted = trace.eye_unconstrained - trace.pivot;
  QVector3D const resolved = trace.eye_resolved - trace.pivot;
  float const off_axis = (wanted.x() * resolved.z()) - (wanted.z() * resolved.x());
  EXPECT_NEAR(off_axis, 0.0F, 1.0e-3F)
      << "the eye left the boom axis, so something pushed it sideways instead of "
         "pulling it in toward the commander";
}

TEST_F(CommanderCameraCollisionTest, ABuriedPivotNeverPushesTheLensAwayFromTheBody) {
  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      1U, "temple", 0.0F, 0.0F, 1, 0.0F);

  auto const trace = settled_trace(looking_south(QVector3D(0.0F, 0.0F, 0.0F), false));
  ASSERT_TRUE(trace.valid);
  EXPECT_NEAR(trace.boom_clear_fraction, 0.0F, 1.0e-4F)
      << "the commander was placed inside the temple body, so no part of the "
         "boom is clear and this test is not measuring what it means to";

  float const planar = std::hypot(trace.eye_resolved.x() - trace.pivot.x(),
                                  trace.eye_resolved.z() - trace.pivot.z());
  EXPECT_LT(planar, 0.75F)
      << "depenetration carried the lens out to the nearest temple wall, which "
         "is where it used to lose the commander entirely";
}
