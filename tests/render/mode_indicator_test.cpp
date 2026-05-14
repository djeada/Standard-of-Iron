#include <gtest/gtest.h>

#include "render/geom/mode_indicator.h"

namespace {

TEST(ModeIndicator, UsesLargerIconScale) {
  EXPECT_FLOAT_EQ(Render::Geom::k_indicator_size, 0.8F);
}

TEST(ModeIndicator, RaisesMarkersForTallerUnits) {
  float const infantry_height = Render::Geom::indicator_height_for_unit(1.1F, 0.6F);
  float const cavalry_height = Render::Geom::indicator_height_for_unit(2.0F, 0.8F);
  float const elephant_height = Render::Geom::indicator_height_for_unit(3.2F, 2.1F);

  EXPECT_GT(cavalry_height, infantry_height);
  EXPECT_GT(elephant_height, cavalry_height);
}

TEST(ModeIndicator, KeepsClearanceForSmallUnits) {
  EXPECT_GE(Render::Geom::indicator_height_for_unit(0.4F, 0.5F),
            Render::Geom::k_indicator_height_base + Render::Geom::k_indicator_head_gap);
}

} // namespace
