#include "picking_service.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/world.h"
#include <algorithm>

namespace Game {
namespace Systems {

bool PickingService::worldToScreen(const Render::GL::Camera &cam, int viewW,
                                   int viewH, const QVector3D &world,
                                   QPointF &out) const {
  return cam.worldToScreen(world, viewW, viewH, out);
}

bool PickingService::screenToGround(const Render::GL::Camera &cam, int viewW,
                                    int viewH, const QPointF &screenPt,
                                    QVector3D &outWorld) const {
  if (viewW <= 0 || viewH <= 0)
    return false;
  return cam.screenToGround(float(screenPt.x()), float(screenPt.y()),
                            float(viewW), float(viewH), outWorld);
}

bool PickingService::projectBounds(const Render::GL::Camera &cam,
                                   const QVector3D &center, float hx, float hz,
                                   int viewW, int viewH, QRectF &out) const {
  QVector3D corners[4] = {
      QVector3D(center.x() - hx, center.y(), center.z() - hz),
      QVector3D(center.x() + hx, center.y(), center.z() - hz),
      QVector3D(center.x() + hx, center.y(), center.z() + hz),
      QVector3D(center.x() - hx, center.y(), center.z() + hz)};
  QPointF screenPts[4];
  for (int i = 0; i < 4; ++i) {
    if (!worldToScreen(cam, viewW, viewH, corners[i], screenPts[i]))
      return false;
  }
  float minX = screenPts[0].x(), maxX = screenPts[0].x();
  float minY = screenPts[0].y(), maxY = screenPts[0].y();
  for (int i = 1; i < 4; ++i) {
    minX = std::min(minX, float(screenPts[i].x()));
    maxX = std::max(maxX, float(screenPts[i].x()));
    minY = std::min(minY, float(screenPts[i].y()));
    maxY = std::max(maxY, float(screenPts[i].y()));
  }
  out = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
  return true;
}

Engine::Core::EntityID
PickingService::updateHover(float sx, float sy, Engine::Core::World &world,
                            const Render::GL::Camera &camera, int viewW,
                            int viewH) {
  if (sx < 0 || sy < 0) {
    m_prevHoverId = 0;
    return 0;
  }
  auto prevHover = m_prevHoverId;
  Engine::Core::EntityID currentHover = 0;

  if (prevHover) {
    if (auto *e = world.getEntity(prevHover)) {
      if (e->hasComponent<Engine::Core::BuildingComponent>()) {

        auto *u = e->getComponent<Engine::Core::UnitComponent>();
        if (u && u->ownerId == 1) {
          if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
            float pxPad = 12.0f;
            if (auto *prod =
                    e->getComponent<Engine::Core::ProductionComponent>()) {
              if (prod->inProgress)
                pxPad = 24.0f;
            }
            const float marginXZ_keep = 1.10f;
            const float pad_keep = 1.12f;
            float hxk = std::max(0.4f, t->scale.x * marginXZ_keep * pad_keep);
            float hzk = std::max(0.4f, t->scale.z * marginXZ_keep * pad_keep);
            QRectF bounds;
            if (projectBounds(
                    camera,
                    QVector3D(t->position.x, t->position.y, t->position.z), hxk,
                    hzk, viewW, viewH, bounds)) {
              bounds.adjust(-pxPad, -pxPad, pxPad, pxPad);
              if (bounds.contains(QPointF(sx, sy))) {
                currentHover = prevHover;
                m_hoverGraceTicks = 6;
                m_prevHoverId = currentHover;
                return currentHover;
              }
            }
          }
        }
      }
    }
  }

  float bestD2 = std::numeric_limits<float>::max();
  auto ents = world.getEntitiesWith<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->hasComponent<Engine::Core::UnitComponent>())
      continue;
    if (!e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != 1)
      continue;

    auto *t = e->getComponent<Engine::Core::TransformComponent>();
    const float marginXZ = 1.10f;
    const float hoverPad = 1.06f;
    float hx = std::max(0.4f, t->scale.x * marginXZ * hoverPad);
    float hz = std::max(0.4f, t->scale.z * marginXZ * hoverPad);
    QRectF bounds;
    if (!projectBounds(camera,
                       QVector3D(t->position.x, t->position.y, t->position.z),
                       hx, hz, viewW, viewH, bounds))
      continue;
    if (!bounds.contains(QPointF(sx, sy)))
      continue;
    QPointF centerSp;
    if (!worldToScreen(camera, viewW, viewH,
                       QVector3D(t->position.x, t->position.y, t->position.z),
                       centerSp))
      centerSp = bounds.center();
    float dx = float(sx) - float(centerSp.x());
    float dy = float(sy) - float(centerSp.y());
    float d2 = dx * dx + dy * dy;
    if (d2 < bestD2) {
      bestD2 = d2;
      currentHover = e->getId();
    }
  }

  if (currentHover != 0 && currentHover != prevHover) {
    m_hoverGraceTicks = 6;
  }

  if (currentHover == 0 && prevHover != 0) {
    if (auto *e = world.getEntity(prevHover)) {
      auto *t = e->getComponent<Engine::Core::TransformComponent>();
      auto *u = e->getComponent<Engine::Core::UnitComponent>();
      if (t && e->getComponent<Engine::Core::BuildingComponent>() && u &&
          u->ownerId == 1) {
        const float marginXZ = 1.12f;
        const float keepPad =
            (e->getComponent<Engine::Core::ProductionComponent>() &&
             e->getComponent<Engine::Core::ProductionComponent>()->inProgress)
                ? 1.16f
                : 1.12f;
        float hx = std::max(0.4f, t->scale.x * marginXZ * keepPad);
        float hz = std::max(0.4f, t->scale.z * marginXZ * keepPad);
        QRectF bounds;
        if (projectBounds(
                camera, QVector3D(t->position.x, t->position.y, t->position.z),
                hx, hz, viewW, viewH, bounds)) {
          if (bounds.contains(QPointF(sx, sy))) {
            currentHover = prevHover;
          }
        }
      }
    }
  }

  if (currentHover == 0 && prevHover != 0 && m_hoverGraceTicks > 0) {
    currentHover = prevHover;
  }

  if (m_hoverGraceTicks > 0)
    --m_hoverGraceTicks;
  m_prevHoverId = currentHover;
  return currentHover;
}

Engine::Core::EntityID
PickingService::pickSingle(float sx, float sy, Engine::Core::World &world,
                           const Render::GL::Camera &camera, int viewW,
                           int viewH, int ownerFilter,
                           bool preferBuildingsFirst) const {
  const float baseUnitPickRadius = 18.0f;
  const float baseBuildingPickRadius = 28.0f;
  float bestUnitDist2 = std::numeric_limits<float>::max();
  float bestBuildingDist2 = std::numeric_limits<float>::max();
  Engine::Core::EntityID bestUnitId = 0;
  Engine::Core::EntityID bestBuildingId = 0;
  auto ents = world.getEntitiesWith<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->hasComponent<Engine::Core::UnitComponent>())
      continue;
    auto *t = e->getComponent<Engine::Core::TransformComponent>();
    auto *u = e->getComponent<Engine::Core::UnitComponent>();

    if (ownerFilter != 0 && u->ownerId != ownerFilter)
      continue;

    QPointF sp;
    if (!camera.worldToScreen(
            QVector3D(t->position.x, t->position.y, t->position.z), viewW,
            viewH, sp))
      continue;
    float dx = float(sx) - float(sp.x());
    float dy = float(sy) - float(sp.y());
    float d2 = dx * dx + dy * dy;
    if (e->hasComponent<Engine::Core::BuildingComponent>()) {
      bool hit = false;
      float pickDist2 = d2;
      const float marginXZ = 1.6f;
      const float marginY = 1.2f;
      float hx = std::max(0.6f, t->scale.x * marginXZ);
      float hz = std::max(0.6f, t->scale.z * marginXZ);
      float hy = std::max(0.5f, t->scale.y * marginY);
      QPointF pts[8];
      int okCount = 0;
      auto project = [&](const QVector3D &w, QPointF &out) {
        return camera.worldToScreen(w, viewW, viewH, out);
      };
      okCount += project(QVector3D(t->position.x - hx, t->position.y + 0.0f,
                                   t->position.z - hz),
                         pts[0]);
      okCount += project(QVector3D(t->position.x + hx, t->position.y + 0.0f,
                                   t->position.z - hz),
                         pts[1]);
      okCount += project(QVector3D(t->position.x + hx, t->position.y + 0.0f,
                                   t->position.z + hz),
                         pts[2]);
      okCount += project(QVector3D(t->position.x - hx, t->position.y + 0.0f,
                                   t->position.z + hz),
                         pts[3]);
      okCount += project(
          QVector3D(t->position.x - hx, t->position.y + hy, t->position.z - hz),
          pts[4]);
      okCount += project(
          QVector3D(t->position.x + hx, t->position.y + hy, t->position.z - hz),
          pts[5]);
      okCount += project(
          QVector3D(t->position.x + hx, t->position.y + hy, t->position.z + hz),
          pts[6]);
      okCount += project(
          QVector3D(t->position.x - hx, t->position.y + hy, t->position.z + hz),
          pts[7]);
      if (okCount == 8) {
        float minX = pts[0].x(), maxX = pts[0].x();
        float minY = pts[0].y(), maxY = pts[0].y();
        for (int i = 1; i < 8; ++i) {
          minX = std::min(minX, float(pts[i].x()));
          maxX = std::max(maxX, float(pts[i].x()));
          minY = std::min(minY, float(pts[i].y()));
          maxY = std::max(maxY, float(pts[i].y()));
        }
        if (float(sx) >= minX && float(sx) <= maxX && float(sy) >= minY &&
            float(sy) <= maxY) {
          hit = true;
          pickDist2 = d2;
        }
      }
      if (!hit) {
        float scaleXZ = std::max(std::max(t->scale.x, t->scale.z), 1.0f);
        float rp = baseBuildingPickRadius * scaleXZ;
        float r2 = rp * rp;
        if (d2 <= r2)
          hit = true;
      }
      if (hit && pickDist2 < bestBuildingDist2) {
        bestBuildingDist2 = pickDist2;
        bestBuildingId = e->getId();
      }
    } else {
      float r2 = baseUnitPickRadius * baseUnitPickRadius;
      if (d2 <= r2 && d2 < bestUnitDist2) {
        bestUnitDist2 = d2;
        bestUnitId = e->getId();
      }
    }
  }
  if (preferBuildingsFirst) {
    if (bestBuildingId && (!bestUnitId || bestBuildingDist2 <= bestUnitDist2))
      return bestBuildingId;
    if (bestUnitId)
      return bestUnitId;
  } else {
    if (bestUnitId)
      return bestUnitId;
    if (bestBuildingId)
      return bestBuildingId;
  }
  return 0;
}

std::vector<Engine::Core::EntityID>
PickingService::pickInRect(float x1, float y1, float x2, float y2,
                           Engine::Core::World &world,
                           const Render::GL::Camera &camera, int viewW,
                           int viewH, int ownerFilter) const {
  float minX = std::min(x1, x2);
  float maxX = std::max(x1, x2);
  float minY = std::min(y1, y2);
  float maxY = std::max(y1, y2);
  std::vector<Engine::Core::EntityID> picked;
  auto ents = world.getEntitiesWith<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->hasComponent<Engine::Core::UnitComponent>())
      continue;
    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u || u->ownerId != ownerFilter)
      continue;
    auto *t = e->getComponent<Engine::Core::TransformComponent>();
    QPointF sp;
    if (!camera.worldToScreen(
            QVector3D(t->position.x, t->position.y, t->position.z), viewW,
            viewH, sp))
      continue;
    if (sp.x() >= minX && sp.x() <= maxX && sp.y() >= minY && sp.y() <= maxY) {
      picked.push_back(e->getId());
    }
  }
  return picked;
}

} // namespace Systems
} // namespace Game
