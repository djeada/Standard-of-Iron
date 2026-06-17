#include <QCoreApplication>
#include <QMatrix4x4>

#include <array>
#include <gtest/gtest.h>
#include <string>

#include "animation/clip_manifest.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/entity/mounted_knight_pose.h"
#include "render/horse/horse_motion.h"
#include "render/humanoid/skeleton.h"
#include "tests/render/test_asset_paths.h"

using namespace Render::Creature::Bpat;

TEST(BpatRegistry, LoadsAllSpecies) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  std::size_t const loaded =
      static_cast<std::size_t>(
          reg.load_species(k_species_humanoid, root + "/humanoid.bpat")) +
      static_cast<std::size_t>(
          reg.load_species(k_species_horse, root + "/horse.bpat")) +
      static_cast<std::size_t>(
          reg.load_species(k_species_elephant, root + "/elephant.bpat")) +
      static_cast<std::size_t>(
          reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat")) +
      static_cast<std::size_t>(
          reg.load_species(k_species_humanoid_spear, root + "/humanoid_spear.bpat")) +
      static_cast<std::size_t>(reg.load_species(k_species_humanoid_skeleton,
                                                root + "/humanoid_skeleton.bpat"));
  EXPECT_EQ(loaded, k_species_count);
  EXPECT_TRUE(reg.has_species(k_species_humanoid));
  EXPECT_TRUE(reg.has_species(k_species_horse));
  EXPECT_TRUE(reg.has_species(k_species_elephant));
  EXPECT_TRUE(reg.has_species(k_species_humanoid_sword));
  EXPECT_TRUE(reg.has_species(k_species_humanoid_spear));
  EXPECT_TRUE(reg.has_species(k_species_humanoid_skeleton));
}

TEST(BpatRegistry, LoadAllFindsAssetsWhenLaunchedFromBuildDir) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  namespace fs = std::filesystem;
  fs::path const build_dir =
      fs::path(QCoreApplication::applicationDirPath().toStdString()).parent_path();
  if (!fs::exists(build_dir)) {
    GTEST_SKIP() << "build directory not present";
  }

  fs::path const previous_cwd = fs::current_path();
  fs::current_path(build_dir);
  auto restore_cwd = [&previous_cwd] {
    fs::current_path(previous_cwd);
  };

  auto& reg = BpatRegistry::instance();
  const std::size_t loaded = reg.load_all("assets/creatures");
  restore_cwd();

  EXPECT_EQ(loaded, k_species_count);
  EXPECT_TRUE(reg.has_species(k_species_humanoid));
  EXPECT_TRUE(reg.has_species(k_species_horse));
  EXPECT_TRUE(reg.has_species(k_species_elephant));
  EXPECT_TRUE(reg.has_species(k_species_humanoid_sword));
  EXPECT_TRUE(reg.has_species(k_species_humanoid_spear));
  EXPECT_TRUE(reg.has_species(k_species_humanoid_skeleton));
}

TEST(BpatRegistry, SamplePaletteRoundTripsForEachSpecies) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(reg.load_species(k_species_horse, root + "/horse.bpat"));
  ASSERT_TRUE(reg.load_species(k_species_elephant, root + "/elephant.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_spear, root + "/humanoid_spear.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_skeleton, root + "/humanoid_skeleton.bpat"));

  for (auto const species : {k_species_humanoid,
                             k_species_horse,
                             k_species_elephant,
                             k_species_humanoid_sword,
                             k_species_humanoid_spear,
                             k_species_humanoid_skeleton}) {
    auto const* blob = reg.blob(species);
    ASSERT_NE(blob, nullptr) << "species " << species;
    ASSERT_GT(blob->clip_count(), 0U);
    auto const c = blob->clip(0);
    ASSERT_GT(c.frame_count, 0U);
    ASSERT_GT(blob->bone_count(), 0U);

    std::array<QMatrix4x4, 64> palette{};
    auto const n = reg.sample_palette(species, 0U, 0U, std::span<QMatrix4x4>(palette));
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

TEST(BpatRegistry, HumanoidWalkClipsAnimateLowerBodyForEveryHumanoidSpecies) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();

  struct SpeciesFile {
    std::uint32_t id;
    const char* file;
  };
  constexpr std::array<SpeciesFile, 4> k_humanoid_species{{
      {k_species_humanoid, "humanoid.bpat"},
      {k_species_humanoid_sword, "humanoid_sword.bpat"},
      {k_species_humanoid_spear, "humanoid_spear.bpat"},
      {k_species_humanoid_skeleton, "humanoid_skeleton.bpat"},
  }};
  constexpr std::array<HumanoidBone, 7> k_lower_body_bones{{
      HumanoidBone::Pelvis,
      HumanoidBone::HipL,
      HumanoidBone::KneeL,
      HumanoidBone::FootL,
      HumanoidBone::HipR,
      HumanoidBone::KneeR,
      HumanoidBone::FootR,
  }};

  for (auto const species : k_humanoid_species) {
    ASSERT_TRUE(reg.load_species(species.id, root + "/" + species.file))
        << species.file;
    auto const* blob = reg.blob(species.id);
    ASSERT_NE(blob, nullptr) << species.file;
    ASSERT_GT(blob->clip_count(),
              static_cast<std::uint32_t>(Render::Creature::k_humanoid_walk_clip))
        << species.file;

    auto const walk_clip = blob->clip(Render::Creature::k_humanoid_walk_clip);
    ASSERT_GT(walk_clip.frame_count, 1U) << species.file;

    std::array<QMatrix4x4, 64> first_palette{};
    auto const n = reg.sample_palette(species.id,
                                      Render::Creature::k_humanoid_walk_clip,
                                      0U,
                                      std::span<QMatrix4x4>(first_palette));
    ASSERT_GE(n, static_cast<std::uint32_t>(HumanoidBone::Count)) << species.file;

    bool lower_body_moves = false;
    for (std::uint32_t frame = 1U; frame < walk_clip.frame_count; ++frame) {
      std::array<QMatrix4x4, 64> frame_palette{};
      ASSERT_EQ(reg.sample_palette(species.id,
                                   Render::Creature::k_humanoid_walk_clip,
                                   frame,
                                   std::span<QMatrix4x4>(frame_palette)),
                n)
          << species.file << " frame " << frame;
      for (auto const bone : k_lower_body_bones) {
        auto const index = static_cast<std::size_t>(bone);
        if (frame_palette[index] != first_palette[index]) {
          lower_body_moves = true;
          break;
        }
      }
      if (lower_body_moves) {
        break;
      }
    }

    EXPECT_TRUE(lower_body_moves)
        << species.file << " walk clip has static lower-body bones";
  }
}

TEST(BpatRegistry, AttackSwordClipExistsAndDiffersFromIdle) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));

  auto const* blob = reg.blob(k_species_humanoid);
  ASSERT_NE(blob, nullptr);

  constexpr std::uint16_t k_attack_sword_a_clip =
      Render::Creature::k_humanoid_attack_sword_a_clip;
  ASSERT_GT(blob->clip_count(), static_cast<std::uint32_t>(k_attack_sword_a_clip))
      << "humanoid.bpat is missing attack_sword_a (re-run bpat_baker)";

  auto const attack_clip = blob->clip(k_attack_sword_a_clip);
  ASSERT_GT(attack_clip.frame_count, 0U);
  EXPECT_FALSE(attack_clip.loops) << "attack_sword_a must be a non-looping clip";

  std::uint32_t const mid_frame = attack_clip.frame_count / 2U;
  std::array<QMatrix4x4, 64> attack_palette{};
  auto const n = reg.sample_palette(k_species_humanoid,
                                    k_attack_sword_a_clip,
                                    mid_frame,
                                    std::span<QMatrix4x4>(attack_palette));
  ASSERT_GT(n, 0U);

  std::array<QMatrix4x4, 64> idle_palette{};
  reg.sample_palette(k_species_humanoid, 0U, 0U, std::span<QMatrix4x4>(idle_palette));

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

TEST(BpatRegistry, HumanoidAttackClipsBakeDenseFramesAndOrderedMarkers) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  auto const* default_blob = reg.blob(k_species_humanoid);
  auto const* sword_blob = reg.blob(k_species_humanoid_sword);
  ASSERT_NE(default_blob, nullptr);
  ASSERT_NE(sword_blob, nullptr);

  auto const assert_dense_ordered_attack = [](auto const& clip, const char* name) {
    EXPECT_GE(clip.frame_count, 32U) << name;
    EXPECT_GE(clip.marker_anticipation_start, 0.0F) << name;
    EXPECT_GT(clip.marker_weapon_release, clip.marker_anticipation_start) << name;
    EXPECT_GE(clip.marker_contact, clip.marker_weapon_release) << name;
    EXPECT_GT(clip.marker_recover_unlocked, clip.marker_contact) << name;
    EXPECT_GT(clip.marker_exit_safe, clip.marker_recover_unlocked) << name;
    EXPECT_LT(clip.marker_exit_safe, 1.0F) << name;
  };

  auto const default_sword =
      default_blob->clip(Render::Creature::k_humanoid_attack_sword_a_clip);
  auto const sword_ready_sword =
      sword_blob->clip(Render::Creature::k_humanoid_attack_sword_a_clip);
  auto const spear_clip =
      default_blob->clip(Render::Creature::k_humanoid_attack_spear_a_clip);
  auto const bow_clip =
      default_blob->clip(Render::Creature::k_humanoid_attack_bow_clip);

  assert_dense_ordered_attack(default_sword, "default attack_sword_a");
  assert_dense_ordered_attack(sword_ready_sword, "sword-ready attack_sword_a");
  assert_dense_ordered_attack(spear_clip, "attack_spear_a");
  assert_dense_ordered_attack(bow_clip, "attack_bow");

  EXPECT_GT(sword_ready_sword.marker_weapon_release,
            default_sword.marker_weapon_release);
  EXPECT_GT(sword_ready_sword.marker_recover_unlocked,
            default_sword.marker_recover_unlocked);
}

TEST(BpatRegistry, HumanoidAttackMarkersMatchAnimationManifest) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_spear, root + "/humanoid_spear.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_skeleton, root + "/humanoid_skeleton.bpat"));

  auto const assert_markers_equal = [&reg](std::uint32_t species_id,
                                           std::uint16_t clip_id,
                                           Animation::HumanoidClipProfile profile) {
    auto const* blob = reg.blob(species_id);
    ASSERT_NE(blob, nullptr);
    auto const clip = blob->clip(clip_id);
    auto const markers = Animation::authored_humanoid_clip_markers(clip_id, profile);
    EXPECT_FLOAT_EQ(clip.marker_anticipation_start, markers.anticipation_start);
    EXPECT_FLOAT_EQ(clip.marker_weapon_release, markers.weapon_release);
    EXPECT_FLOAT_EQ(clip.marker_contact, markers.contact);
    EXPECT_FLOAT_EQ(clip.marker_recover_unlocked, markers.recover_unlocked);
    EXPECT_FLOAT_EQ(clip.marker_exit_safe, markers.exit_safe);
  };

  assert_markers_equal(k_species_humanoid,
                       Animation::k_humanoid_attack_sword_a_clip,
                       Animation::HumanoidClipProfile::Default);
  assert_markers_equal(k_species_humanoid_sword,
                       Animation::k_humanoid_attack_sword_a_clip,
                       Animation::HumanoidClipProfile::SwordReady);
  assert_markers_equal(k_species_humanoid_spear,
                       Animation::k_humanoid_attack_spear_a_clip,
                       Animation::HumanoidClipProfile::SpearReady);
  assert_markers_equal(k_species_humanoid_skeleton,
                       Animation::k_humanoid_attack_sword_a_clip,
                       Animation::HumanoidClipProfile::Skeleton);
  assert_markers_equal(k_species_humanoid_sword,
                       Animation::k_humanoid_riding_sword_strike_clip,
                       Animation::HumanoidClipProfile::SwordReady);
}

TEST(BpatRegistry, HoldClipsBakeKneelingWeaponReadyPoses) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  auto sample_bone = [&](std::uint32_t species_id,
                         std::uint16_t clip_index,
                         std::uint32_t frame_in_clip,
                         HumanoidBone bone) {
    std::array<QMatrix4x4, 64> palette{};
    auto const n = reg.sample_palette(
        species_id, clip_index, frame_in_clip, std::span<QMatrix4x4>(palette));
    auto const bone_index = static_cast<std::size_t>(bone);
    EXPECT_GT(n, bone_index);
    return palette[bone_index].column(3).toVector3D();
  };

  auto const* default_blob = reg.blob(k_species_humanoid);
  auto const* sword_blob = reg.blob(k_species_humanoid_sword);
  ASSERT_NE(default_blob, nullptr);
  ASSERT_NE(sword_blob, nullptr);

  auto const idle_pelvis = sample_bone(k_species_humanoid,
                                       Render::Creature::k_humanoid_idle_clip,
                                       0U,
                                       HumanoidBone::Pelvis);
  auto const idle_hand_r = sample_bone(k_species_humanoid,
                                       Render::Creature::k_humanoid_idle_clip,
                                       0U,
                                       HumanoidBone::HandR);
  auto const idle_hand_l = sample_bone(k_species_humanoid,
                                       Render::Creature::k_humanoid_idle_clip,
                                       0U,
                                       HumanoidBone::HandL);

  auto const spear_hold_frame =
      default_blob->clip(Render::Creature::k_humanoid_hold_clip).frame_count - 1U;
  auto const bow_hold_frame =
      default_blob->clip(Render::Creature::k_humanoid_hold_bow_clip).frame_count - 1U;
  auto const sword_hold_frame =
      sword_blob->clip(Render::Creature::k_humanoid_hold_clip).frame_count - 1U;

  auto const spear_hold_pelvis = sample_bone(k_species_humanoid,
                                             Render::Creature::k_humanoid_hold_clip,
                                             spear_hold_frame,
                                             HumanoidBone::Pelvis);
  auto const spear_hold_hand_r = sample_bone(k_species_humanoid,
                                             Render::Creature::k_humanoid_hold_clip,
                                             spear_hold_frame,
                                             HumanoidBone::HandR);

  auto const bow_hold_pelvis = sample_bone(k_species_humanoid,
                                           Render::Creature::k_humanoid_hold_bow_clip,
                                           bow_hold_frame,
                                           HumanoidBone::Pelvis);
  auto const bow_hold_hand_l = sample_bone(k_species_humanoid,
                                           Render::Creature::k_humanoid_hold_bow_clip,
                                           bow_hold_frame,
                                           HumanoidBone::HandL);

  auto const sword_idle_pelvis = sample_bone(k_species_humanoid_sword,
                                             Render::Creature::k_humanoid_idle_clip,
                                             0U,
                                             HumanoidBone::Pelvis);
  auto const sword_hold_pelvis = sample_bone(k_species_humanoid_sword,
                                             Render::Creature::k_humanoid_hold_clip,
                                             sword_hold_frame,
                                             HumanoidBone::Pelvis);

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
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  std::array<QMatrix4x4, 64> default_palette{};
  std::array<QMatrix4x4, 64> sword_palette{};
  auto const n = reg.sample_palette(
      k_species_humanoid, 0U, 0U, std::span<QMatrix4x4>(default_palette));
  ASSERT_GT(n, 0U);
  ASSERT_EQ(reg.sample_palette(
                k_species_humanoid_sword, 0U, 0U, std::span<QMatrix4x4>(sword_palette)),
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

TEST(BpatRegistry, SkeletonHumanoidSwordAttackReachIsShorterThanDefaultHumanoid) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_skeleton, root + "/humanoid_skeleton.bpat"));

  std::array<QMatrix4x4, 64> default_palette{};
  std::array<QMatrix4x4, 64> skeleton_palette{};
  constexpr std::uint32_t k_frame = 14U;
  auto const bone_count =
      reg.sample_palette(k_species_humanoid,
                         Render::Creature::k_humanoid_attack_sword_a_clip,
                         k_frame,
                         std::span<QMatrix4x4>(default_palette));
  ASSERT_GT(bone_count, static_cast<std::uint32_t>(HumanoidBone::HandR));
  ASSERT_EQ(reg.sample_palette(k_species_humanoid_skeleton,
                               Render::Creature::k_humanoid_attack_sword_a_clip,
                               k_frame,
                               std::span<QMatrix4x4>(skeleton_palette)),
            bone_count);

  auto const hand_index = static_cast<std::size_t>(HumanoidBone::HandR);
  auto const shoulder_index = static_cast<std::size_t>(HumanoidBone::ShoulderR);
  QVector3D const default_hand = default_palette[hand_index].column(3).toVector3D();
  QVector3D const default_shoulder =
      default_palette[shoulder_index].column(3).toVector3D();
  QVector3D const skeleton_hand = skeleton_palette[hand_index].column(3).toVector3D();
  QVector3D const skeleton_shoulder =
      skeleton_palette[shoulder_index].column(3).toVector3D();

  EXPECT_LT(skeleton_hand.z() - skeleton_shoulder.z(),
            default_hand.z() - default_shoulder.z());
}

TEST(BpatRegistry, SwordHumanoidSwordAttackDiffersFromDefaultHumanoidAttack) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  auto const* default_blob = reg.blob(k_species_humanoid);
  auto const* sword_blob = reg.blob(k_species_humanoid_sword);
  ASSERT_NE(default_blob, nullptr);
  ASSERT_NE(sword_blob, nullptr);

  constexpr std::uint16_t k_attack_clip =
      Render::Creature::k_humanoid_attack_sword_a_clip;
  ASSERT_GT(default_blob->clip_count(), static_cast<std::uint32_t>(k_attack_clip));
  ASSERT_GT(sword_blob->clip_count(), static_cast<std::uint32_t>(k_attack_clip));

  std::uint32_t const frame = std::min(default_blob->clip(k_attack_clip).frame_count,
                                       sword_blob->clip(k_attack_clip).frame_count) /
                              2U;
  std::array<QMatrix4x4, 64> default_palette{};
  std::array<QMatrix4x4, 64> sword_palette{};
  auto const bone_count = reg.sample_palette(
      k_species_humanoid, k_attack_clip, frame, std::span<QMatrix4x4>(default_palette));
  ASSERT_GT(bone_count, static_cast<std::uint32_t>(HumanoidBone::HandR));
  ASSERT_EQ(reg.sample_palette(k_species_humanoid_sword,
                               k_attack_clip,
                               frame,
                               std::span<QMatrix4x4>(sword_palette)),
            bone_count);

  auto const hand_index = static_cast<std::size_t>(HumanoidBone::HandR);
  auto const shoulder_index = static_cast<std::size_t>(HumanoidBone::ShoulderR);
  QVector3D const default_hand = default_palette[hand_index].column(3).toVector3D();
  QVector3D const default_shoulder =
      default_palette[shoulder_index].column(3).toVector3D();
  QVector3D const sword_hand = sword_palette[hand_index].column(3).toVector3D();
  QVector3D const sword_shoulder = sword_palette[shoulder_index].column(3).toVector3D();

  EXPECT_GT((sword_hand - default_hand).length(), 0.05F);
  EXPECT_GT((sword_shoulder - default_shoulder).length(), 0.05F);
}

TEST(BpatRegistry, SwordHumanoidAmbientIdleStartsFromShieldReadyIdle) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  std::array<QMatrix4x4, 64> idle_palette{};
  auto const n = reg.sample_palette(k_species_humanoid_sword,
                                    Render::Creature::k_humanoid_idle_clip,
                                    0U,
                                    std::span<QMatrix4x4>(idle_palette));
  ASSERT_GT(n, 0U);

  for (auto clip : {Render::Creature::k_humanoid_idle_squat_clip,
                    Render::Creature::k_humanoid_idle_jump_clip,
                    Render::Creature::k_humanoid_idle_weapon_clip,
                    Render::Creature::k_humanoid_idle_weave_clip}) {
    std::array<QMatrix4x4, 64> ambient_palette{};
    ASSERT_EQ(
        reg.sample_palette(
            k_species_humanoid_sword, clip, 0U, std::span<QMatrix4x4>(ambient_palette)),
        n)
        << "clip " << clip;
    for (std::uint32_t bone = 0U; bone < n; ++bone) {
      EXPECT_EQ(ambient_palette[bone], idle_palette[bone])
          << "clip " << clip << " bone " << bone;
    }
  }
}

TEST(BpatRegistry, HumanoidIdleClipRemainsStableAcrossLoop) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  auto verify_idle_clip_stable = [&](std::uint32_t species_id) {
    auto const* blob = reg.blob(species_id);
    ASSERT_NE(blob, nullptr);
    auto const idle_clip = blob->clip(Render::Creature::k_humanoid_idle_clip);
    ASSERT_GT(idle_clip.frame_count, 2U);

    std::array<QMatrix4x4, 64> start_palette{};
    std::array<QMatrix4x4, 64> mid_palette{};
    auto const bone_count = reg.sample_palette(species_id,
                                               Render::Creature::k_humanoid_idle_clip,
                                               0U,
                                               std::span<QMatrix4x4>(start_palette));
    ASSERT_GT(bone_count, 0U);
    ASSERT_EQ(reg.sample_palette(species_id,
                                 Render::Creature::k_humanoid_idle_clip,
                                 idle_clip.frame_count / 2U,
                                 std::span<QMatrix4x4>(mid_palette)),
              bone_count);

    for (std::uint32_t bone = 0U; bone < bone_count; ++bone) {
      EXPECT_EQ(start_palette[bone], mid_palette[bone])
          << "species " << species_id
          << " idle clip should stay stable across the loop";
    }
  };

  verify_idle_clip_stable(k_species_humanoid);
  verify_idle_clip_stable(k_species_humanoid_sword);
}

TEST(BpatRegistry, HumanoidAmbientIdleClipStillAnimatesAcrossSequence) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  auto const* blob = reg.blob(k_species_humanoid_sword);
  ASSERT_NE(blob, nullptr);
  auto const ambient_clip = blob->clip(Render::Creature::k_humanoid_idle_weapon_clip);
  ASSERT_GT(ambient_clip.frame_count, 2U);

  std::array<QMatrix4x4, 64> start_palette{};
  std::array<QMatrix4x4, 64> mid_palette{};
  auto const bone_count =
      reg.sample_palette(k_species_humanoid_sword,
                         Render::Creature::k_humanoid_idle_weapon_clip,
                         0U,
                         std::span<QMatrix4x4>(start_palette));
  ASSERT_GT(bone_count, 0U);
  ASSERT_EQ(reg.sample_palette(k_species_humanoid_sword,
                               Render::Creature::k_humanoid_idle_weapon_clip,
                               ambient_clip.frame_count / 2U,
                               std::span<QMatrix4x4>(mid_palette)),
            bone_count);

  bool any_different = false;
  for (std::uint32_t bone = 0U; bone < bone_count; ++bone) {
    if (start_palette[bone] != mid_palette[bone]) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different)
      << "ambient idle clip should still animate across the sequence";
}

TEST(BpatRegistry, SwordHumanoidRidingChargeKeepsShieldHandMountedRelative) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  std::array<QMatrix4x4, 64> palette{};
  auto const n = reg.sample_palette(k_species_humanoid_sword,
                                    Render::Creature::k_humanoid_riding_charge_clip,
                                    0U,
                                    std::span<QMatrix4x4>(palette));
  auto const hand_index = static_cast<std::size_t>(HumanoidBone::HandL);
  ASSERT_GT(n, hand_index);
  QVector3D const hand_l = palette[hand_index].column(3).toVector3D();

  auto horse_profile = Render::GL::make_horse_profile(0U, {}, {});
  auto mount = Render::GL::compute_mount_frame(horse_profile);
  Render::GL::tune_mounted_knight_frame(horse_profile.dims, mount);
  QVector3D const expected = mount.seat_position + mount.seat_forward * 0.05F +
                             mount.seat_right * -0.16F + mount.seat_up * 0.22F;

  EXPECT_NEAR(hand_l.x(), expected.x(), 0.12F);
  EXPECT_NEAR(hand_l.y(), expected.y(), 0.12F);
  EXPECT_NEAR(hand_l.z(), expected.z(), 0.12F);
}

TEST(BpatRegistry, SwordHumanoidRidingSwordStrikeDropsHandBelowChamber) {
  using Render::Humanoid::HumanoidBone;

  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));

  std::array<QMatrix4x4, 64> chamber_palette{};
  std::array<QMatrix4x4, 64> strike_palette{};
  auto const bone_count =
      reg.sample_palette(k_species_humanoid_sword,
                         Render::Creature::k_humanoid_riding_sword_strike_clip,
                         8U,
                         std::span<QMatrix4x4>(chamber_palette));
  ASSERT_GT(bone_count, 0U);
  ASSERT_EQ(reg.sample_palette(k_species_humanoid_sword,
                               Render::Creature::k_humanoid_riding_sword_strike_clip,
                               22U,
                               std::span<QMatrix4x4>(strike_palette)),
            bone_count);

  auto const hand_index = static_cast<std::size_t>(HumanoidBone::HandR);
  auto const shoulder_index = static_cast<std::size_t>(HumanoidBone::ShoulderR);
  ASSERT_GT(bone_count, shoulder_index);

  QVector3D const chamber_hand = chamber_palette[hand_index].column(3).toVector3D();
  QVector3D const strike_hand = strike_palette[hand_index].column(3).toVector3D();
  QVector3D const chamber_shoulder =
      chamber_palette[shoulder_index].column(3).toVector3D();
  QVector3D const strike_shoulder =
      strike_palette[shoulder_index].column(3).toVector3D();

  EXPECT_GT(strike_hand.x(), chamber_hand.x() + 0.08F);
  EXPECT_LT(strike_hand.y(), chamber_hand.y());
  EXPECT_LT(strike_shoulder.y(), chamber_shoulder.y());
  EXPECT_GT(strike_hand.z(), chamber_hand.z());
}

TEST(BpatRegistry, SwordHumanoidRidingSwordStrikeRotatesGripIntoCut) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(
      reg.load_species(k_species_humanoid_sword, root + "/humanoid_sword.bpat"));
  auto const* blob = reg.blob(k_species_humanoid_sword);
  ASSERT_NE(blob, nullptr);

  std::uint32_t hand_socket_index = blob->socket_count();
  for (std::uint32_t i = 0U; i < blob->socket_count(); ++i) {
    if (blob->socket(i).name == "hand_r") {
      hand_socket_index = i;
      break;
    }
  }
  ASSERT_LT(hand_socket_index, blob->socket_count());

  QMatrix4x4 chamber_hand;
  QMatrix4x4 strike_hand;
  ASSERT_TRUE(reg.sample_socket(k_species_humanoid_sword,
                                Render::Creature::k_humanoid_riding_sword_strike_clip,
                                8U,
                                hand_socket_index,
                                chamber_hand));
  ASSERT_TRUE(reg.sample_socket(k_species_humanoid_sword,
                                Render::Creature::k_humanoid_riding_sword_strike_clip,
                                22U,
                                hand_socket_index,
                                strike_hand));

  QVector3D const chamber_up = chamber_hand.column(1).toVector3D().normalized();
  QVector3D const strike_up = strike_hand.column(1).toVector3D().normalized();

  EXPECT_LT(strike_up.y(), chamber_up.y() - 0.12F);
  EXPECT_GT(strike_up.z(), chamber_up.z() + 0.30F);
}
