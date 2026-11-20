#include <QFile>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <gtest/gtest.h>

#include "tools/map_editor/tool_catalog.h"
#include "tools/map_editor/world_backdrop.h"
#include "tools/map_editor/world_sprites.h"

namespace {

const QByteArray k_lake_and_hill = R"({
  "name": "Backdrop Test",
  "coord_system": "grid",
  "grid": {"width": 64, "height": 64, "tile_size": 1.0},
  "biome": {"ground_type": "soil_fertile", "seed": 7},
  "terrain": [{"type": "hill", "x": 44, "z": 20, "width": 16, "depth": 12, "height": 5,
               "entrances": [{"x": 44, "z": 30}, {"x": 34, "z": 20}]}],
  "lakes": [{"x": 18, "z": 44, "width": 18, "depth": 14}],
  "forests": [{"id": "wood", "x": 46, "z": 46, "radius": 9}],
  "roads": [{"start": [4, 8], "end": [60, 8], "width": 3}]
})";

auto pixel_at(const MapEditor::WorldBackdrop& backdrop,
              float grid_x,
              float grid_z) -> QColor {
  const int px = static_cast<int>((static_cast<float>(backdrop.grid_width) - grid_x) *
                                  backdrop.pixels_per_cell);
  const int py = static_cast<int>((static_cast<float>(backdrop.grid_height) - grid_z) *
                                  backdrop.pixels_per_cell);
  return backdrop.image.pixelColor(std::clamp(px, 0, backdrop.image.width() - 1),
                                   std::clamp(py, 0, backdrop.image.height() - 1));
}

auto shipped_map(const char* name) -> QByteArray {
  QFile file(QStringLiteral("assets/maps/%1").arg(QLatin1String(name)));
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << name;
  return file.readAll();
}

} // namespace

TEST(MapEditorWorldBackdrop, PaintsWaterBlueAndMeadowGreenWhereTheMapPutsThem) {
  const auto backdrop = MapEditor::render_world_backdrop(k_lake_and_hill);
  ASSERT_TRUE(backdrop.ok()) << backdrop.error.toStdString();
  EXPECT_EQ(backdrop.grid_width, 64);
  EXPECT_EQ(backdrop.image.width(), 512)
      << "a small map is painted at eight pixels a cell";

  const QColor lake = pixel_at(backdrop, 18.0F, 44.0F);
  EXPECT_GT(lake.blue(), lake.red() + 30) << "the lake is not water-coloured";
  EXPECT_GT(lake.blue(), lake.green());

  const QColor meadow = pixel_at(backdrop, 12.0F, 20.0F);
  EXPECT_GT(meadow.green(), meadow.blue() + 20) << "open ground is not grass-coloured";
  EXPECT_GT(meadow.green(), meadow.red());
}

TEST(MapEditorWorldBackdrop, LightsTheHillFromTheTopLeftOfTheCanvas) {
  const auto backdrop = MapEditor::render_world_backdrop(k_lake_and_hill);
  ASSERT_TRUE(backdrop.ok());
  const auto mean_value = [&](float grid_x, float grid_z) {
    int total = 0;
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dx = -1; dx <= 1; ++dx) {
        total += pixel_at(backdrop,
                          grid_x + static_cast<float>(dx) * 0.4F,
                          grid_z + static_cast<float>(dz) * 0.4F)
                     .value();
      }
    }
    return total / 9;
  };
  const int lit = mean_value(49.5F, 20.0F);
  const int shaded = mean_value(38.5F, 20.0F);
  EXPECT_GT(lit, shaded + 12) << "the canvas shows x growing to the left, so the slope "
                                 "at high x faces the light; lit="
                              << lit << " shaded=" << shaded;
}

TEST(MapEditorWorldBackdrop, TheSameMapPaintsTheSamePicture) {
  const auto first = MapEditor::render_world_backdrop(k_lake_and_hill);
  const auto second = MapEditor::render_world_backdrop(k_lake_and_hill);
  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(second.ok());
  EXPECT_TRUE(first.image == second.image)
      << "a repaint after an unrelated edit must not make the ground shimmer";
}

TEST(MapEditorWorldBackdrop, ALargeMapIsPaintedWithinTheSizeBudget) {
  MapEditor::WorldBackdropOptions options;
  options.max_dimension_px = 600;
  const auto backdrop =
      MapEditor::render_world_backdrop(shipped_map("map_aurelia_magna.json"), options);
  ASSERT_TRUE(backdrop.ok()) << backdrop.error.toStdString();
  EXPECT_LE(backdrop.image.width(), 769 * 2 + 2);
  EXPECT_FLOAT_EQ(backdrop.pixels_per_cell, options.min_pixels_per_cell);
}

TEST(MapEditorWorldBackdrop, ANightMapIsPaintedDarkAndSaysSo) {
  const auto night =
      MapEditor::render_world_backdrop(shipped_map("map_iron_sepulcher_watch.json"));
  const auto day =
      MapEditor::render_world_backdrop(shipped_map("map_pinewater_cut.json"));
  ASSERT_TRUE(night.ok()) << night.error.toStdString();
  ASSERT_TRUE(day.ok()) << day.error.toStdString();
  EXPECT_FLOAT_EQ(night.night, 1.0F);
  EXPECT_FLOAT_EQ(day.night, 0.0F);
  const auto mean_value = [](const QImage& image) {
    long long total = 0;
    int samples = 0;
    for (int y = 0; y < image.height(); y += 9) {
      for (int x = 0; x < image.width(); x += 9) {
        total += image.pixelColor(x, y).value();
        ++samples;
      }
    }
    return static_cast<int>(total / std::max(samples, 1));
  };
  EXPECT_LT(mean_value(night.image) + 25, mean_value(day.image));
}

TEST(MapEditorWorldBackdrop, ABrokenDocumentReportsWhyInsteadOfPaintingNothing) {
  const auto backdrop = MapEditor::render_world_backdrop("{ not json");
  EXPECT_FALSE(backdrop.ok());
  EXPECT_FALSE(backdrop.error.isEmpty());
}

TEST(MapEditorWorldSprites, EveryPlaceablePropAndBuildingHasAPaintedForm) {
  for (const MapEditor::ToolSpec& spec : MapEditor::tool_catalog()) {
    const QString type = MapEditor::element_type_for_tool(spec.tool);
    switch (MapEditor::tool_placement(spec.tool)) {
    case MapEditor::ToolPlacement::WorldProp:
      EXPECT_TRUE(MapEditor::natural_sprite_for(type) !=
                      MapEditor::NaturalSprite::None ||
                  MapEditor::has_prop_sprite(type))
          << type.toStdString()
          << " would fall back to a lettered badge on the painted world";
      break;
    case MapEditor::ToolPlacement::Structure:
      EXPECT_TRUE(MapEditor::has_structure_sprite(type))
          << type.toStdString()
          << " would fall back to a lettered badge on the painted world";
      break;
    default:
      break;
    }
  }
}

TEST(MapEditorWorldSprites, ASpritePutsPaintWhereItIsAsked) {
  QImage canvas(160, 160, QImage::Format_ARGB32_Premultiplied);
  for (const char* type :
       {"barracks", "home", "defense_tower", "marketplace", "temple", "farm"}) {
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    MapEditor::paint_structure_sprite(painter,
                                      QLatin1String(type),
                                      QPointF(80.0, 80.0),
                                      6.0F,
                                      0.0F,
                                      QColor(60, 120, 230),
                                      3U);
    painter.end();
    EXPECT_GT(canvas.pixelColor(80, 80).alpha(), 200)
        << type << " left its own centre unpainted";
    EXPECT_EQ(canvas.pixelColor(2, 2).alpha(), 0)
        << type << " painted far outside its footprint";
  }
}
