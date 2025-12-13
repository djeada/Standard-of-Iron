#pragma once
#include "../core/ownership_constants.h"
#include "../systems/owner_registry.h"
#include <QVector3D>

namespace Game::Visuals {
inline auto team_colorForOwner(int owner_id) -> QVector3D {

  if (Game::Core::isNeutralOwner(owner_id)) {
    return {0.5F, 0.5F, 0.5F};
  }

  auto &registry = Game::Systems::OwnerRegistry::instance();
  auto color = registry.get_owner_color(owner_id);
  return {color[0], color[1], color[2]};
}
} // namespace Game::Visuals
