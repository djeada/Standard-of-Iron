#include <gtest/gtest.h>

#include "render/geom/projectile_renderer.h"

using Render::GL::classify_projectile_relation;
using Render::GL::ProjectileRelation;
using Render::GL::ProjectileViewContext;

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
