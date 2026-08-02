#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <set>

#include "game/accessibility/team_identity.h"

namespace {

using namespace Game::Accessibility;

constexpr float k_min_luminance_gap = 0.12F;

constexpr std::array<PaletteVariant, 3> k_color_vision_variants{
    PaletteVariant::Protanopia,
    PaletteVariant::Deuteranopia,
    PaletteVariant::Tritanopia,
};

constexpr std::array<PaletteVariant, 4> k_all_variants{
    PaletteVariant::Standard,
    PaletteVariant::Protanopia,
    PaletteVariant::Deuteranopia,
    PaletteVariant::Tritanopia,
};

auto variant_name(PaletteVariant variant) -> const char* {
  switch (variant) {
  case PaletteVariant::Protanopia:
    return "protanopia";
  case PaletteVariant::Deuteranopia:
    return "deuteranopia";
  case PaletteVariant::Tritanopia:
    return "tritanopia";
  case PaletteVariant::Standard:
  default:
    return "standard";
  }
}

class TeamIdentityTest : public ::testing::Test {
protected:
  void SetUp() override {
    TeamIdentity::set_palette_variant(PaletteVariant::Standard);
    TeamIdentity::set_patterns_enabled(false);
  }

  void TearDown() override {
    TeamIdentity::set_palette_variant(PaletteVariant::Standard);
    TeamIdentity::set_patterns_enabled(false);
  }
};

TEST_F(TeamIdentityTest, EveryPaletteGivesEachTeamItsOwnColour) {
  for (const auto variant : k_all_variants) {
    for (std::size_t a = 0; a < k_team_palette_size; ++a) {
      for (std::size_t b = a + 1; b < k_team_palette_size; ++b) {
        EXPECT_NE(TeamIdentity::palette(variant)[a], TeamIdentity::palette(variant)[b])
            << variant_name(variant) << " slots " << a << " and " << b;
      }
    }
  }
}

TEST_F(TeamIdentityTest, ColourVisionPalettesStaySeparableWithoutHue) {

  for (const auto variant : k_color_vision_variants) {
    const auto& palette = TeamIdentity::palette(variant);
    for (std::size_t a = 0; a < k_team_palette_size; ++a) {
      for (std::size_t b = a + 1; b < k_team_palette_size; ++b) {
        const float gap = std::abs(TeamIdentity::relative_luminance(palette[a]) -
                                   TeamIdentity::relative_luminance(palette[b]));
        EXPECT_GE(gap, k_min_luminance_gap)
            << variant_name(variant) << " slots " << a << " and " << b;
      }
    }
  }
}

TEST_F(TeamIdentityTest, ChannelsStayInsideTheRenderableRange) {
  for (const auto variant : k_all_variants) {
    for (const auto& color : TeamIdentity::palette(variant)) {
      for (const float channel : color) {
        EXPECT_GE(channel, 0.0F) << variant_name(variant);
        EXPECT_LE(channel, 1.0F) << variant_name(variant);
      }
    }
  }
}

TEST_F(TeamIdentityTest, TheStandardPaletteIsUnchangedFromWhatTheGameShipped) {
  const auto& palette = TeamIdentity::palette(PaletteVariant::Standard);

  EXPECT_EQ(palette[0], (TeamColor{0.20F, 0.55F, 1.00F}));
  EXPECT_EQ(palette[1], (TeamColor{1.00F, 0.30F, 0.30F}));
  EXPECT_EQ(palette[2], (TeamColor{0.20F, 0.80F, 0.40F}));
  EXPECT_EQ(palette[3], (TeamColor{1.00F, 0.80F, 0.20F}));
}

TEST_F(TeamIdentityTest, OwnerSlotOneMapsToTheFirstPaletteEntry) {
  EXPECT_EQ(TeamIdentity::color_for_slot(1),
            TeamIdentity::palette(PaletteVariant::Standard)[0]);
  EXPECT_EQ(TeamIdentity::color_for_slot(4),
            TeamIdentity::palette(PaletteVariant::Standard)[3]);

  EXPECT_EQ(TeamIdentity::color_for_slot(5), TeamIdentity::color_for_slot(1));

  EXPECT_EQ(TeamIdentity::color_for_slot(0),
            TeamIdentity::palette(PaletteVariant::Standard)[0]);
}

TEST_F(TeamIdentityTest, TheFourShippedTeamSlotsEachGetTheirOwnRingShape) {
  std::set<TeamPattern> seen;
  for (int slot = 1; slot <= static_cast<int>(k_team_palette_size); ++slot) {
    seen.insert(TeamIdentity::pattern_for_slot(slot));
  }
  EXPECT_EQ(seen.size(), k_team_palette_size);
}

TEST_F(TeamIdentityTest, TeamsThatShareAColourNeverAlsoShareAPattern) {

  ASSERT_EQ(TeamIdentity::color_for_slot(5), TeamIdentity::color_for_slot(1));
  EXPECT_NE(TeamIdentity::pattern_for_slot(5), TeamIdentity::pattern_for_slot(1));
}

TEST_F(TeamIdentityTest, TheFirstTeamKeepsTheSolidRingTheGameAlwaysDrew) {
  EXPECT_EQ(TeamIdentity::pattern_for_slot(1), TeamPattern::Solid);
}

TEST_F(TeamIdentityTest, ModeNamesSelectTheMatchingPalette) {
  TeamIdentity::set_palette_variant_from_mode("deuteranopia");
  EXPECT_EQ(TeamIdentity::palette_variant(), PaletteVariant::Deuteranopia);

  TeamIdentity::set_palette_variant_from_mode("tritanopia");
  EXPECT_EQ(TeamIdentity::palette_variant(), PaletteVariant::Tritanopia);

  TeamIdentity::set_palette_variant_from_mode("none");
  EXPECT_EQ(TeamIdentity::palette_variant(), PaletteVariant::Standard);
}

TEST_F(TeamIdentityTest, AnUnknownModeNameFallsBackToTheStandardPalette) {
  TeamIdentity::set_palette_variant_from_mode("protanopia");
  ASSERT_EQ(TeamIdentity::palette_variant(), PaletteVariant::Protanopia);

  TeamIdentity::set_palette_variant_from_mode("not-a-mode");

  EXPECT_EQ(TeamIdentity::palette_variant(), PaletteVariant::Standard);
}

TEST_F(TeamIdentityTest, TheActiveVariantIsWhatSlotLookupsUse) {
  TeamIdentity::set_palette_variant(PaletteVariant::Tritanopia);

  EXPECT_EQ(TeamIdentity::color_for_slot(2),
            TeamIdentity::palette(PaletteVariant::Tritanopia)[1]);
  EXPECT_NE(TeamIdentity::color_for_slot(2),
            TeamIdentity::palette(PaletteVariant::Standard)[1]);
}

} // namespace
