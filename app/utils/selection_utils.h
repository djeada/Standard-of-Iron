#pragma once

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"
#include <algorithm>
#include <vector>

namespace App::Utils {

inline void
sanitize_selection(Engine::Core::World *world,
                   Game::Systems::SelectionSystem *selection_system) {
  if ((world == nullptr) || (selection_system == nullptr)) {
    return;
  }
  const auto &sel = selection_system->get_selected_units();
  std::vector<Engine::Core::EntityID> toKeep;
  toKeep.reserve(sel.size());
  for (auto id : sel) {
    if (auto *e = world->get_entity(id)) {
      if (auto *u = e->get_component<Engine::Core::UnitComponent>()) {
        if (u->health > 0) {
          toKeep.push_back(id);
        }
      }
    }
  }
  if (toKeep.size() != sel.size() ||
      !std::equal(toKeep.begin(), toKeep.end(), sel.begin())) {
    selection_system->clear_selection();
    for (auto id : toKeep) {
      selection_system->select_unit(id);
    }
  }
}

} // namespace App::Utils
