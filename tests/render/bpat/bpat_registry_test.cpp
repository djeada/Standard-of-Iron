#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/humanoid/skeleton.h"
#include "tests/render/test_asset_paths.h"

#include <QCoreApplication>
#include <gtest/gtest.h>

#include <QMatrix4x4>

#include <array>
#include <string>

using namespace Render::Creature::Bpat;

TEST(BpatRegistry, LoadsAllSpecies) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto &reg = BpatRegistry::instance();
  std::size_t const loaded =
      static_cast<std::size_t>(
          reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat")) +
      static_cast<std::size_t>(
          reg.load_species(kSpeciesHorse, root + "/horse.bpat")) +
      static_cast<std::size_t>(
          reg.load_species(kSpeciesElephant, root + "/elephant.bpat")) +
      static_cast<std::size_t>(reg.load_species(kSpeciesHumanoidSword,
                                                root + "/humanoid_sword.bpat"));
  EXPECT_EQ(loaded, 4U);
  EXPECT_TRUE(reg.has_species(kSpeciesHumanoid));
  EXPECT_TRUE(reg.has_species(kSpeciesHorse));
  EXPECT_TRUE(reg.has_species(kSpeciesElephant));
  EXPECT_TRUE(reg.has_species(kSpeciesHumanoidSword));
}

TEST(BpatRegistry, LoadAllFindsAssetsWhenLaunchedFromBuildDir) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  namespace fs = std::filesystem;
  fs::path const build_dir =
      fs::path(QCoreApplication::applicationDirPath().toStdString())
          .parent_path();
  if (!fs::exists(build_dir)) {
    GTEST_SKIP() << "build directory not present";
  }

  fs::path const previous_cwd = fs::current_path();
  fs::current_path(build_dir);
  auto restore_cwd = [&previous_cwd] { fs::current_path(previous_cwd); };

  auto &reg = BpatRegistry::instance();
  const std::size_t loaded = reg.load_all("assets/creatures");
  restore_cwd();

  EXPECT_EQ(loaded, 4U);
  EXPECT_TRUE(reg.has_species(kSpeciesHumanoid));
  EXPECT_TRUE(reg.has_species(kSpeciesHorse));
  EXPECT_TRUE(reg.has_species(kSpeciesElephant));
  EXPECT_TRUE(reg.has_species(kSpeciesHumanoidSword));
}

TEST(BpatRegistry, SamplePaletteRoundTripsForEachSpecies) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(reg.load_species(kSpeciesHorse, root + "/horse.bpat"));
  ASSERT_TRUE(reg.load_species(kSpeciesElephant, root + "/elephant.bpat"));
  ASSERT_TRUE(
      reg.load_species(kSpeciesHumanoidSword, root + "/humanoid_sword.bpat"));

  for (auto const species : {kSpeciesHumanoid, kSpeciesHorse, kSpeciesElephant,
                             kSpeciesHumanoidSword}) {
    auto const *blob = reg.blob(species);
    ASSERT_NE(blob, nullptr) << "species " << species;
    ASSERT_GT(blob->clip_count(), 0U);
    auto const c = blob->clip(0);
    ASSERT_GT(c.frame_count, 0U);
    ASSERT_GT(blob->bone_count(), 0U);

    std::array<QMatrix4x4, 64> palette{};
    auto const n =
        reg.sample_palette(species, 0U, 0U, std::span<QMatrix4x4>(palette));
    EXPECT_EQ(n, blob->bone_count());

    bool any_non_identity = false;
    for (std::uint32_t i = 0U; i < n; ++i) {
      if (palette[i] != QMatrix4x4{}) {
        any_non_identity = true;
        break;
      }
    }
    EXPECT_TRUE(any_non_identity)
        << "species " << species << " produced all-identity palette";
  }
}

TEST(BpatRegistry, AttackSwordClipExistsAndDiffersFromIdle) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));

  auto const *blob = reg.blob(kSpeciesHumanoid);
  ASSERT_NE(blob, nullptr);

  constexpr std::uint16_t kAttackSwordAClip =
      Render::Creature::kHumanoidAttackSwordAClip;
  ASSERT_GT(blob->clip_count(), static_cast<std::uint32_t>(kAttackSwordAClip))
      << "humanoid.bpat is missing attack_sword_a (re-run bpat_baker)";

  auto const attack_clip = blob->clip(kAttackSwordAClip);
  ASSERT_GT(attack_clip.frame_count, 0U);
  EXPECT_FALSE(attack_clip.loops)
      << "attack_sword_a must be a non-looping clip";

  std::uint32_t const mid_frame = attack_clip.frame_count / 2U;
  std::array<QMatrix4x4, 64> attack_palette{};
  auto const n =
      reg.sample_palette(kSpeciesHumanoid, kAttackSwordAClip, mid_frame,
                         std::span<QMatrix4x4>(attack_palette));
  ASSERT_GT(n, 0U);

  std::array<QMatrix4x4, 64> idle_palette{};
  reg.sample_palette(kSpeciesHumanoid, 0U, 0U,
                     std::span<QMatrix4x4>(idle_palette));

  bool any_different = false;
  for (std::uint32_t i = 0U; i < n; ++i) {
    if (attack_palette[i] != idle_palette[i]) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different)
      << "attack_sword_a mid-frame palette must differ from idle frame 0";
}

TEST(BpatRegistry, HoldClipsBakeKneelingWeaponReadyPoses) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(kSpeciesHumanoidSword, root + "/humanoid_sword.bpat"));

  auto sample_bone = [&](std::uint32_t species_id, std::uint16_t clip_index,
                         std::uint32_t frame_in_clip, HumanoidBone bone) {
    std::array<QMatrix4x4, 64> palette{};
    auto const n = reg.sample_palette(species_id, clip_index, frame_in_clip,
                                      std::span<QMatrix4x4>(palette));
    auto const bone_index = static_cast<std::size_t>(bone);
    EXPECT_GT(n, bone_index);
    return palette[bone_index].column(3).toVector3D();
  };

  auto const *default_blob = reg.blob(kSpeciesHumanoid);
  auto const *sword_blob = reg.blob(kSpeciesHumanoidSword);
  ASSERT_NE(default_blob, nullptr);
  ASSERT_NE(sword_blob, nullptr);

  auto const idle_pelvis =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidIdleClip, 0U,
                  HumanoidBone::Pelvis);
  auto const idle_hand_r =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidIdleClip, 0U,
                  HumanoidBone::HandR);
  auto const idle_hand_l =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidIdleClip, 0U,
                  HumanoidBone::HandL);

  auto const spear_hold_frame =
      default_blob->clip(Render::Creature::kHumanoidHoldClip).frame_count - 1U;
  auto const bow_hold_frame =
      default_blob->clip(Render::Creature::kHumanoidHoldBowClip).frame_count -
      1U;
  auto const sword_hold_frame =
      sword_blob->clip(Render::Creature::kHumanoidHoldClip).frame_count - 1U;

  auto const spear_hold_pelvis =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidHoldClip,
                  spear_hold_frame, HumanoidBone::Pelvis);
  auto const spear_hold_hand_r =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidHoldClip,
                  spear_hold_frame, HumanoidBone::HandR);

  auto const bow_hold_pelvis =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidHoldBowClip,
                  bow_hold_frame, HumanoidBone::Pelvis);
  auto const bow_hold_hand_l =
      sample_bone(kSpeciesHumanoid, Render::Creature::kHumanoidHoldBowClip,
                  bow_hold_frame, HumanoidBone::HandL);

  auto const sword_idle_pelvis =
      sample_bone(kSpeciesHumanoidSword, Render::Creature::kHumanoidIdleClip,
                  0U, HumanoidBone::Pelvis);
  auto const sword_hold_pelvis =
      sample_bone(kSpeciesHumanoidSword, Render::Creature::kHumanoidHoldClip,
                  sword_hold_frame, HumanoidBone::Pelvis);

  EXPECT_LT(spear_hold_pelvis.y(), idle_pelvis.y());
  EXPECT_LT(bow_hold_pelvis.y(), idle_pelvis.y());
  EXPECT_LT(sword_hold_pelvis.y(), sword_idle_pelvis.y());

  EXPECT_GT((spear_hold_hand_r - idle_hand_r).length(), 0.05F);
  EXPECT_GT((bow_hold_hand_l - idle_hand_l).length(), 0.05F);
}

TEST(BpatRegistry, SwordHumanoidIdleDiffersFromDefaultHumanoid) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(kSpeciesHumanoidSword, root + "/humanoid_sword.bpat"));

  std::array<QMatrix4x4, 64> default_palette{};
  std::array<QMatrix4x4, 64> sword_palette{};
  auto const n = reg.sample_palette(kSpeciesHumanoid, 0U, 0U,
                                    std::span<QMatrix4x4>(default_palette));
  ASSERT_GT(n, 0U);
  ASSERT_EQ(reg.sample_palette(kSpeciesHumanoidSword, 0U, 0U,
                               std::span<QMatrix4x4>(sword_palette)),
            n);

  bool any_different = false;
  for (std::uint32_t i = 0U; i < n; ++i) {
    if (default_palette[i] != sword_palette[i]) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different)
      << "humanoid_sword idle must differ from generic humanoid idle";
}
