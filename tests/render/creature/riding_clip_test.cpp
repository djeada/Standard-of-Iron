#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"

#include <QMatrix4x4>
#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

using namespace Render::Creature::Bpat;

namespace {

auto find_assets_dir() -> std::string {
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

TEST(RidingClip, HumanoidBpatHasAtLeast15Clips) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  auto const *blob = reg.blob(kSpeciesHumanoid);
  ASSERT_NE(blob, nullptr);
  EXPECT_GE(blob->clip_count(), 15U)
      << "humanoid.bpat must contain riding clips (re-run bpat_baker)";
}

TEST(RidingClip, RidingIdleClipDiffersFromInfantryIdle) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  auto const *blob = reg.blob(kSpeciesHumanoid);
  ASSERT_NE(blob, nullptr);
  ASSERT_GE(blob->clip_count(), 12U);

  constexpr std::uint16_t kRidingIdleClip = 11U;
  constexpr std::uint16_t kIdleClip = 0U;

  std::array<QMatrix4x4, 64> riding_palette{};
  auto const n = reg.sample_palette(kSpeciesHumanoid, kRidingIdleClip, 0U,
                                    std::span<QMatrix4x4>(riding_palette));
  ASSERT_GT(n, 0U);

  std::array<QMatrix4x4, 64> idle_palette{};
  reg.sample_palette(kSpeciesHumanoid, kIdleClip, 0U,
                     std::span<QMatrix4x4>(idle_palette));

  bool any_different = false;
  for (std::uint32_t i = 0U; i < n; ++i) {
    if (riding_palette[i] != idle_palette[i]) {
      any_different = true;
      break;
    }
  }
  EXPECT_TRUE(any_different)
      << "riding_idle clip must differ from infantry idle clip";
}

TEST(RidingClip, RidingChargeClipIsNonLooping) {
  auto const root = find_assets_dir();
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto &reg = BpatRegistry::instance();
  ASSERT_TRUE(reg.load_species(kSpeciesHumanoid, root + "/humanoid.bpat"));
  auto const *blob = reg.blob(kSpeciesHumanoid);
  ASSERT_NE(blob, nullptr);
  ASSERT_GE(blob->clip_count(), 13U);

  constexpr std::uint16_t kRidingChargeClip = 12U;
  auto const clip = blob->clip(kRidingChargeClip);
  EXPECT_FALSE(clip.loops) << "riding_charge must be non-looping";
  EXPECT_GT(clip.frame_count, 0U);
}
