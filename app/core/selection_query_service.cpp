#include "selection_query_service.h"

#include "rts_action_model.h"

SelectionQueryService::SelectionQueryService(Engine::Core::World* world,
                                             QObject* parent)
    : QObject(parent)
    , m_world(world) {
}

auto SelectionQueryService::get_selected_units_command_mode() const -> QString {
  return App::Core::get_selection_command_mode(m_world);
}

auto SelectionQueryService::get_selected_units_toggle_state(const QString& mode) const
    -> QString {
  return App::Core::get_toggle_state(m_world, mode);
}

auto SelectionQueryService::get_selected_units_mode_availability() const
    -> QVariantMap {
  return App::Core::get_mode_availability(m_world);
}
