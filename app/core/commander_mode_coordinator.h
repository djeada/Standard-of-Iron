#pragma once

#include <QVector3D>
#include <QtGlobal>

#include <optional>
#include <vector>

#include "../models/cursor_mode.h"

class CommanderControlController;
class ProductionManager;
struct ViewportState;

namespace Engine::Core {
class Entity;
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Game::Systems {
class PickingService;
class SelectionController;
} // namespace Game::Systems

namespace Render::GL {
class Camera;
} // namespace Render::GL

namespace App::Core {

struct CommanderSelectionContext {
  Engine::Core::World* world = nullptr;
  Game::Systems::SelectionController* selection_controller = nullptr;
  Engine::Core::EntityID controlled_commander_id = 0;
  int local_owner_id = 0;
  std::vector<Engine::Core::EntityID> const* saved_rts_selection_ids = nullptr;
};

struct CommanderSelectionEffects {
  std::vector<Engine::Core::EntityID> saved_rts_selection_ids;
  bool clear_saved_rts_selection_ids = false;
  bool sync_selection_flags = false;
  bool emit_selected_units_changed = false;
};

struct CommanderModeContext {
  Engine::Core::World* world = nullptr;
  Engine::Core::Entity* commander = nullptr;
  Render::GL::Camera* commander_camera = nullptr;
  CommanderControlController* commander_control = nullptr;
  Engine::Core::EntityID controlled_commander_id = 0;
  int local_owner_id = 0;
  bool is_spectator_mode = false;
  bool follow_selection_enabled = false;
  bool rts_follow_selection_snapshot_valid = false;
  bool rts_follow_selection_snapshot = false;
};

struct CommanderModeEffects {
  bool entered = false;
  Engine::Core::EntityID controlled_commander_id = 0;
  bool save_follow_selection_snapshot = false;
  bool saved_follow_selection_enabled = false;
  std::optional<float> commander_view_yaw;
  std::optional<bool> restored_follow_selection_enabled;
};

struct CommanderRallyContext {
  Engine::Core::World* world = nullptr;
  Engine::Core::Entity* local_commander = nullptr;
  Engine::Core::Entity* controlled_commander = nullptr;
  Game::Systems::PickingService* picking_service = nullptr;
  Render::GL::Camera* camera = nullptr;
  int viewport_width = 0;
  int viewport_height = 0;
  qreal screen_x = 0.0;
  qreal screen_y = 0.0;
  int local_owner_id = 0;
  bool commander_mode_active = false;
  CursorMode cursor_mode = CursorMode::Normal;
};

struct CommanderRallyEffects {
  bool should_exit_commander_mode = false;
  bool reset_commander_input = false;
  bool clear_rally_preview = false;
  bool seed_preview_from_view_center = false;
  std::optional<CursorMode> cursor_mode;
};

struct BarracksRallyContext {
  Engine::Core::World* world = nullptr;
  ProductionManager* production_manager = nullptr;
  ViewportState const* viewport = nullptr;
  int local_owner_id = 0;
  qreal screen_x = 0.0;
  qreal screen_y = 0.0;
  CursorMode cursor_mode = CursorMode::Normal;
};

struct BarracksRallyEffects {
  bool clear_rally_preview = false;
  std::optional<QVector3D> rally_preview;
  std::optional<CursorMode> cursor_mode;
};

struct CommanderUpdateContext {
  Engine::Core::World* world = nullptr;
  Render::GL::Camera* commander_camera = nullptr;
  CommanderControlController* commander_control = nullptr;
  Engine::Core::EntityID controlled_commander_id = 0;
  int local_owner_id = 0;
  float dt = 0.0F;
};

struct CommanderUpdateEffects {
  bool should_exit_commander_mode = false;
  std::optional<float> hit_stop_duration;
};

struct CommanderDirectControlContext {
  Engine::Core::World* world = nullptr;
  Engine::Core::EntityID controlled_commander_id = 0;
  bool commander_mode_active = false;
  bool placing_commander_rally = false;
};

struct CommanderDirectControlEffects {
  bool reset_commander_input = false;
};

class CommanderModeCoordinator {
public:
  [[nodiscard]] auto store_rts_selection(const CommanderSelectionContext& context) const
      -> CommanderSelectionEffects;
  void select_controlled_commander(const CommanderSelectionContext& context) const;
  [[nodiscard]] auto restore_rts_selection(
      const CommanderSelectionContext& context) const -> CommanderSelectionEffects;

  [[nodiscard]] auto enter_commander_control_mode(
      const CommanderModeContext& context) const -> CommanderModeEffects;
  [[nodiscard]] auto exit_commander_control_mode(
      const CommanderModeContext& context) const -> CommanderModeEffects;
  void clear_controlled_commander_state(const CommanderModeContext& context) const;

  [[nodiscard]] auto begin_commander_flag_rally(
      const CommanderRallyContext& context) const -> CommanderRallyEffects;
  [[nodiscard]] auto confirm_commander_flag_rally(
      const CommanderRallyContext& context) const -> CommanderRallyEffects;
  [[nodiscard]] auto
  cancel_commander_flag_rally(CursorMode cursor_mode) const -> CommanderRallyEffects;

  [[nodiscard]] auto begin_barracks_rally_placement(
      const BarracksRallyContext& context) const -> BarracksRallyEffects;
  [[nodiscard]] auto confirm_barracks_rally_placement(
      const BarracksRallyContext& context) const -> BarracksRallyEffects;
  [[nodiscard]] auto
  cancel_barracks_rally_placement(CursorMode cursor_mode) const -> BarracksRallyEffects;
  [[nodiscard]] auto seed_barracks_rally_preview_from_selection(
      const BarracksRallyContext& context) const -> std::optional<QVector3D>;

  [[nodiscard]] auto update_commander_control_mode(
      const CommanderUpdateContext& context) const -> CommanderUpdateEffects;
  [[nodiscard]] auto restore_controlled_commander_direct_control_if_ready(
      const CommanderDirectControlContext& context) const
      -> CommanderDirectControlEffects;

private:
  void clear_controlled_commander_state_impl(
      Engine::Core::World* world, Engine::Core::EntityID controlled_commander_id) const;
  [[nodiscard]] static auto
  active_commander(const CommanderRallyContext& context) -> Engine::Core::Entity*;
  [[nodiscard]] static auto has_selected_local_barracks(Engine::Core::World* world,
                                                        int local_owner_id) -> bool;
};

} // namespace App::Core
