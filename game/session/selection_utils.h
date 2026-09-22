#pragma once

#include <algorithm>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/session/selection_service.h"

namespace Game::Selection {

inline void sanitize_selection(Engine::Core::World* world,
                               Game::Session::SelectionService* selection_system) {
  if ((world == nullptr) || (selection_system == nullptr)) {
    return;
  }
  const auto& sel = selection_system->get_selected_units();
  std::vector<Engine::Core::EntityID> to_keep;
  to_keep.reserve(sel.size());
  for (auto id : sel) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(id);
    if (unit != nullptr && unit->health > 0) {
      to_keep.push_back(id);
    }
  }
  if (to_keep.size() != sel.size() ||
      !std::equal(to_keep.begin(), to_keep.end(), sel.begin())) {
    selection_system->clear_selection();
    for (auto id : to_keep) {
      selection_system->select_unit(id);
    }
  }
}

} // namespace Game::Selection
