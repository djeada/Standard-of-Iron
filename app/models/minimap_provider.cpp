#include "minimap_provider.h"
#include "../core/game_engine.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/owner_registry.h"
#include "../../game/visuals/team_colors.h"
#include "../../game/units/troop_type.h"
#include "../../render/gl/camera.h"
#include <QColor>

MinimapProvider::MinimapProvider(GameEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine) {
  if (m_engine) {
    updateWorldBounds();
  }
}

void MinimapProvider::refresh() {
  if (!m_engine) {
    return;
  }
  updateUnits();
  updateBuildings();
  updateViewport();
}

void MinimapProvider::updateUnits() {
  QVariantList newUnits;

  if (!m_engine) {
    m_units = newUnits;
    emit unitsChanged();
    return;
  }

  auto *world = m_engine->getWorld();
  if (!world) {
    m_units = newUnits;
    emit unitsChanged();
    return;
  }

  // Iterate through all entities
  auto &entities = world->getEntities();
  for (const auto &entity : entities) {
    if (!entity)
      continue;

    // Check if entity is a unit (has health, ownership, and is not a building)
    auto *health = entity->get<Engine::Core::HealthComponent>();
    auto *ownership = entity->get<Engine::Core::OwnershipComponent>();
    auto *transform = entity->get<Engine::Core::TransformComponent>();
    auto *troopType = entity->get<Game::Units::TroopType>();

    if (!health || !ownership || !transform || !troopType)
      continue;

    // Skip if dead
    if (health->health <= 0)
      continue;

    // Get color based on owner
    auto color = Game::Visuals::teamColorForOwner(ownership->ownerId);
    QColor qcolor(static_cast<int>(color.x() * 255),
                   static_cast<int>(color.y() * 255),
                   static_cast<int>(color.z() * 255));

    QVariantMap unit;
    unit["x"] = transform->position.x();
    unit["z"] = transform->position.z();
    unit["color"] = qcolor.name();
    unit["ownerId"] = ownership->ownerId;

    newUnits.append(unit);
  }

  m_units = newUnits;
  emit unitsChanged();
}

void MinimapProvider::updateBuildings() {
  QVariantList newBuildings;

  if (!m_engine) {
    m_buildings = newBuildings;
    emit buildingsChanged();
    return;
  }

  auto *world = m_engine->getWorld();
  if (!world) {
    m_buildings = newBuildings;
    emit buildingsChanged();
    return;
  }

  // Iterate through all entities looking for buildings
  auto &entities = world->getEntities();
  for (const auto &entity : entities) {
    if (!entity)
      continue;

    // Check if entity is a building (has building component)
    auto *building = entity->get<Engine::Core::BuildingComponent>();
    auto *ownership = entity->get<Engine::Core::OwnershipComponent>();
    auto *transform = entity->get<Engine::Core::TransformComponent>();
    auto *health = entity->get<Engine::Core::HealthComponent>();

    if (!building || !ownership || !transform || !health)
      continue;

    // Skip if dead
    if (health->health <= 0)
      continue;

    // Get color based on owner
    auto color = Game::Visuals::teamColorForOwner(ownership->ownerId);
    QColor qcolor(static_cast<int>(color.x() * 255),
                   static_cast<int>(color.y() * 255),
                   static_cast<int>(color.z() * 255));

    QVariantMap buildingData;
    buildingData["x"] = transform->position.x();
    buildingData["z"] = transform->position.z();
    buildingData["color"] = qcolor.name();
    buildingData["ownerId"] = ownership->ownerId;

    newBuildings.append(buildingData);
  }

  m_buildings = newBuildings;
  emit buildingsChanged();
}

void MinimapProvider::updateWorldBounds() {
  // Get actual world bounds from the map/terrain
  // For now, use reasonable defaults - can be updated later with actual map data
  m_worldBounds = QRectF(-150.0, -150.0, 300.0, 300.0);
  emit worldBoundsChanged();
}

void MinimapProvider::updateViewport() {
  if (!m_engine) {
    return;
  }

  auto *camera = m_engine->getCamera();
  if (!camera) {
    return;
  }

  // Get camera target position and calculate visible area
  // This is a simplified calculation - adjust based on actual camera frustum
  QVector3D target = camera->target();
  float distance = camera->distance();

  // Estimate visible area based on camera distance
  float halfWidth = distance * 0.7f;  // Adjust multiplier as needed
  float halfHeight = distance * 0.5f;

  m_viewport = QRectF(target.x() - halfWidth, target.z() - halfHeight,
                       halfWidth * 2.0f, halfHeight * 2.0f);
  emit viewportChanged();
}

QPointF MinimapProvider::worldToMinimap(qreal worldX, qreal worldZ,
                                         qreal minimapWidth,
                                         qreal minimapHeight) const {
  if (m_worldBounds.width() <= 0 || m_worldBounds.height() <= 0) {
    return QPointF(0, 0);
  }

  qreal normX = (worldX - m_worldBounds.left()) / m_worldBounds.width();
  qreal normZ = (worldZ - m_worldBounds.top()) / m_worldBounds.height();

  return QPointF(normX * minimapWidth, normZ * minimapHeight);
}

QPointF MinimapProvider::minimapToWorld(qreal minimapX, qreal minimapY,
                                         qreal minimapWidth,
                                         qreal minimapHeight) const {
  if (minimapWidth <= 0 || minimapHeight <= 0) {
    return QPointF(0, 0);
  }

  qreal normX = minimapX / minimapWidth;
  qreal normZ = minimapY / minimapHeight;

  qreal worldX = m_worldBounds.left() + normX * m_worldBounds.width();
  qreal worldZ = m_worldBounds.top() + normZ * m_worldBounds.height();

  return QPointF(worldX, worldZ);
}
