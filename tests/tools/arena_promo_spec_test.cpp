#include <QDir>
#include <QFile>
#include <QTemporaryDir>

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

namespace {

auto write_spec(const QTemporaryDir& dir, const char* body) -> QString {
  const QString path = QDir(dir.path()).filePath(QStringLiteral("spec.json"));
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(body);
  file.close();
  return path;
}

} // namespace

TEST(ArenaPromoSpecTest, AnAuthoredReelKeepsTheGameplayUiOffUnlessItAsks) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString path = write_spec(dir, R"({
    "id": "probe",
    "shots": [
      { "name": "played", "scenario": "arena", "duration": 2.0,
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 2.0 } ] }
    ]
  })");

  QString error;
  const auto spec = Arena::Promo::load(path, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  ASSERT_EQ(spec->shots.size(), 1U);
  EXPECT_FALSE(spec->shots[0].gameplay_ui)
      << "an authored reel is a cinematic: its scenario suppresses the overlays, "
         "and a default of on put crossed swords and damage pills over every "
         "shot of them";
}

TEST(ArenaPromoSpecTest, AReelCanTurnTheGameplayUiOnWholeOrPerShot) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString path = write_spec(dir, R"({
    "id": "probe",
    "gameplay_ui": true,
    "shots": [
      { "name": "clean", "scenario": "arena", "duration": 2.0,
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 2.0 } ] },
      { "name": "played", "scenario": "arena", "start": 4.0, "duration": 2.0,
        "gameplay_ui": false,
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 2.0 } ] }
    ]
  })");

  QString error;
  const auto spec = Arena::Promo::load(path, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  ASSERT_EQ(spec->shots.size(), 2U);
  EXPECT_TRUE(spec->gameplay_ui);
  EXPECT_TRUE(spec->shots[0].gameplay_ui)
      << "the spec-level setting is the default for every shot";
  EXPECT_FALSE(spec->shots[1].gameplay_ui)
      << "a shot may still opt out of the gameplay UI";
}

TEST(ArenaPromoSpecTest, TimeLapseIsSlowMotionWrittenTheFriendlyWayRound) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString path = write_spec(dir, R"({
    "id": "probe",
    "shots": [
      { "name": "towns", "scenario": "arena", "duration": 600.0, "time_lapse": 20,
        "focus": { "mode": "point", "point": [0, 0, 0] },
        "camera": [ { "time": 0.0, "distance": 60, "pitch": 55 },
                    { "time": 600.0, "distance": 62, "pitch": 55 } ] }
    ]
  })");

  QString error;
  const auto spec = Arena::Promo::load(path, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  ASSERT_EQ(spec->shots.size(), 1U);
  EXPECT_NEAR(spec->shots[0].slow_motion, 0.05F, 1e-5F)
      << "time_lapse 20 is slow_motion 1/20";
  EXPECT_TRUE(Arena::Promo::motion_violations(*spec).empty())
      << "a ten-minute shot shown in thirty seconds is a legal clip";
}

TEST(ArenaPromoSpecTest, TimeLapseAndSlowMotionTogetherAreRefused) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString path = write_spec(dir, R"({
    "id": "probe",
    "shots": [
      { "name": "both", "scenario": "arena", "duration": 4.0, "time_lapse": 4,
        "slow_motion": 2.0,
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 4.0 } ] }
    ]
  })");

  QString error;
  EXPECT_FALSE(Arena::Promo::load(path, &error).has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("same knob"))) << error.toStdString();
}

TEST(ArenaPromoSpecTest, CameraRatesAreJudgedInScreenSeconds) {

  auto lapse = shot("lapse", "arena", 0.0F, 30.0F);
  lapse.slow_motion = 0.05F;
  lapse.keys = {key(0.0F, 0.0F), key(30.0F, 90.0F)};
  EXPECT_TRUE(
      mentions(Arena::Promo::motion_violations(spec_of({lapse})), "swings yaw"));

  auto slow = shot("slow", "arena", 0.0F, 30.0F);
  slow.slow_motion = 2.0F;
  slow.keys = {key(0.0F, 0.0F), key(30.0F, 90.0F)};
  EXPECT_FALSE(
      mentions(Arena::Promo::motion_violations(spec_of({slow})), "swings yaw"));
}

TEST(ArenaPromoSpecTest, AShotMayStartOnAMatchEvent) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString path = write_spec(dir, R"({
    "id": "probe",
    "casting_overlay": true,
    "require_decision": true,
    "forbid_world_edge": true,
    "shots": [
      { "name": "march", "scenario": "arena", "duration": 8.0,
        "start_on": { "event": "first_wave", "side": "scipio", "offset": -3.0 },
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 8.0 } ] },
      { "name": "clash", "scenario": "arena", "duration": 6.0,
        "start_on": { "event": "first_contact" },
        "casting_overlay": false,
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 6.0 } ] }
    ]
  })");

  QString error;
  const auto spec = Arena::Promo::load(path, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  ASSERT_EQ(spec->shots.size(), 2U);
  ASSERT_TRUE(spec->shots[0].start_on.has_value());
  EXPECT_EQ(spec->shots[0].start_on->event, QStringLiteral("first_wave"));
  EXPECT_EQ(spec->shots[0].start_on->side, QStringLiteral("scipio"));
  EXPECT_FLOAT_EQ(spec->shots[0].start_on->offset_seconds, -3.0F);
  ASSERT_TRUE(spec->shots[1].start_on.has_value());
  EXPECT_TRUE(spec->shots[1].start_on->side.isEmpty());
  EXPECT_TRUE(Arena::Promo::uses_start_events(*spec));
  EXPECT_TRUE(spec->require_decision);
  EXPECT_TRUE(spec->forbid_world_edge);
  EXPECT_TRUE(spec->shots[0].casting_overlay)
      << "the spec-level casting overlay is the default for every shot";
  EXPECT_FALSE(spec->shots[1].casting_overlay) << "a shot may still opt out";
}

TEST(ArenaPromoSpecTest, AnUnknownStartEventOrAStartOnBothWaysIsRefused) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  QString error;
  EXPECT_FALSE(Arena::Promo::load(write_spec(dir, R"({
    "id": "probe",
    "shots": [
      { "name": "x", "scenario": "arena", "duration": 4.0,
        "start_on": { "event": "the_big_moment" },
        "focus": { "mode": "all" }, "camera": [ { "time": 0.0 }, { "time": 4.0 } ] }
    ]
  })"),
                                  &error)
                   .has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("unknown start_on event")))
      << error.toStdString();

  EXPECT_FALSE(Arena::Promo::load(write_spec(dir, R"({
    "id": "probe",
    "shots": [
      { "name": "x", "scenario": "arena", "start": 5.0, "duration": 4.0,
        "start_on": { "event": "decision" },
        "focus": { "mode": "all" }, "camera": [ { "time": 0.0 }, { "time": 4.0 } ] }
    ]
  })"),
                                  &error)
                   .has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("both start and start_on")))
      << error.toStdString();
}

TEST(ArenaPromoSpecTest, TheGroundFootprintKnowsWhenTheViewLeavesTheArenaFloor) {
  using Arena::Promo::Pose;
  Pose overhead;
  overhead.distance = 60.0F;
  overhead.pitch = 60.0F;
  overhead.yaw = 315.0F;
  overhead.fov = 40.0F;
  const auto tight = Arena::Promo::view_ground_footprint(overhead, {}, 16.0F / 9.0F);
  EXPECT_FALSE(tight.horizon_visible);
  EXPECT_LT(tight.max_abs_extent, 58.0F)
      << "a steep sixty-metre view of the centre stays on a 58 m floor";
  EXPECT_FALSE(Arena::Promo::frames_world_edge(tight, 58.0F));

  Pose wide = overhead;
  wide.distance = 130.0F;
  wide.pitch = 47.0F;
  const auto loose = Arena::Promo::view_ground_footprint(wide, {}, 16.0F / 9.0F);
  EXPECT_TRUE(Arena::Promo::frames_world_edge(loose, 58.0F))
      << "reaches " << loose.max_abs_extent;

  Pose flat = overhead;
  flat.pitch = 15.0F;
  const auto horizon = Arena::Promo::view_ground_footprint(flat, {}, 16.0F / 9.0F);
  EXPECT_TRUE(horizon.horizon_visible)
      << "a fifteen-degree pitch with a forty-degree lens looks over the horizon";
}

TEST(ArenaPromoSpecTest, WorldEdgeViolationsNamePointFocusShotsThatFrameTheRim) {
  auto rim = shot("rim", "arena", 0.0F, 10.0F);
  rim.focus.mode = Arena::Promo::FocusMode::Point;
  CameraKey far_key;
  far_key.time = 0.0F;
  far_key.distance = 130.0F;
  far_key.pitch = 47.0F;
  far_key.yaw = 315.0F;
  far_key.fov = 40.0F;
  rim.keys = {far_key};

  auto tight = shot("tight", "arena", 0.0F, 10.0F);
  tight.focus.mode = Arena::Promo::FocusMode::Point;
  CameraKey near_key = far_key;
  near_key.distance = 50.0F;
  near_key.pitch = 60.0F;
  tight.keys = {near_key};

  auto dynamic = shot("dynamic", "arena", 0.0F, 10.0F);
  dynamic.focus.mode = Arena::Promo::FocusMode::AllUnits;
  dynamic.keys = {far_key};

  const auto breaches =
      Arena::Promo::world_edge_violations(spec_of({rim, tight, dynamic}), 58.0F);
  EXPECT_TRUE(mentions(breaches, "rim:"));
  EXPECT_FALSE(mentions(breaches, "tight:"));
  EXPECT_FALSE(mentions(breaches, "dynamic:"))
      << "a dynamic focus cannot be judged before the match plays";
}
