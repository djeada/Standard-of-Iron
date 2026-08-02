#include <cmath>
#include <gtest/gtest.h>
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

} // namespace

// A promo author writes yaw as a compass bearing, so keys either side of north
// wrap. Blending them numerically sweeps the camera the long way around the
// battle -- three hundred degrees of orbit where sixteen were meant.
TEST(ArenaPromoSpecTest, YawBlendsAlongTheShorterArc) {
  const std::vector<CameraKey> keys{key(0.0F, 8.0F), key(1.0F, 352.0F)};

  const float midpoint = Arena::Promo::evaluate(keys, 0.5F).yaw;
  EXPECT_NEAR(std::fmod(midpoint + 360.0F, 360.0F), 0.0F, 0.01F);

  // A quarter of the way in, the camera has moved a quarter of sixteen
  // degrees, not a quarter of the way round the compass.
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
  // Ordered by start time, whatever order they were authored in: the recorder
  // plays a pass forwards through one continuous simulation.
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

// Two angles on the same moment are a normal thing to author, and one run
// cannot record both: the second has to get a run of its own.
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
