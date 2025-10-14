#pragma once
#include "../systems/owner_registry.h"
#include <QVector3D>

namespace Game::Visuals {
inline QVector3D teamColorForOwner(int ownerId) {

  auto &registry = Game::Systems::OwnerRegistry::instance();
  auto color = registry.getOwnerColor(ownerId);
  return QVector3D(color[0], color[1], color[2]);
}
} 
