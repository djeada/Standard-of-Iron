#pragma once

#include <QVariant>
#include <QVector3D>

#include <functional>
#include <optional>

#include "../models/cursor_mode.h"

class CursorManager;
class HoverTracker;
class ProductionManager;

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
}

namespace App::Controllers {
class CommandController;
}

namespace App::Core::FrameUiCoordinator {

struct RenderEffectsContext {
  Render::GL::Renderer* renderer = nullptr;
  Engine::Core::World* world = nullptr;
  App::Controllers::CommandController* command_controller = nullptr;
  int local_owner_id = 0;
  std::optional<QVector3D> commander_rally_preview_pos;
};

void render_effects(const RenderEffectsContext& context,
                    const std::function<void()>& render_runtime_mode_effects);

enum class CursorResolution {
  None,
  CancelBarracksRallyPlacement,
  CancelCommanderFlagRally,
  ResetToNormal
};

struct SelectionPruneContext {
  Engine::Core::World* world = nullptr;
  CursorManager* cursor_manager = nullptr;
  ProductionManager* production_manager = nullptr;
  App::Controllers::CommandController* command_controller = nullptr;
  int local_owner_id = 0;
  QVariantMap hud_action_states;
};

struct SelectionPruneEffects {
  bool cancel_construction = false;
  bool cancel_formation = false;
  bool clear_patrol_first_waypoint = false;
  CursorResolution cursor_resolution = CursorResolution::None;
};

[[nodiscard]] auto prune_selection_action_context(const SelectionPruneContext& context)
    -> SelectionPruneEffects;

struct CivilianDeliveryContext {
  Engine::Core::World* world = nullptr;
  HoverTracker* hover_tracker = nullptr;
  int local_owner_id = 0;
};

[[nodiscard]] auto
civilian_delivery_available(const CivilianDeliveryContext& context) -> bool;

} // namespace App::Core::FrameUiCoordinator
