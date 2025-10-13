#pragma once

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"
#include <algorithm>
#include <vector>

namespace App {
namespace Utils {

inline void sanitizeSelection(Engine::Core::World *world,
                              Game::Systems::SelectionSystem *selectionSystem) {
  if (!world || !selectionSystem)
    return;
  const auto &sel = selectionSystem->getSelectedUnits();
  std::vector<Engine::Core::EntityID> toKeep;
  toKeep.reserve(sel.size());
  for (auto id : sel) {
    if (auto *e = world->getEntity(id)) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (u->health > 0)
          toKeep.push_back(id);
      }
    }
  }
  if (toKeep.size() != sel.size() ||
      !std::equal(toKeep.begin(), toKeep.end(), sel.begin())) {
    selectionSystem->clearSelection();
    for (auto id : toKeep)
      selectionSystem->selectUnit(id);
  }
}

} // namespace Utils
} // namespace App
