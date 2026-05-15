#include <QPoint>
#include <QPointF>

#include <gtest/gtest.h>

#include "tools/map_editor/canvas_transform.h"

namespace {

TEST(MapEditorCanvasTransformTest, CornersMirrorToMatchInGameFacing) {
  const MapEditor::GridSettings grid{100, 80, 1.0F};
  const QPointF pan_offset(24.0F, 40.0F);
  constexpr float zoom = 1.0F;
  constexpr float cell_size = 8.0F;

  EXPECT_EQ(MapEditor::CanvasTransform::grid_to_widget(
                0.0F, 0.0F, grid, zoom, pan_offset, cell_size),
            QPoint(824, 680));
  EXPECT_EQ(MapEditor::CanvasTransform::grid_to_widget(
                100.0F, 0.0F, grid, zoom, pan_offset, cell_size),
            QPoint(24, 680));
  EXPECT_EQ(MapEditor::CanvasTransform::grid_to_widget(
                0.0F, 80.0F, grid, zoom, pan_offset, cell_size),
            QPoint(824, 40));
  EXPECT_EQ(MapEditor::CanvasTransform::grid_to_widget(
                100.0F, 80.0F, grid, zoom, pan_offset, cell_size),
            QPoint(24, 40));
}

TEST(MapEditorCanvasTransformTest, WidgetAndGridTransformsRoundTrip) {
  const MapEditor::GridSettings grid{64, 48, 1.0F};
  const QPointF pan_offset(13.0F, 29.0F);
  constexpr float zoom = 1.5F;
  constexpr float cell_size = 8.0F;

  const QPoint widget = MapEditor::CanvasTransform::grid_to_widget(
      17.5F, 12.25F, grid, zoom, pan_offset, cell_size);
  const QPointF round_trip = MapEditor::CanvasTransform::widget_to_grid(
      widget, grid, zoom, pan_offset, cell_size);

  EXPECT_NEAR(round_trip.x(), 17.5F, 0.1F);
  EXPECT_NEAR(round_trip.y(), 12.25F, 0.1F);
}

} // namespace
