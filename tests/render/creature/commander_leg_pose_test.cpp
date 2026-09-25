#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

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

// The knee's offset from the straight hip-to-foot line: how far it folds
// behind that line (a knee bent backwards) and how far it swings out to the
// side of it (a frog-legged squat).
auto leg_offset(const std::vector<QMatrix4x4>& palette, bool left) -> LegOffset {
  // Clips are baked facing +Z with +X to the right. The skeleton's hips are
  // placed from the knees, so their line yaws with a lunge and is no frame to
  // judge the knees in.
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

// The direct-control commander's sword and spear moves drop the pelvis into a
// deep lunge. restore_leg_lengths used to take the knee's bend direction from
// the authored knee's offset off the hip-to-foot line, which in a lunge is a
// few centimetres of noise: normalised, it threw the knee 0.3 m out to the
// side or folded it backwards. The commander duel read as broken legs.
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
        // The recipe hands back each bone's model-space transform; the
        // baker divides out the bind pose only when it writes the file.
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
