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

TEST(ProjectileRelationTest, TheViewContextResolvesOwnersThroughTheCallback) {
  ProjectileViewContext view;
  view.local_owner_id = 1;
  view.owner_of = [](std::uint64_t id) -> int {
    return id == 10 ? 1 : 2;
  };
  EXPECT_EQ(view.relation_for(10, 20), ProjectileRelation::Outgoing);
  EXPECT_EQ(view.relation_for(20, 10), ProjectileRelation::Incoming);
  EXPECT_EQ(view.relation_for(20, 30), ProjectileRelation::Neutral);
  EXPECT_EQ(view.relation_for(0, 0), ProjectileRelation::Neutral);

  ProjectileViewContext blind;
  blind.local_owner_id = 1;
  EXPECT_EQ(blind.relation_for(10, 20), ProjectileRelation::Neutral)
      << "without an owner resolver the renderer must not guess";
}
