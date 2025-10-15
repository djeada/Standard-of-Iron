#pragma once
#include "../core/ownership_constants.h"
#include "../systems/owner_registry.h"
#include <QVector3D>

namespace Game::Visuals {
inline QVector3D teamColorForOwner(int ownerId) {

  if (Game::Core::isNeutralOwner(ownerId)) {
    return QVector3D(0.5f, 0.5f, 0.5f);
  }

  auto &registry = Game::Systems::OwnerRegistry::instance();
  auto color = registry.getOwnerColor(ownerId);
  return QVector3D(color[0], color[1], color[2]);
}
} // namespace Game::Visuals
