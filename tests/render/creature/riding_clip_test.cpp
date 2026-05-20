#include <QMatrix4x4>

#include <array>
#include <gtest/gtest.h>
#include <string>

#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/humanoid_clip_ids.h"
#include "tests/render/test_asset_paths.h"

using namespace Render::Creature::Bpat;

TEST(RidingClip, HumanoidBpatHasAtLeast16Clips) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  auto const* blob = reg.blob(k_species_humanoid);
  ASSERT_NE(blob, nullptr);
  EXPECT_GT(
      blob->clip_count(),
      static_cast<std::uint32_t>(Render::Creature::k_humanoid_riding_sword_strike_clip))
      << "humanoid.bpat must contain the mounted sword strike clip (re-run bpat_baker)";
}

TEST(RidingClip, RidingIdleClipDiffersFromInfantryIdle) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  auto const* blob = reg.blob(k_species_humanoid);
  ASSERT_NE(blob, nullptr);
  ASSERT_GT(blob->clip_count(),
            static_cast<std::uint32_t>(Render::Creature::k_humanoid_riding_idle_clip));

  constexpr std::uint16_t k_riding_idle_clip =
      Render::Creature::k_humanoid_riding_idle_clip;
  constexpr std::uint16_t k_idle_clip = Render::Creature::k_humanoid_idle_clip;

  std::array<QMatrix4x4, 64> riding_palette{};
  auto const n = reg.sample_palette(k_species_humanoid,
                                    k_riding_idle_clip,
                                    0U,
                                    std::span<QMatrix4x4>(riding_palette));
  ASSERT_GT(n, 0U);

  std::array<QMatrix4x4, 64> idle_palette{};
  reg.sample_palette(
      k_species_humanoid, k_idle_clip, 0U, std::span<QMatrix4x4>(idle_palette));

  bool any_different = false;
  for (std::uint32_t i = 0U; i < n; ++i) {
    if (riding_palette[i] != idle_palette[i]) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different) << "riding_idle clip must differ from infantry idle clip";
}

TEST(RidingClip, RidingChargeClipIsNonLooping) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  auto const* blob = reg.blob(k_species_humanoid);
  ASSERT_NE(blob, nullptr);
  ASSERT_GT(
      blob->clip_count(),
      static_cast<std::uint32_t>(Render::Creature::k_humanoid_riding_charge_clip));

  constexpr std::uint16_t k_riding_charge_clip =
      Render::Creature::k_humanoid_riding_charge_clip;
  auto const clip = blob->clip(k_riding_charge_clip);
  EXPECT_FALSE(clip.loops) << "riding_charge must be non-looping";
  EXPECT_GT(clip.frame_count, 0U);
}

TEST(RidingClip, RidingSwordStrikeClipIsNonLooping) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(k_species_humanoid, root + "/humanoid.bpat"));
  auto const* blob = reg.blob(k_species_humanoid);
  ASSERT_NE(blob, nullptr);
  ASSERT_GT(blob->clip_count(),
            static_cast<std::uint32_t>(
                Render::Creature::k_humanoid_riding_sword_strike_clip));

  auto const clip = blob->clip(Render::Creature::k_humanoid_riding_sword_strike_clip);
  EXPECT_FALSE(clip.loops) << "riding_sword_strike must be non-looping";
  EXPECT_GT(clip.frame_count, 0U);
}
