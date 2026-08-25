#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "tools/arena/promo_spec.h"

namespace {

using Arena::Promo::CameraKey;
using Arena::Promo::Ease;
using Arena::Promo::Shot;
using Arena::Promo::Spec;

auto key(float time, float yaw, Ease ease = Ease::Linear) -> CameraKey {
  CameraKey result;
  result.time = time;
  result.yaw = yaw;
  result.ease = ease;
  return result;
}

auto shot(const char* name,
          const char* scenario,
          float start,
          float duration,
          int seed = 1337) -> Shot {
  Shot result;
  result.name = QString::fromLatin1(name);
  result.scenario = QString::fromLatin1(scenario);
  result.start_seconds = start;
  result.duration_seconds = duration;
  result.seed = seed;
  return result;
}

auto calm_shot(const char* name, float clip_seconds) -> Shot {
  auto result = shot(name, "arena", 0.0F, clip_seconds);
  CameraKey first;
  first.time = 0.0F;
  first.yaw = 90.0F;
  first.pitch = 10.0F;
  first.fov = 40.0F;
  CameraKey last = first;
  last.time = clip_seconds;
  last.yaw = 96.0F;
  result.keys = {first, last};
  return result;
}

auto spec_of(std::vector<Shot> shots) -> Spec {
  Spec result;
  result.id = QStringLiteral("test");
  result.shots = std::move(shots);
  return result;
}

auto mentions(const std::vector<QString>& breaches, const char* needle) -> bool {
  return std::any_of(breaches.begin(), breaches.end(), [needle](const QString& line) {
    return line.contains(QString::fromLatin1(needle));
  });
}

} // namespace

TEST(ArenaPromoSpecMotionTest, CalmCameraWorkPasses) {
  EXPECT_TRUE(Arena::Promo::motion_violations(
                  spec_of({calm_shot("wide", 3.0F), calm_shot("close", 2.5F)}))
                  .empty());
}

TEST(ArenaPromoSpecMotionTest, AWhippingOrbitIsRefused) {
  auto whip = calm_shot("whip", 2.0F);
  whip.keys.back().yaw = 150.0F;

  const auto breaches = Arena::Promo::motion_violations(
      spec_of({whip, calm_shot("hold", 3.0F), calm_shot("hold_two", 3.0F)}));

  EXPECT_TRUE(mentions(breaches, "swings yaw"))
      << "a 30 deg/s orbit is the shaky-chaos camera the limits exist to stop";
}

TEST(ArenaPromoSpecMotionTest, HandheldShakeIsRefused) {
  auto shaky = calm_shot("shaky", 3.0F);
  shaky.shake = 0.09F;

  EXPECT_TRUE(mentions(Arena::Promo::motion_violations(spec_of({shaky})), "shakes at"));
}

TEST(ArenaPromoSpecMotionTest, ARollingHorizonIsRefused) {
  auto rolled = calm_shot("rolled", 2.0F);
  rolled.keys.front().roll = -6.0F;
  rolled.keys.back().roll = 6.0F;

  const auto breaches = Arena::Promo::motion_violations(spec_of({rolled}));

  EXPECT_TRUE(mentions(breaches, "rolls the horizon"));
  EXPECT_TRUE(mentions(breaches, "swings roll"))
      << "a horizon that tips one way and back inside two seconds reads as a stumble";
}

TEST(ArenaPromoSpecMotionTest, FlashCuttingIsRefused) {
  const auto breaches = Arena::Promo::motion_violations(
      spec_of({calm_shot("a", 0.9F), calm_shot("b", 0.9F), calm_shot("c", 0.9F)}));

  EXPECT_TRUE(mentions(breaches, "is on screen for"));
  EXPECT_TRUE(mentions(breaches, "averages"));
}

TEST(ArenaPromoSpecMotionTest, ACrashZoomIsRefused) {
  auto zoom = calm_shot("zoom", 1.6F);
  zoom.keys.front().fov = 54.0F;
  zoom.keys.back().fov = 36.0F;

  EXPECT_TRUE(mentions(
      Arena::Promo::motion_violations(spec_of({zoom, calm_shot("hold", 3.0F)})),
      "swings fov"));
}

TEST(ArenaPromoSpecTest, YawBlendsAlongTheShorterArc) {
  const std::vector<CameraKey> keys{key(0.0F, 8.0F), key(1.0F, 352.0F)};

  const float midpoint = Arena::Promo::evaluate(keys, 0.5F).yaw;
  EXPECT_NEAR(std::fmod(midpoint + 360.0F, 360.0F), 0.0F, 0.01F);

  EXPECT_NEAR(Arena::Promo::evaluate(keys, 0.25F).yaw, 4.0F, 0.01F);
}

TEST(ArenaPromoSpecTest, YawStillTakesAuthoredMiddleKeysTheLongWay) {
  const std::vector<CameraKey> keys{
      key(0.0F, 0.0F), key(1.0F, 170.0F), key(2.0F, 340.0F)};
  EXPECT_NEAR(Arena::Promo::evaluate(keys, 1.0F).yaw, 170.0F, 0.01F);
  EXPECT_NEAR(Arena::Promo::evaluate(keys, 2.0F).yaw, 340.0F, 0.01F);
}

TEST(ArenaPromoSpecTest, ShotsOverOneScenarioRecordInASinglePass) {
  Spec spec;
  spec.shots = {shot("late", "battle", 30.0F, 3.0F),
                shot("early", "battle", 4.0F, 2.0F),
                shot("middle", "battle", 12.0F, 2.0F)};

  const auto passes = Arena::Promo::plan_passes(spec);
  ASSERT_EQ(passes.size(), 1U);
  EXPECT_EQ(passes[0].scenario, QStringLiteral("battle"));

  EXPECT_EQ(passes[0].shots, (std::vector<std::size_t>{1, 2, 0}));
}

TEST(ArenaPromoSpecTest, DifferentScenariosOrSeedsNeedTheirOwnPass) {
  Spec spec;
  spec.shots = {shot("a", "battle", 4.0F, 2.0F),
                shot("b", "siege", 4.0F, 2.0F),
                shot("c", "battle", 20.0F, 2.0F, 99)};

  const auto passes = Arena::Promo::plan_passes(spec);
  ASSERT_EQ(passes.size(), 3U);
  EXPECT_EQ(passes[0].shots, (std::vector<std::size_t>{0}));
  EXPECT_EQ(passes[1].shots, (std::vector<std::size_t>{1}));
  EXPECT_EQ(passes[2].seed, 99);
}

TEST(ArenaPromoSpecTest, OverlappingWindowsSplitIntoSeparatePasses) {
  Spec spec;
  spec.shots = {shot("wide", "battle", 10.0F, 4.0F),
                shot("close", "battle", 12.0F, 4.0F),
                shot("after", "battle", 20.0F, 2.0F)};

  const auto passes = Arena::Promo::plan_passes(spec);
  ASSERT_EQ(passes.size(), 2U);
  EXPECT_EQ(passes[0].shots, (std::vector<std::size_t>{0, 2}));
  EXPECT_EQ(passes[1].shots, (std::vector<std::size_t>{1}));
}
