
#include <QMatrix4x4>

#include <gtest/gtest.h>

#include "render/creature/pipeline/creature_bone_probe.h"

namespace {

using Render::Creature::Pipeline::active_bone_probe;
using Render::Creature::Pipeline::BoneProbe;
using Render::Creature::Pipeline::ScopedBoneProbe;
using Render::Creature::Pipeline::set_active_bone_probe;

TEST(CreatureBoneProbe, NoProbeIsInstalledByDefault) {
  EXPECT_EQ(active_bone_probe(), nullptr);
}

TEST(CreatureBoneProbe, ScopeInstallsAndClears) {
  BoneProbe probe{};
  {
    ScopedBoneProbe const scope(&probe);
    EXPECT_EQ(active_bone_probe(), &probe);
  }
  EXPECT_EQ(active_bone_probe(), nullptr)
      << "a probe that outlives its scope would be a dangling pointer the "
         "pipeline writes through on the next frame";
}

TEST(CreatureBoneProbe, NestedScopesRestoreTheOuterProbe) {
  BoneProbe outer{};
  BoneProbe inner{};

  ScopedBoneProbe const outer_scope(&outer);
  {
    ScopedBoneProbe const inner_scope(&inner);
    EXPECT_EQ(active_bone_probe(), &inner);
  }
  EXPECT_EQ(active_bone_probe(), &outer);
}

TEST(CreatureBoneProbe, ScopeSurvivesAnEarlyManualClear) {
  BoneProbe probe{};
  {
    ScopedBoneProbe const scope(&probe);
    set_active_bone_probe(nullptr);
    EXPECT_EQ(active_bone_probe(), nullptr);
  }
  EXPECT_EQ(active_bone_probe(), nullptr);
}

TEST(CreatureBoneProbe, StartsUnresolvedWithAnIdentityTransform) {
  BoneProbe const probe{};
  EXPECT_FALSE(probe.resolved);
  EXPECT_EQ(probe.entity_id, 0U);
  EXPECT_EQ(probe.instance_index, 0U);
  EXPECT_TRUE(probe.world.isIdentity());
}

} // namespace
