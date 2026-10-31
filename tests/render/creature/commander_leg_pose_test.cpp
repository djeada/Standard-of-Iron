#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/humanoid/asset/humanoid_manifest.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace {

using Render::Humanoid::BakeProfile;
using Render::Humanoid::HumanoidBone;

auto origin(const std::vector<QMatrix4x4>& palette, HumanoidBone bone) -> QVector3D {
  return palette[static_cast<std::size_t>(bone)].column(3).toVector3D();
}

struct LegOffset {
  float behind{0.0F};
  float sideways{0.0F};
};

auto leg_offset(const std::vector<QMatrix4x4>& palette, bool left) -> LegOffset {

  QVector3D const right(1.0F, 0.0F, 0.0F);
  QVector3D const forward(0.0F, 0.0F, 1.0F);
  QVector3D const hip = origin(palette, left ? HumanoidBone::HipL : HumanoidBone::HipR);
  QVector3D const knee =
      origin(palette, left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  QVector3D const foot =
      origin(palette, left ? HumanoidBone::FootL : HumanoidBone::FootR);
  QVector3D const line = foot - hip;
  float const t = std::clamp(QVector3D::dotProduct(knee - hip, line) /
                                 std::max(1.0e-6F, line.lengthSquared()),
                             0.0F,
                             1.0F);
  QVector3D const off = knee - (hip + line * t);
  return {.behind = -QVector3D::dotProduct(off, forward),
          .sideways = std::abs(QVector3D::dotProduct(off, right))};
}

} // namespace

TEST(HumanoidLegPose, HeldPosesKeepBothSegmentsConnectedToTheRigHips) {
  using HP = Render::GL::HumanProportions;
  for (auto const profile : Render::Humanoid::humanoid_bake_profiles()) {
    auto const& recipe = Render::Humanoid::humanoid_bake_recipe(profile);
    for (std::size_t c = 0; c < recipe.clips.size(); ++c) {
      auto const& clip = recipe.clips[c];
      if (!clip.name.starts_with("hold")) {
        continue;
      }
      SCOPED_TRACE(std::string(clip.name));
      SCOPED_TRACE(static_cast<int>(profile));
      for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
        SCOPED_TRACE(f);
        std::vector<QMatrix4x4> palette;
        recipe.bake_clip_frame(c, f, palette, nullptr);
        ASSERT_GE(palette.size(), Render::Humanoid::k_bone_count);
        for (bool const left : {true, false}) {
          auto const hip =
              origin(palette, left ? HumanoidBone::HipL : HumanoidBone::HipR);
          auto const knee =
              origin(palette, left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
          auto const foot =
              origin(palette, left ? HumanoidBone::FootL : HumanoidBone::FootR);
          EXPECT_NEAR(
              (knee - hip).length(), HP::UPPER_LEG_LEN, HP::UPPER_LEG_LEN * 0.031F);
          EXPECT_NEAR(
              (foot - knee).length(), HP::LOWER_LEG_LEN, HP::LOWER_LEG_LEN * 0.031F);
        }
      }
    }
  }
}

TEST(CommanderLegPose, LungesBendTheKneesForwardOverTheToes) {
  constexpr float k_max_behind = 0.03F;
  constexpr float k_max_sideways = 0.16F;
  for (auto const profile : {BakeProfile::SwordReady, BakeProfile::SpearReady}) {
    auto const& recipe = Render::Humanoid::humanoid_bake_recipe(profile);
    ASSERT_TRUE(recipe.complete());
    for (std::size_t c = 0; c < recipe.clips.size(); ++c) {
      auto const& clip = recipe.clips[c];
      if (!clip.name.starts_with("rpg_sword_") &&
          !clip.name.starts_with("rpg_spear_")) {
        continue;
      }
      SCOPED_TRACE(std::string(clip.name));
      float worst_behind = 0.0F;
      float worst_sideways = 0.0F;
      for (std::uint32_t f = 0; f < clip.frame_count; ++f) {

        std::vector<QMatrix4x4> palette;
        recipe.bake_clip_frame(c, f, palette, nullptr);
        ASSERT_GE(palette.size(), Render::Humanoid::k_bone_count);
        for (bool const left : {true, false}) {
          auto const leg = leg_offset(palette, left);
          worst_behind = std::max(worst_behind, leg.behind);
          worst_sideways = std::max(worst_sideways, leg.sideways);
        }
      }
      EXPECT_LE(worst_behind, k_max_behind) << "a knee bends backwards";
      EXPECT_LE(worst_sideways, k_max_sideways) << "a knee splays out sideways";
    }
  }
}
