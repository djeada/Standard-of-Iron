#include <array>
#include <gtest/gtest.h>

#include "render/creature/humanoid/runtime/style_palette.h"
#include "render/entity/archer_renderer_common.h"
#include "render/entity/nations/material_palette.h"

namespace Render::GL {
namespace {

TEST(NationMaterialPalette, SameOwnerRetainsEachNationsDye) {
  constexpr std::array nations{Nation::k_roman_materials,
                               Nation::k_carthage_materials,
                               Nation::k_gallic_materials,
                               Nation::k_iberian_materials,
                               Nation::k_sepulcher_materials};
  constexpr std::array owners{QVector3D(0.1F, 0.2F, 0.9F),
                              QVector3D(0.9F, 0.2F, 0.1F),
                              QVector3D(0.1F, 0.7F, 0.2F)};
  for (const auto& owner : owners) {
    for (const auto& nation : nations) {
      auto palette = make_humanoid_palette(owner, 42U);
      ArcherStyleConfig style;
      Nation::apply_materials(style, nation);
      Humanoid::apply_uniform_palette(palette, style, owner);
      for (const auto& other : nations) {
        if (&other != &nation) {
          EXPECT_LT((palette.cloth - nation.cloth).lengthSquared(),
                    (palette.cloth - other.cloth).lengthSquared());
        }
      }
    }
  }
}

TEST(NationMaterialPalette, OwnerTintChangesClothAndPreservesWoodAndMetal) {
  ArcherStyleConfig style;
  Nation::apply_materials(style, Nation::k_carthage_materials);
  style.wood_color = Nation::k_carthage_materials.timber;
  constexpr QVector3D blue{0.1F, 0.2F, 0.9F};
  constexpr QVector3D red{0.9F, 0.2F, 0.1F};
  auto blue_palette = make_humanoid_palette(blue, 42U);
  auto red_palette = make_humanoid_palette(red, 42U);
  Humanoid::apply_uniform_palette(blue_palette, style, blue);
  Humanoid::apply_uniform_palette(red_palette, style, red);
  EXPECT_NE(red_palette.cloth, blue_palette.cloth);
  EXPECT_EQ(red_palette.wood, blue_palette.wood);
  EXPECT_EQ(red_palette.metal, blue_palette.metal);
}

} // namespace
} // namespace Render::GL
