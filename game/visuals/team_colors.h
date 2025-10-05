#pragma once
#include <QVector3D>

namespace Game::Visuals {
inline QVector3D teamColorForOwner(int ownerId) {
  switch (ownerId) {
  case 1:
    return QVector3D(0.20f, 0.55f, 1.00f);
  case 2:
    return QVector3D(1.00f, 0.30f, 0.30f);
  case 3:
    return QVector3D(0.20f, 0.80f, 0.40f);
  case 4:
    return QVector3D(1.00f, 0.80f, 0.20f);
  default:
    return QVector3D(0.8f, 0.9f, 1.0f);
  }
}
} // namespace Game::Visuals
