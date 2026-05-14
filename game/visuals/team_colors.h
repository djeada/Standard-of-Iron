#pragma once
#include <QVector3D>

#include "../core/ownership_constants.h"
#include "../systems/owner_registry.h"

namespace Game::Visuals {
inline auto team_colorForOwner(int owner_id) -> QVector3D {

  if (Game::Core::is_neutral_owner(owner_id)) {
    return {0.5F, 0.5F, 0.5F};
  }

  auto& registry = Game::Systems::OwnerRegistry::instance();
  auto color = registry.get_owner_color(owner_id);
  return {color[0], color[1], color[2]};
}
} // namespace Game::Visuals
