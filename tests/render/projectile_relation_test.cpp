#include <gtest/gtest.h>

#include "render/geom/projectile_renderer.h"

using Render::GL::classify_projectile_relation;
using Render::GL::ProjectileRelation;
using Render::GL::ProjectileViewContext;

TEST(FireballPresentationTest, ImpactKeepsItsFlashAndFadesToZeroBeforeRetirement) {
  using Render::GL::fireball_impact_envelope;
  EXPECT_FLOAT_EQ(fireball_impact_envelope(-0.1F), 1.0F);
  EXPECT_FLOAT_EQ(fireball_impact_envelope(0.0F), 1.0F);
  EXPECT_FLOAT_EQ(fireball_impact_envelope(0.25F), 1.0F);
  EXPECT_FLOAT_EQ(fireball_impact_envelope(1.0F), 0.0F);
  EXPECT_FLOAT_EQ(fireball_impact_envelope(1.1F), 0.0F);
  float previous = 1.0F;
  for (int frame = 1; frame <= 120; ++frame) {
    float const current = fireball_impact_envelope(frame / 120.0F);
    EXPECT_GE(current, 0.0F);
    EXPECT_LE(current, previous);
    EXPECT_LT(previous - current, 0.02F);
    previous = current;
  }
  EXPECT_LT(fireball_impact_envelope(0.99F), 0.001F);
}

TEST(ProjectileRelationTest, ArrowsShotByTheLocalPlayerAreOutgoing) {
  EXPECT_EQ(classify_projectile_relation(1, 1, 2), ProjectileRelation::Outgoing);
  EXPECT_EQ(classify_projectile_relation(1, 1, 0), ProjectileRelation::Outgoing);
}

TEST(ProjectileRelationTest, ArrowsAimedAtTheLocalPlayerAreIncoming) {
  EXPECT_EQ(classify_projectile_relation(1, 2, 1), ProjectileRelation::Incoming);
}

TEST(ProjectileRelationTest, EverythingElseIsNeutral) {
  EXPECT_EQ(classify_projectile_relation(1, 2, 3), ProjectileRelation::Neutral);
  EXPECT_EQ(classify_projectile_relation(1, 0, 0), ProjectileRelation::Neutral);
  EXPECT_EQ(classify_projectile_relation(0, 1, 2), ProjectileRelation::Neutral)
      << "spectators have no side";
}

TEST(ProjectileRelationTest, TheViewContextClassifiesOwnersResolvedAtPublishTime) {
  ProjectileViewContext view;
  view.local_owner_id = 1;

  EXPECT_EQ(view.relation_for_owners(1, 2), ProjectileRelation::Outgoing);
  EXPECT_EQ(view.relation_for_owners(2, 1), ProjectileRelation::Incoming);
  EXPECT_EQ(view.relation_for_owners(2, 3), ProjectileRelation::Neutral);
  EXPECT_EQ(view.relation_for_owners(0, 0), ProjectileRelation::Neutral)
      << "an unowned pair belongs to nobody";
}

TEST(ProjectileRelationTest, ASpectatorHasNoSide) {
  ProjectileViewContext spectator;
  spectator.local_owner_id = 0;
  EXPECT_EQ(spectator.relation_for_owners(1, 2), ProjectileRelation::Neutral)
      << "with no local owner the renderer must not guess";
}
