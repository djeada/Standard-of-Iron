

#include "render/animation/channel_evaluator.h"
#include "render/animation/clip.h"
#include "render/animation/clips/archer_idle_clip.h"
#include "render/animation/state_machine.h"

#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::Animation;

namespace {

constexpr float kEps = 1e-5F;

auto make_float_clip(std::initializer_list<std::pair<float, float>> kvs)
    -> Clip<float> {
  std::vector<Keyframe<float>> keys;
  keys.reserve(kvs.size());
  for (auto const &[t, v] : kvs) {
    keys.push_back({t, v});
  }
  return Clip<float>("test", std::move(keys));
}

} // namespace

TEST(AnimationClipTest, EmptyClipReturnsDefault) {
  Clip<float> c{};
  EXPECT_NEAR(evaluate(c, 0.0F), 0.0F, kEps);
  EXPECT_NEAR(evaluate(c, 10.0F), 0.0F, kEps);
  EXPECT_EQ(c.duration(), 0.0F);
}

TEST(AnimationClipTest, SingleKeyframeClipReturnsValue) {
  auto c = make_float_clip({{0.5F, 1.25F}});
  EXPECT_NEAR(evaluate(c, 0.0F), 1.25F, kEps);
  EXPECT_NEAR(evaluate(c, 100.0F), 1.25F, kEps);
  EXPECT_EQ(c.key_count(), 1U);
}

TEST(AnimationClipTest, LerpsBetweenTwoKeys) {
  auto c = make_float_clip({{0.0F, 0.0F}, {1.0F, 10.0F}});
  EXPECT_NEAR(evaluate(c, 0.0F), 0.0F, kEps);
  EXPECT_NEAR(evaluate(c, 0.25F), 2.5F, kEps);
  EXPECT_NEAR(evaluate(c, 0.5F), 5.0F, kEps);
  EXPECT_NEAR(evaluate(c, 1.0F), 10.0F, kEps);
}

TEST(AnimationClipTest, ClampWrapModeHoldsEndpoints) {
  auto c = make_float_clip({{0.0F, 0.0F}, {1.0F, 10.0F}});
  EXPECT_NEAR(evaluate(c, -5.0F, WrapMode::Clamp), 0.0F, kEps);
  EXPECT_NEAR(evaluate(c, 5.0F, WrapMode::Clamp), 10.0F, kEps);
}

TEST(AnimationClipTest, LoopWrapModeRepeatsInTime) {
  auto c = make_float_clip({{0.0F, 0.0F}, {1.0F, 10.0F}, {2.0F, 0.0F}});

  EXPECT_NEAR(evaluate(c, 2.5F, WrapMode::Loop), 5.0F, kEps);

  EXPECT_NEAR(evaluate(c, -0.5F, WrapMode::Loop), 5.0F, kEps);
}

TEST(AnimationClipTest, SortsUnorderedKeyframes) {

  Clip<float> c("unordered", {{1.0F, 10.0F}, {0.0F, 0.0F}, {0.5F, 5.0F}});
  EXPECT_NEAR(evaluate(c, 0.0F), 0.0F, kEps);
  EXPECT_NEAR(evaluate(c, 0.5F), 5.0F, kEps);
  EXPECT_NEAR(evaluate(c, 1.0F), 10.0F, kEps);
}

TEST(AnimationClipTest, Vec3ClipLerps) {
  std::vector<Keyframe<QVector3D>> keys{
      {0.0F, QVector3D(0, 0, 0)},
      {1.0F, QVector3D(10, 20, -30)},
  };
  Clip<QVector3D> c("vec", std::move(keys));
  auto v = evaluate(c, 0.5F);
  EXPECT_NEAR(v.x(), 5.0F, kEps);
  EXPECT_NEAR(v.y(), 10.0F, kEps);
  EXPECT_NEAR(v.z(), -15.0F, kEps);
}

TEST(AnimationStateMachineTest, DefaultStateIsZero) {
  StateMachine sm;
  EXPECT_EQ(sm.current(), 0U);
  EXPECT_FALSE(sm.is_blending());
  EXPECT_NEAR(sm.weight(), 1.0F, kEps);
}

TEST(AnimationStateMachineTest, InstantTransitionHasFullWeight) {
  StateMachine sm(1);
  sm.request(2, 0.0F);
  EXPECT_EQ(sm.current(), 2U);
  EXPECT_FALSE(sm.is_blending());
  EXPECT_NEAR(sm.weight(), 1.0F, kEps);
}

TEST(AnimationStateMachineTest, BlendedTransitionRampsWeight) {
  StateMachine sm(1);
  sm.request(2, 1.0F);
  EXPECT_EQ(sm.current(), 2U);
  EXPECT_EQ(sm.previous(), 1U);
  EXPECT_TRUE(sm.is_blending());
  EXPECT_NEAR(sm.weight(), 0.0F, kEps);
  sm.tick(0.5F);
  EXPECT_NEAR(sm.weight(), 0.5F, kEps);
  sm.tick(0.5F);
  EXPECT_NEAR(sm.weight(), 1.0F, kEps);
  EXPECT_FALSE(sm.is_blending());
}

TEST(AnimationStateMachineTest, RequestSameStateIsNoOp) {
  StateMachine sm(3);
  sm.request(3, 1.0F);
  EXPECT_EQ(sm.current(), 3U);
  EXPECT_FALSE(sm.is_blending());
}

TEST(AnimationChannelEvaluatorTest, SamplesCurrentStateClip) {
  StateMachine sm(0);
  std::vector<Clip<float>> clips;
  clips.push_back(make_float_clip({{0.0F, 0.0F}, {1.0F, 10.0F}}));
  clips.push_back(make_float_clip({{0.0F, 100.0F}, {1.0F, 200.0F}}));
  ChannelEvaluator<float> ch(std::move(clips), &sm);
  EXPECT_NEAR(ch.sample(0.5F), 5.0F, kEps);
}

TEST(AnimationChannelEvaluatorTest, BlendsBetweenTwoClipsDuringTransition) {
  StateMachine sm(0);
  std::vector<Clip<float>> clips;
  clips.push_back(make_float_clip({{0.0F, 0.0F}}));
  clips.push_back(make_float_clip({{0.0F, 100.0F}}));
  ChannelEvaluator<float> ch(std::move(clips), &sm);

  sm.request(1, 1.0F);
  sm.tick(0.25F);

  EXPECT_NEAR(ch.sample(0.0F), 25.0F, kEps);

  sm.tick(0.5F);
  EXPECT_NEAR(ch.sample(0.0F), 75.0F, kEps);

  sm.tick(0.25F);
  EXPECT_NEAR(ch.sample(0.0F), 100.0F, kEps);
}

TEST(AnimationChannelEvaluatorTest, MissingClipSlotReturnsZero) {
  StateMachine sm(5);
  std::vector<Clip<float>> clips;
  clips.push_back(make_float_clip({{0.0F, 7.0F}}));
  clips.push_back(make_float_clip({{0.0F, 9.0F}}));
  ChannelEvaluator<float> ch(std::move(clips), &sm);
  EXPECT_NEAR(ch.sample(0.0F), 0.0F, kEps);
}

TEST(AnimationAuthoredClipsTest, ArcherIdleSwayIsLoopable) {
  auto c = Clips::make_archer_idle_sway_x();
  EXPECT_NEAR(c.duration(), 2.0F, kEps);
  EXPECT_GT(c.key_count(), 2U);

  auto start = evaluate(c, 0.0F, WrapMode::Loop);
  auto end = evaluate(c, 2.0F - 1e-4F, WrapMode::Loop);
  EXPECT_NEAR(start, 0.0F, 0.01F);
  EXPECT_NEAR(end, 0.0F, 0.01F);

  float peak = 0.0F;
  for (float t = 0.0F; t < 2.0F; t += 0.05F) {
    peak = std::max(peak, std::abs(evaluate(c, t, WrapMode::Loop)));
  }
  EXPECT_GT(peak, 0.015F);
}

TEST(AnimationAuthoredClipsTest, ArcherWalkSwayAmplitudeExceedsIdle) {
  auto idle = Clips::make_archer_idle_sway_x();
  auto walk = Clips::make_archer_walk_sway_x();
  float idle_peak = 0.0F;
  float walk_peak = 0.0F;
  for (float t = 0.0F; t < idle.duration(); t += 0.05F) {
    idle_peak =
        std::max(idle_peak, std::abs(evaluate(idle, t, WrapMode::Loop)));
  }
  for (float t = 0.0F; t < walk.duration(); t += 0.05F) {
    walk_peak =
        std::max(walk_peak, std::abs(evaluate(walk, t, WrapMode::Loop)));
  }
  EXPECT_GT(walk_peak, idle_peak);
}
