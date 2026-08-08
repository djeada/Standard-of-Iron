#include <gtest/gtest.h>

#include "tools/map_editor/canvas_input.h"

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

} // namespace
