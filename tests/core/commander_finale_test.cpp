#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_finale.h"

using App::Core::CommanderFinale;

namespace {

const QVector3D k_ground(10.0F, 2.0F, -4.0F);
// A chase view: a few metres behind and above, looking past the commander.
const QVector3D k_eye(10.9F, 4.4F, -7.3F);
const QVector3D k_target(9.6F, 2.9F, 1.6F);

auto planar_distance(const QVector3D& a, const QVector3D& b) -> float {
  return std::hypot(a.x() - b.x(), a.z() - b.z());
}

} // namespace

TEST(CommanderFinale, NothingRunsUntilItIsBegun) {
  CommanderFinale finale;
  EXPECT_FALSE(finale.active());
  EXPECT_FALSE(finale.expired());
  finale.advance(10.0F);
  EXPECT_FALSE(finale.active());
  EXPECT_FALSE(finale.take_hand_back());
}

TEST(CommanderFinale, TheTakeOverStartsFromThePlayersOwnView) {
  CommanderFinale finale;
  finale.begin(CommanderFinale::Kind::Fallen, k_ground, k_eye, k_target);
  auto const pose = finale.pose();
  EXPECT_LT((pose.eye - k_eye).length(), 1.0e-3F)
      << "the first frame of the beat must be the frame the player was looking at";
  EXPECT_LT((pose.target - k_target).length(), 1.0e-3F);
}

TEST(CommanderFinale, TheLensDrawsBackRisesAndSettlesOnTheCommander) {
  CommanderFinale finale;
  finale.begin(CommanderFinale::Kind::Fallen, k_ground, k_eye, k_target);
  float const start_radius = planar_distance(k_eye, k_ground);
  finale.advance(CommanderFinale::k_duration_seconds);
  auto const pose = finale.pose();

  EXPECT_GT(planar_distance(pose.eye, k_ground), start_radius * 1.3F);
  EXPECT_GT(pose.eye.y(), k_eye.y() + 1.0F);
  EXPECT_LT(planar_distance(pose.target, k_ground), 0.05F)
      << "the last frame looks at the man, not at where the player was aiming";
  EXPECT_LT(pose.target.y(), k_ground.y() + 0.8F)
      << "a fallen commander lies on the ground; the lens looks down at him";
}

TEST(CommanderFinale, AStandingCommanderIsFramedAtChestHeight) {
  CommanderFinale finale;
  finale.begin(CommanderFinale::Kind::Victory, k_ground, k_eye, k_target);
  finale.advance(CommanderFinale::k_duration_seconds);
  EXPECT_GT(finale.pose().target.y(), k_ground.y() + 1.0F);
}

TEST(CommanderFinale, TheTurnIsContinuousAtAnyFrameRate) {
  for (float const frame_rate : {24.0F, 60.0F, 165.0F}) {
    CommanderFinale finale;
    finale.begin(CommanderFinale::Kind::Fallen, k_ground, k_eye, k_target);
    float const dt = 1.0F / frame_rate;
    QVector3D previous = finale.pose().eye;
    float fastest = 0.0F;
    while (!finale.expired()) {
      finale.advance(dt);
      QVector3D const eye = finale.pose().eye;
      fastest = std::max(fastest, (eye - previous).length() / dt);
      previous = eye;
    }
    EXPECT_LT(fastest, 4.5F) << "the lens moved at " << fastest << " m/s at "
                             << frame_rate << " fps; that reads as a cut";
  }
}

TEST(CommanderFinale, TheSameElapsedTimeGivesTheSamePoseHoweverItWasStepped) {
  CommanderFinale coarse;
  CommanderFinale fine;
  coarse.begin(CommanderFinale::Kind::Victory, k_ground, k_eye, k_target);
  fine.begin(CommanderFinale::Kind::Victory, k_ground, k_eye, k_target);
  coarse.advance(1.5F);
  for (int step = 0; step < 150; ++step) {
    fine.advance(0.01F);
  }
  EXPECT_LT((coarse.pose().eye - fine.pose().eye).length(), 1.0e-2F);
}

TEST(CommanderFinale, ItHandsBackExactlyOnceAndOnlyWhenItHasRunItsCourse) {
  CommanderFinale finale;
  finale.begin(CommanderFinale::Kind::Defeat, k_ground, k_eye, k_target);
  finale.advance(CommanderFinale::k_duration_seconds * 0.5F);
  EXPECT_FALSE(finale.take_hand_back());
  finale.advance(CommanderFinale::k_duration_seconds);
  EXPECT_TRUE(finale.expired());
  EXPECT_TRUE(finale.take_hand_back());
  EXPECT_FALSE(finale.take_hand_back())
      << "a second hand-back would leave commander mode twice";
  EXPECT_TRUE(finale.active()) << "it stays up until the view model ends it";
  finale.end();
  EXPECT_FALSE(finale.active());
}

TEST(CommanderFinale, ALensStraightAboveTheCommanderStillGetsAnOrbit) {
  CommanderFinale finale;
  finale.begin(CommanderFinale::Kind::Fallen,
               k_ground,
               k_ground + QVector3D(0.0F, 6.0F, 0.0F),
               k_ground);
  finale.advance(CommanderFinale::k_duration_seconds);
  auto const pose = finale.pose();
  EXPECT_TRUE(std::isfinite(pose.eye.x()) && std::isfinite(pose.eye.z()));
  EXPECT_GT(planar_distance(pose.eye, k_ground), 2.0F)
      << "a view looking straight down has to open out, not spin on the spot";
}
