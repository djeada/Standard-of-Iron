#pragma once

#include <QVariant>
#include <QVector3D>

#include <functional>
#include <optional>
#include <vector>

#include "app/input/cursor_mode.h"
#include "app/orders/order_markers.h"
#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/target_focus.h"

class CursorManager;
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

  const Game::Systems::AttackTargetingHighlights* attack_targeting = nullptr;
  const std::vector<Game::Systems::AttackRangeRing>* attack_range_rings = nullptr;
  const std::vector<App::Core::OrderMarker>* order_markers = nullptr;
  const std::vector<Game::Systems::TargetFocusMarker>* target_focus = nullptr;
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

} // namespace App::Core::FrameUiCoordinator
