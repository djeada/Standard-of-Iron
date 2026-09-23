#include <cmath>
#include <gtest/gtest.h>

#include "animation/clip_manifest.h"
#include "render/entity/siege_crew.h"

using Render::GL::SiegeCrewFrame;
using Render::GL::SiegeCrewMode;
using Render::GL::SiegeCrewState;

namespace {

constexpr float k_step = 1.0F / 30.0F;

auto run(SiegeCrewState& state,
         SiegeCrewFrame frame,
         float from,
         float seconds,
         float speed = 0.0F) -> float {
  float time = from;
  for (float t = 0.0F; t < seconds; t += k_step) {
    time += k_step;
    frame.travelled += speed * k_step;
    Render::GL::advance_siege_crew(state, frame, time);
  }
  return time;
}

auto parked(bool ballista) -> SiegeCrewFrame {
  SiegeCrewFrame frame{};
  frame.ballista = ballista;
  frame.engine_scale = ballista ? 1.35F : 1.44F;
  return frame;
}

} // namespace

TEST(SiegeCrewTest, AParkedEngineKeepsItsCrewAtRest) {
  SiegeCrewState state;
  run(state, parked(false), 0.0F, 1.0F);
  EXPECT_EQ(state.mode, SiegeCrewMode::Rest);
  for (std::size_t i = 0; i < Render::GL::siege_crew_size(false); ++i) {
    EXPECT_TRUE(state.members[i].placed);
    EXPECT_NE(state.members[i].clip, Animation::k_humanoid_crew_push_clip);
  }
  EXPECT_EQ(Render::GL::siege_crew_size(true), 3U);
}

TEST(SiegeCrewTest, AMovingEngineIsPushedFromBehindWithStridesThatFollowTheGround) {
  SiegeCrewState state;
  float time = run(state, parked(false), 0.0F, 1.0F);
  SiegeCrewFrame moving = parked(false);
  moving.movement = 0.8F;
  time = run(state, moving, time, 4.0F, 1.0F);
  ASSERT_EQ(state.mode, SiegeCrewMode::Push);
  for (std::size_t i = 0; i < Render::GL::siege_crew_size(false); ++i) {
    const auto& member = state.members[i];
    EXPECT_EQ(member.clip, Animation::k_humanoid_crew_push_clip) << "member " << i;
    EXPECT_LT(member.z, -0.40F) << "member " << i << " pushes from behind";
    EXPECT_LT(std::abs(std::remainder(member.yaw, 6.2831853F)), 0.05F)
        << "member " << i << " faces the way the engine rolls";
  }
  moving.travelled = 10.0F;
  Render::GL::advance_siege_crew(state, moving, time + k_step);
  const float at_ten = state.members[0].phase;
  Render::GL::advance_siege_crew(state, moving, time + 2.0F * k_step);
  EXPECT_FLOAT_EQ(state.members[0].phase, at_ten)
      << "no ground covered, no stride: the feet must not slide";
  moving.travelled = 10.15F;
  Render::GL::advance_siege_crew(state, moving, time + 3.0F * k_step);
  const float advanced = std::fmod(state.members[0].phase - at_ten + 1.0F, 1.0F);
  EXPECT_NEAR(advanced, 0.25F, 1.0e-3F)
      << "0.15 m is a quarter of a 0.60 m push stride";
}

TEST(SiegeCrewTest, AShotIsLoadedByCrankersAndAHeaverTiedToTheLoadingProgress) {
  SiegeCrewState state;
  float time = run(state, parked(false), 0.0F, 1.0F);
  SiegeCrewFrame loading = parked(false);
  loading.loading = true;
  loading.loading_time = 0.4F;
  loading.loading_progress = 0.2F;
  time = run(state, loading, time, 4.0F);
  ASSERT_EQ(state.mode, SiegeCrewMode::Load);
  EXPECT_EQ(state.members[0].clip, Animation::k_humanoid_crew_crank_clip);
  EXPECT_EQ(state.members[1].clip, Animation::k_humanoid_crew_crank_clip);
  EXPECT_EQ(state.members[2].clip, Animation::k_humanoid_crew_heave_clip);
  EXPECT_NEAR(state.members[2].phase, 0.2F, 1.0e-4F);

  loading.loading = false;
  time = run(state, loading, time, 1.0F);
  EXPECT_EQ(state.mode, SiegeCrewMode::Load)
      << "the pause between shots does not send the crew back to rest";
  run(state, loading, time, 3.0F);
  EXPECT_EQ(state.mode, SiegeCrewMode::Rest);
}

TEST(SiegeCrewTest, ChangingJobsBlendsTheClipAndTurnsGradually) {
  SiegeCrewState state;
  float time = run(state, parked(true), 0.0F, 1.0F);
  SiegeCrewFrame moving = parked(true);
  moving.movement = 0.8F;
  const float yaw_before = state.members[0].yaw;
  Render::GL::advance_siege_crew(state, moving, time + k_step);
  const auto& member = state.members[0];
  EXPECT_GT(member.blend, 0.5F) << "the outgoing clip must fade, not cut";
  EXPECT_NE(member.previous_clip, 0xFFFFU);
  const float turned = std::abs(std::remainder(member.yaw - yaw_before, 6.2831853F));
  EXPECT_LT(turned, 0.5F) << "one frame must not snap the crewman round";
}
