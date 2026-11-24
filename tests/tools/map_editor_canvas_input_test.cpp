#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QWheelEvent>

#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "tests/strategic/strategic_test_world.h"
#include "tools/map_editor/canvas_input.h"
#include "tools/map_editor/editor_window.h"
#include "tools/map_editor/element_edit_json.h"
#include "tools/map_editor/json_edit_dialog.h"
#include "tools/map_editor/map_canvas.h"
#include "tools/map_editor/move_handle.h"
#include "tools/map_editor/terrain_projection_widget.h"
#include "tools/map_editor/world_canvas.h"
#include "tools/map_editor/world_document.h"

namespace {

using MapEditor::right_click_action;
using MapEditor::RightClickAction;
using MapEditor::ToolType;

TEST(MapEditorCanvasInputTest, RightClickPutsAnArmedToolAway) {
  EXPECT_EQ(right_click_action(false, ToolType::Forest), RightClickAction::ClearTool);
  EXPECT_EQ(right_click_action(false, ToolType::Gate), RightClickAction::ClearTool);
  EXPECT_EQ(right_click_action(false, ToolType::Hill), RightClickAction::ClearTool);
  EXPECT_EQ(right_click_action(false, ToolType::WildlifeSheep),
            RightClickAction::ClearTool);
  EXPECT_EQ(right_click_action(false, ToolType::Eraser), RightClickAction::ClearTool);
}

TEST(MapEditorCanvasInputTest, RightClickStillOpensTheMenuWhenNothingIsArmed) {
  EXPECT_EQ(right_click_action(false, ToolType::Select),
            RightClickAction::ShowContextMenu)
      << "the element menu - edit, duplicate, delete - is only reachable this way";
}

TEST(MapEditorCanvasInputTest, AHalfDrawnLineIsCancelledBeforeTheToolIsPutAway) {
  EXPECT_EQ(right_click_action(true, ToolType::Wall),
            RightClickAction::CancelLinearDraw)
      << "the first right-click drops the pending endpoint, not the wall tool";
  EXPECT_EQ(right_click_action(true, ToolType::Road),
            RightClickAction::CancelLinearDraw);
}

void pointer(QWidget& canvas,
             QEvent::Type type,
             QPoint pos,
             Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  const auto button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
  const auto buttons =
      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
  QMouseEvent event(type, pos, canvas.mapToGlobal(pos), button, buttons, modifiers);
  QApplication::sendEvent(&canvas, &event);
}

void click(QWidget& canvas,
           QPoint pos,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  pointer(canvas, QEvent::MouseButtonPress, pos, modifiers);
  pointer(canvas, QEvent::MouseButtonRelease, pos, modifiers);
}

void escape(QWidget& widget) {
  QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(&widget, &event);
}

const QByteArray k_map = R"({
  "name": "Canvas input",
  "grid": {"width": 64, "height": 64, "tile_size": 1.0},
  "structures": [{"type":"home", "x":32, "z":32}, {"type":"home", "x":40, "z":40}],
  "rivers": [{"start":[14, 8], "end":[14, 56], "width": 4}],
  "fords": [{"position":{"x":14, "z":22}, "length":12}],
  "roads": [{"start":[8, 44], "end":[44, 44], "waypoints":[[8, 44], [24, 46], [44, 44]]}],
  "forests": [{"x":44, "z":12, "radius": 6}]
})";

using MapEditor::MapCanvas;

class MapEditorViewportTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(data.load_from_bytes(k_map));
    canvas.set_world_view(false);
    canvas.set_map_data(&data);
    canvas.resize(560, 560);
    canvas.show();
    canvas.set_zoom(1.0F);
    canvas.center_on(QPointF(32, 32));
    center = QPoint(canvas.width() / 2, canvas.height() / 2);
  }

  MapEditor::MapData data;
  MapCanvas canvas;
  QPoint center;
};

TEST_F(MapEditorViewportTest, DraggingFromAnObjectPansWithOrWithoutASelection) {
  const QPoint pan(48, 32);
  pointer(canvas, QEvent::MouseButtonPress, center);
  pointer(canvas, QEvent::MouseMove, center + pan);
  pointer(canvas, QEvent::MouseButtonRelease, center + pan);
  EXPECT_FALSE(canvas.has_selection());
  click(canvas, center + pan);
  ASSERT_EQ(canvas.selected_element_type(), MapCanvas::LayerStructure);
  pointer(canvas, QEvent::MouseButtonPress, center + pan);
  pointer(canvas, QEvent::MouseMove, center + pan * 2);
  pointer(canvas, QEvent::MouseButtonRelease, center + pan * 2);
  EXPECT_EQ(canvas.selection_count(), 1);
  EXPECT_FLOAT_EQ(data.structures().front().x, 32.0F);
  EXPECT_FLOAT_EQ(data.structures().front().z, 32.0F);
  EXPECT_FALSE(data.can_undo());

  const QPoint anchor = center + pan * 2;
  const float old_zoom = canvas.zoom();
  QWheelEvent wheel(anchor,
                    canvas.mapToGlobal(anchor),
                    {},
                    QPoint(0, 120),
                    Qt::NoButton,
                    Qt::NoModifier,
                    Qt::NoScrollPhase,
                    false);
  QApplication::sendEvent(&canvas, &wheel);
  EXPECT_GT(canvas.zoom(), old_zoom);
  canvas.clear_selection();
  click(canvas, anchor);
  EXPECT_EQ(canvas.selected_element_index(), 0);
  EXPECT_FALSE(data.is_modified());
}

TEST_F(MapEditorViewportTest, MovingAGroupPreviewsLocallyAndCommitsOneUndoStep) {
  click(canvas, center + QPoint(-64, -64));
  click(canvas, center, Qt::ShiftModifier);
  ASSERT_EQ(canvas.selection_count(), 2);
  const QPoint handle =
      MapEditor::move_handle_rect(center, canvas.rect()).center().toPoint();
  int changes = 0;
  QObject::connect(
      &data, &MapEditor::MapData::data_changed, &canvas, [&]() { ++changes; });
  pointer(canvas, QEvent::MouseButtonPress, handle);
  pointer(canvas, QEvent::MouseMove, handle + QPoint(32, 0));
  EXPECT_EQ(changes, 0);
  EXPECT_FALSE(data.is_modified());
  EXPECT_FLOAT_EQ(data.structures().front().x, 32.0F);

  pointer(canvas, QEvent::MouseButtonRelease, handle + QPoint(40, 0));
  EXPECT_EQ(changes, 1);
  EXPECT_FLOAT_EQ(data.structures().front().x, 27.0F);
  EXPECT_FLOAT_EQ(data.structures().back().x, 35.0F);
  ASSERT_TRUE(data.can_undo());
  data.undo();
  EXPECT_EQ(changes, 2);
  EXPECT_FLOAT_EQ(data.structures().front().x, 32.0F);
  EXPECT_FLOAT_EQ(data.structures().back().x, 40.0F);
  EXPECT_FALSE(data.can_undo());
}

TEST_F(MapEditorViewportTest, EscapeDiscardsAMoveWithoutDirtyingTheMap) {
  click(canvas, center);
  const QPoint handle =
      MapEditor::move_handle_rect(center, canvas.rect()).center().toPoint();
  pointer(canvas, QEvent::MouseButtonPress, handle);
  pointer(canvas, QEvent::MouseMove, handle + QPoint(50, 15));
  escape(canvas);
  pointer(canvas, QEvent::MouseButtonRelease, handle + QPoint(50, 15));
  EXPECT_EQ(canvas.selection_count(), 1);
  EXPECT_FALSE(data.is_modified());
  EXPECT_FALSE(data.can_undo());
  EXPECT_FLOAT_EQ(data.structures().front().x, 32.0F);
  EXPECT_FLOAT_EQ(data.structures().front().z, 32.0F);
}

TEST_F(MapEditorViewportTest, SelectedEndpointMovesItsWaypointWhenReleased) {
  const QPoint endpoint = center + QPoint(192, -96);
  click(canvas, endpoint);
  ASSERT_EQ(canvas.selected_element_type(), MapCanvas::LayerLinear);
  ASSERT_EQ(data.linear_elements()[canvas.selected_element_index()].type,
            QStringLiteral("road"));
  const int index = canvas.selected_element_index();
  pointer(canvas, QEvent::MouseButtonPress, endpoint);
  pointer(canvas, QEvent::MouseMove, endpoint + QPoint(0, 24));
  EXPECT_FLOAT_EQ(data.linear_elements()[index].start.y(), 44.0F);
  pointer(canvas, QEvent::MouseButtonRelease, endpoint + QPoint(0, 24));
  EXPECT_FLOAT_EQ(data.linear_elements()[index].start.y(), 41.0F);
  EXPECT_EQ(data.linear_elements()[index].waypoints.front(), QPointF(8, 41));
}

TEST_F(MapEditorViewportTest, FordMarkerPicksTheCrossingAndLayersAreIndependent) {
  const QPoint ford = center + QPoint(144, 80);
  click(canvas, ford);
  ASSERT_EQ(canvas.selected_element_type(), MapCanvas::LayerLinear);
  const auto& crossing = data.linear_elements()[canvas.selected_element_index()];
  EXPECT_EQ(crossing.type, QStringLiteral("ford"));
  EXPECT_EQ((crossing.start + crossing.end) * 0.5F, QVector2D(14, 22));
  EXPECT_FLOAT_EQ(crossing.start.y(), crossing.end.y());
  EXPECT_NE(crossing.start.x(), crossing.end.x());

  canvas.set_layer_visible(MapCanvas::LayerLinear, false);
  EXPECT_FALSE(canvas.has_selection());
  click(canvas, ford);
  EXPECT_FALSE(canvas.has_selection());
  canvas.set_layer_visible(MapCanvas::LayerFogZone, false);
  EXPECT_TRUE(canvas.layer_visible(MapCanvas::LayerForest));
  click(canvas, center + QPoint(-96, 160));
  EXPECT_EQ(canvas.selected_element_type(), MapCanvas::LayerForest);
}

TEST_F(MapEditorViewportTest,
       TerrainRefreshPausesPlacementUntilThePreviewOrModeChanges) {
  canvas.set_world_view(true);
  EXPECT_TRUE(canvas.preview_pending());
  EXPECT_TRUE(canvas.edits_paused());
  canvas.set_current_tool(ToolType::Hill);
  click(canvas, center);
  EXPECT_TRUE(data.terrain_elements().isEmpty());

  canvas.set_world_view(false);
  EXPECT_FALSE(canvas.edits_paused());
  click(canvas, center);
  ASSERT_EQ(data.terrain_elements().size(), 1);
  canvas.set_world_view(true);
  EXPECT_TRUE(canvas.edits_paused());

  QEventLoop loop;
  QObject::connect(&canvas, &MapCanvas::preview_changed, &loop, [&]() {
    if (!canvas.preview_pending()) {
      loop.quit();
    }
  });
  QTimer::singleShot(5000, &loop, &QEventLoop::quit);
  loop.exec();
  ASSERT_FALSE(canvas.edits_paused());
  ASSERT_TRUE(canvas.world_backdrop_ready()) << canvas.preview_error().toStdString();
  canvas.set_current_tool(ToolType::PropBoulder);
  click(canvas, center);
  EXPECT_EQ(data.world_props().size(), 1);
  EXPECT_TRUE(canvas.preview_pending());
  EXPECT_FALSE(canvas.edits_paused());
  EXPECT_TRUE(canvas.world_backdrop_ready());
}

TEST_F(MapEditorViewportTest, HillPresetsSaveTheEntranceShownInTheProjection) {
  for (const auto tool : {ToolType::Hill,
                          ToolType::HillRidge,
                          ToolType::HillArc,
                          ToolType::HillElbow,
                          ToolType::HillRing}) {
    SCOPED_TRACE(static_cast<int>(tool));
    canvas.set_current_tool(tool);
    click(canvas, center);
    ASSERT_EQ(data.terrain_elements().size(), 1);
    const auto& hill = data.terrain_elements().front();
    ASSERT_EQ(hill.entrances.size(), 1);
    const auto saved = QJsonDocument::fromJson(data.to_json_bytes()).object();
    const auto authored =
        saved.value(QStringLiteral("terrain")).toArray().first().toObject();
    EXPECT_EQ(authored.value(QStringLiteral("entrances")).toArray(), hill.entrances);

    const auto edit = MapEditor::ElementEditJson::to_document(hill);
    ASSERT_TRUE(edit.has_value());
    MapEditor::JsonEditDialog dialog(edit->title,
                                     edit->json,
                                     edit->hill_projection,
                                     {},
                                     nullptr,
                                     MapEditor::HillProjection::map_context(data));
    ASSERT_TRUE(dialog.is_valid());
    EXPECT_EQ(dialog.get_json().value(QStringLiteral("entrances")).toArray(),
              hill.entrances);
    const auto* projection = dialog.findChild<MapEditor::TerrainProjectionWidget*>();
    ASSERT_NE(projection, nullptr);
    ASSERT_EQ(projection->entrance_cells().size(), 1);
    EXPECT_TRUE(MapEditor::HillProjection::rim_cells(projection->body_cells())
                    .contains(projection->entrance_cells().front()));

    QTemporaryFile file;
    ASSERT_TRUE(file.open());
    const QByteArray runtime_json = data.to_canonical_json_bytes();
    ASSERT_EQ(file.write(runtime_json), runtime_json.size());
    ASSERT_TRUE(file.flush());
    Game::Map::MapDefinition definition;
    QString error;
    ASSERT_TRUE(
        Game::Map::MapLoader::load_from_json_file(file.fileName(), definition, &error))
        << error.toStdString();
    Game::Map::TerrainHeightMap height_map(
        definition.grid.width, definition.grid.height, definition.grid.tile_size);
    height_map.build_from_features(definition.terrain);
    const auto entry = hill.entrances.first().toObject();
    EXPECT_TRUE(height_map.isHillEntrance(entry.value(QStringLiteral("x")).toInt(),
                                          entry.value(QStringLiteral("z")).toInt()));
    data.undo();
  }
}

TEST(MapEditorCanvasInputTest, HillDialogRequiresAnEntranceAndCanRestoreItsDefault) {
  const QJsonObject legacy{{"type", "hill"},
                           {"shape", "arc"},
                           {"x", 32},
                           {"z", 32},
                           {"width", 40},
                           {"depth", 40},
                           {"thickness", 10}};
  MapEditor::JsonEditDialog dialog(
      QStringLiteral("Edit terrain"), legacy, true, {}, nullptr, {1.0, 64, 64});
  auto* editor = dialog.findChild<QPlainTextEdit*>();
  auto* projection = dialog.findChild<MapEditor::TerrainProjectionWidget*>();
  ASSERT_NE(editor, nullptr);
  ASSERT_NE(projection, nullptr);
  const auto buttons = dialog.findChildren<QPushButton*>();
  const auto find_button = [&buttons](const QString& text) -> QPushButton* {
    for (auto* button : buttons) {
      if (button->text() == text) {
        return button;
      }
    }
    return nullptr;
  };
  const auto draft = dialog.get_json().value(QStringLiteral("entrances")).toArray();
  ASSERT_EQ(draft.size(), 1);
  ASSERT_FALSE(projection->entrance_cells().isEmpty());
  auto* ok = find_button(QStringLiteral("OK"));
  ASSERT_NE(ok, nullptr);
  ASSERT_TRUE(ok->isEnabled());

  QJsonObject edited = legacy;
  edited[QStringLiteral("entrances")] = QJsonArray{QJsonObject{}};
  editor->setPlainText(QJsonDocument(edited).toJson());
  EXPECT_FALSE(dialog.is_valid());
  EXPECT_FALSE(ok->isEnabled());
  EXPECT_TRUE(projection->entrance_cells().isEmpty());
  ok->click();
  EXPECT_EQ(dialog.result(), QDialog::Rejected);

  auto* add_default = find_button(QStringLiteral("Add Default Entrance"));
  ASSERT_NE(add_default, nullptr);
  add_default->click();
  EXPECT_EQ(dialog.get_json().value(QStringLiteral("entrances")).toArray(), draft);
  EXPECT_FALSE(projection->entrance_cells().isEmpty());
  ASSERT_TRUE(ok->isEnabled());
  ok->click();
  EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(MapEditorCanvasInputTest, JSONPreviewRendersOnDemand) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QFile file(temp.filePath(QStringLiteral("canvas.json")));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  ASSERT_EQ(file.write(k_map), k_map.size());
  file.close();

  MapEditor::EditorWindow window;
  const auto* preview =
      window.findChild<QPlainTextEdit*>(QStringLiteral("mapJsonPreview"));
  ASSERT_NE(preview, nullptr);
  ASSERT_TRUE(window.load_file(file.fileName()));
  EXPECT_TRUE(preview->document()->isEmpty());
  window.select_sidebar_tab(QStringLiteral("JSON"));
  EXPECT_TRUE(preview->toPlainText().contains(QStringLiteral("Canvas input")));
}

TEST(MapEditorCanvasInputTest, CampaignPansFromASiteAndOnlyCommitsItsMoveHandle) {
  MapEditor::WorldDocument data;
  data.adopt(StrategicTest::micro_world(), nullptr, {});
  MapEditor::WorldCanvas canvas(&data);
  canvas.set_layer(MapEditor::WorldCanvas::Armies, false);
  canvas.resize(560, 520);
  canvas.show();
  const QString site_id = QStringLiteral("market");
  const auto origin = data.definition().site(site_id)->position;
  canvas.focus_on(site_id);
  const QPoint center(canvas.width() / 2, canvas.height() / 2);
  const QPoint drag(32, 16);
  click(canvas, center);
  ASSERT_EQ(data.selection(), site_id);
  pointer(canvas, QEvent::MouseButtonPress, center);
  pointer(canvas, QEvent::MouseMove, center + drag);
  pointer(canvas, QEvent::MouseButtonRelease, center + drag);
  EXPECT_FALSE(data.can_undo());
  EXPECT_EQ(data.definition().site(site_id)->position, origin);
  const QPoint handle =
      MapEditor::move_handle_rect(center + drag, canvas.rect()).center().toPoint();
  pointer(canvas, QEvent::MouseButtonPress, handle);
  pointer(canvas, QEvent::MouseMove, handle + drag);
  EXPECT_EQ(data.definition().site(site_id)->position, origin);
  pointer(canvas, QEvent::MouseButtonRelease, handle + drag);
  ASSERT_TRUE(data.can_undo());
  EXPECT_NE(data.definition().site(site_id)->position, origin);
  data.undo();
  EXPECT_EQ(data.definition().site(site_id)->position, origin);
}

} // namespace
