#pragma once

#include "tool_type.h"

namespace MapEditor {

enum class RightClickAction {
  CancelLinearDraw,
  ClearTool,
  ShowContextMenu,
};

[[nodiscard]] inline auto right_click_action(bool placing_linear,
                                             ToolType tool) -> RightClickAction {
  if (placing_linear) {
    return RightClickAction::CancelLinearDraw;
  }
  if (tool != ToolType::Select) {
    return RightClickAction::ClearTool;
  }
  return RightClickAction::ShowContextMenu;
}

} // namespace MapEditor
