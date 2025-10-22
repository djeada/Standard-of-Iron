#pragma once

#include "../../game/core/entity.h"
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

class GameEngine;

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

class MinimapProvider : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList units READ units NOTIFY unitsChanged)
  Q_PROPERTY(QVariantList buildings READ buildings NOTIFY buildingsChanged)
  Q_PROPERTY(QRectF worldBounds READ worldBounds NOTIFY worldBoundsChanged)
  Q_PROPERTY(QRectF viewport READ viewport NOTIFY viewportChanged)
  Q_PROPERTY(int mapWidth READ mapWidth NOTIFY worldBoundsChanged)
  Q_PROPERTY(int mapHeight READ mapHeight NOTIFY worldBoundsChanged)

public:
  explicit MinimapProvider(GameEngine *engine, QObject *parent = nullptr);

  QVariantList units() const { return m_units; }
  QVariantList buildings() const { return m_buildings; }
  QRectF worldBounds() const { return m_worldBounds; }
  QRectF viewport() const { return m_viewport; }
  int mapWidth() const { return static_cast<int>(m_worldBounds.width()); }
  int mapHeight() const { return static_cast<int>(m_worldBounds.height()); }

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void updateViewport();
  Q_INVOKABLE QPointF worldToMinimap(qreal worldX, qreal worldZ,
                                      qreal minimapWidth,
                                      qreal minimapHeight) const;
  Q_INVOKABLE QPointF minimapToWorld(qreal minimapX, qreal minimapY,
                                      qreal minimapWidth,
                                      qreal minimapHeight) const;

signals:
  void unitsChanged();
  void buildingsChanged();
  void worldBoundsChanged();
  void viewportChanged();

private:
  void updateUnits();
  void updateBuildings();
  void updateWorldBounds();

  GameEngine *m_engine = nullptr;
  QVariantList m_units;
  QVariantList m_buildings;
  QRectF m_worldBounds;
  QRectF m_viewport;
};
