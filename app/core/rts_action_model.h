#pragma once

#include <QString>
#include <QVariantMap>

#include <vector>

#include "../models/cursor_mode.h"

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace App::Core {

struct ActionContext {
  Engine::Core::World* world = nullptr;
  CursorMode cursor_mode = CursorMode::Normal;
  bool placing_construction = false;
  QString pending_builder_construction_type;
  bool placing_formation = false;
  bool has_patrol_first_waypoint = false;
};

[[nodiscard]] auto get_action_states(const ActionContext& context) -> QVariantMap;
[[nodiscard]] auto get_current_action_mode(const ActionContext& context) -> QString;
[[nodiscard]] auto get_toggle_state(Engine::Core::World* world,
                                    const QString& action_id) -> QString;
[[nodiscard]] auto get_mode_availability(Engine::Core::World* world) -> QVariantMap;
[[nodiscard]] auto get_selection_command_mode(Engine::Core::World* world) -> QString;
[[nodiscard]] auto filter_selected_units_for_action(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selected,
    const QString& action_id) -> std::vector<Engine::Core::EntityID>;
[[nodiscard]] auto action_id_for_cursor_mode(CursorMode mode) -> QString;

} // namespace App::Core
