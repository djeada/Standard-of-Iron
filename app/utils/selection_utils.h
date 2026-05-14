#pragma once

#include <algorithm>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"

namespace App::Utils {

inline void sanitize_selection(Engine::Core::World* world,
                               Game::Systems::SelectionSystem* selection_system) {
  if ((world == nullptr) || (selection_system == nullptr)) {
    return;
  }
  const auto& sel = selection_system->get_selected_units();
  std::vector<Engine::Core::EntityID> to_keep;
  to_keep.reserve(sel.size());
  for (auto id : sel) {
    if (auto* e = world->get_entity(id)) {
      if (auto* u = e->get_component<Engine::Core::UnitComponent>()) {
        if (u->health > 0) {
          to_keep.push_back(id);
        }
      }
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

} // namespace App::Utils
