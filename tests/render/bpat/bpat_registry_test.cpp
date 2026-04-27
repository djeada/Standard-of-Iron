#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"

#include <gtest/gtest.h>

#include <QMatrix4x4>

#include <array>
#include <filesystem>
#include <string>

using namespace Render::Creature::Bpat;

namespace {

auto find_assets_dir() -> std::string {
  // Tests run with CWD == project root or build dir; look for asset relative.
  for (auto const *candidate :
       {"assets/creatures", "../assets/creatures", "../../assets/creatures"}) {
    std::filesystem::path p{candidate};
    if (std::filesystem::exists(p / "humanoid.bpat")) {
      return std::filesystem::absolute(p).string();
    }
  }
  return {};
}

} // namespace

TEST(BpatRegistry, LoadsAllSpecies) {
  auto const root = find_assets_dir();
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
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }

  namespace fs = std::filesystem;
  fs::path const project_root = fs::path(root).parent_path().parent_path();
  fs::path const build_dir = project_root / "build";
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
  auto const root = find_assets_dir();
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
    // Default-constructed QMatrix4x4 is identity; baked frame 0 should not
    // leave the entire palette as identity.
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
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found in CWD";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));

  auto const *blob = reg.blob(kSpeciesHumanoid);
  ASSERT_NE(blob, nullptr);

  // Clip 4 is attack_sword_a.
  constexpr std::uint16_t kAttackSwordAClip = 4U;
  ASSERT_GT(blob->clip_count(), static_cast<std::uint32_t>(kAttackSwordAClip))
      << "humanoid.bpat must contain at least 11 clips (re-run bpat_baker)";

  auto const attack_clip = blob->clip(kAttackSwordAClip);
  ASSERT_GT(attack_clip.frame_count, 0U);
  EXPECT_FALSE(attack_clip.loops)
      << "attack_sword_a must be a non-looping clip";

  // Sample mid-attack frame.
  std::uint32_t const mid_frame = attack_clip.frame_count / 2U;
  std::array<QMatrix4x4, 64> attack_palette{};
  auto const n =
      reg.sample_palette(kSpeciesHumanoid, kAttackSwordAClip, mid_frame,
                         std::span<QMatrix4x4>(attack_palette));
  ASSERT_GT(n, 0U);

  // Sample idle clip frame 0 for comparison.
  std::array<QMatrix4x4, 64> idle_palette{};
  reg.sample_palette(kSpeciesHumanoid, 0U, 0U,
                     std::span<QMatrix4x4>(idle_palette));

  // At least one bone must differ — the attack pose moves the right arm.
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

TEST(BpatRegistry, SwordHumanoidIdleDiffersFromDefaultHumanoid) {
  auto const root = find_assets_dir();
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
