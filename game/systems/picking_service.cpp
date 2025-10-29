#include "picking_service.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/world.h"
#include <algorithm>
#include <limits>
#include <qglobal.h>
#include <qpoint.h>
#include <qvectornd.h>
#include <vector>

namespace Game::Systems {

auto PickingService::worldToScreen(const Render::GL::Camera &cam, int viewW,
                                   int viewH, const QVector3D &world,
                                   QPointF &out) -> bool {
  return cam.worldToScreen(world, qreal(viewW), qreal(viewH), out);
}

auto PickingService::screenToGround(const Render::GL::Camera &cam, int viewW,
                                    int viewH, const QPointF &screenPt,
                                    QVector3D &outWorld) -> bool {
  if (viewW <= 0 || viewH <= 0) {
    return false;
  }
  return cam.screenToGround(screenPt.x(), screenPt.y(), qreal(viewW),
                            qreal(viewH), outWorld);
}

auto PickingService::projectBounds(const Render::GL::Camera &cam,
                                   const QVector3D &center, float hx, float hz,
                                   int viewW, int viewH, QRectF &out) -> bool {
  QVector3D const corners[4] = {
      QVector3D(center.x() - hx, center.y(), center.z() - hz),
      QVector3D(center.x() + hx, center.y(), center.z() - hz),
      QVector3D(center.x() + hx, center.y(), center.z() + hz),
      QVector3D(center.x() - hx, center.y(), center.z() + hz)};
  QPointF screen_pts[4];
  for (int i = 0; i < 4; ++i) {
    if (!worldToScreen(cam, viewW, viewH, corners[i], screen_pts[i])) {
      return false;
    }
  }
  qreal minX = screen_pts[0].x();
  qreal maxX = screen_pts[0].x();
  qreal minY = screen_pts[0].y();
  qreal maxY = screen_pts[0].y();
  for (int i = 1; i < 4; ++i) {
    minX = std::min(minX, screen_pts[i].x());
    maxX = std::max(maxX, screen_pts[i].x());
    minY = std::min(minY, screen_pts[i].y());
    maxY = std::max(maxY, screen_pts[i].y());
  }
  out = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
  return true;
}

auto PickingService::updateHover(float sx, float sy, Engine::Core::World &world,
                                 const Render::GL::Camera &camera, int viewW,
                                 int viewH) -> Engine::Core::EntityID {
  if (sx < 0 || sy < 0 || sx >= viewW || sy >= viewH) {
    m_prev_hoverId = 0;
    return 0;
  }
  auto prev_hover = m_prev_hoverId;

  Engine::Core::EntityID const picked =
      pickSingle(sx, sy, world, camera, viewW, viewH, 0, false);

  if (picked != 0 && picked != prev_hover) {
    m_hoverGraceTicks = 6;
  }

  Engine::Core::EntityID current_hover = picked;
  if (current_hover == 0 && prev_hover != 0 && m_hoverGraceTicks > 0) {

    current_hover = prev_hover;
  }

  if (m_hoverGraceTicks > 0) {
    --m_hoverGraceTicks;
  }
  m_prev_hoverId = current_hover;
  return current_hover;
}

auto PickingService::pickSingle(
    float sx, float sy, Engine::Core::World &world,
    const Render::GL::Camera &camera, int viewW, int viewH, int ownerFilter,
    bool preferBuildingsFirst) -> Engine::Core::EntityID {

  const float base_unit_pick_radius = 30.0F;
  const float base_building_pick_radius = 30.0F;
  float best_unit_dist2 = std::numeric_limits<float>::max();
  float best_building_dist2 = std::numeric_limits<float>::max();
  Engine::Core::EntityID best_unit_id = 0;
  Engine::Core::EntityID best_building_id = 0;
  auto ents = world.getEntitiesWith<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->hasComponent<Engine::Core::UnitComponent>()) {
      continue;
    }
    auto *t = e->getComponent<Engine::Core::TransformComponent>();
    auto *u = e->getComponent<Engine::Core::UnitComponent>();

    if (ownerFilter != 0 && u->owner_id != ownerFilter) {
      continue;
    }

    QPointF sp;
    if (!camera.worldToScreen(
            QVector3D(t->position.x, t->position.y, t->position.z), viewW,
            viewH, sp)) {
      continue;
    }
    auto const dx = float(sx - sp.x());
    auto const dy = float(sy - sp.y());
    float const d2 = dx * dx + dy * dy;
    if (e->hasComponent<Engine::Core::BuildingComponent>()) {
      bool hit = false;
      float pick_dist2 = d2;
      const float margin_xZ = 1.6F;
      const float margin_y = 1.2F;
      float const hx = std::max(0.6F, t->scale.x * margin_xZ);
      float const hz = std::max(0.6F, t->scale.z * margin_xZ);
      float const hy = std::max(0.5F, t->scale.y * margin_y);
      QPointF pts[8];
      int ok_count = 0;
      auto project = [&](const QVector3D &w, QPointF &out) {
        return camera.worldToScreen(w, viewW, viewH, out);
      };
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x - hx, t->position.y + 0.0F,
                            t->position.z - hz),
                  pts[0]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x + hx, t->position.y + 0.0F,
                            t->position.z - hz),
                  pts[1]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x + hx, t->position.y + 0.0F,
                            t->position.z + hz),
                  pts[2]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x - hx, t->position.y + 0.0F,
                            t->position.z + hz),
                  pts[3]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x - hx, t->position.y + hy, t->position.z - hz),
          pts[4]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x + hx, t->position.y + hy, t->position.z - hz),
          pts[5]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x + hx, t->position.y + hy, t->position.z + hz),
          pts[6]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x - hx, t->position.y + hy, t->position.z + hz),
          pts[7]));
      if (ok_count == 8) {
        qreal minX = pts[0].x();
        qreal maxX = pts[0].x();
        qreal minY = pts[0].y();
        qreal maxY = pts[0].y();
        for (int i = 1; i < 8; ++i) {
          minX = std::min(minX, pts[i].x());
          maxX = std::max(maxX, pts[i].x());
          minY = std::min(minY, pts[i].y());
          maxY = std::max(maxY, pts[i].y());
        }
        if (sx >= minX && sx <= maxX && sy >= minY && sy <= maxY) {
          hit = true;
          pick_dist2 = d2;
        }
      }
      if (!hit) {
        float const scale_xZ = std::max(std::max(t->scale.x, t->scale.z), 1.0F);
        float const rp = base_building_pick_radius * scale_xZ;
        float const r2 = rp * rp;
        if (d2 <= r2) {
          hit = true;
        }
      }
      if (hit && pick_dist2 < best_building_dist2) {
        best_building_dist2 = pick_dist2;
        best_building_id = e->getId();
      }
    } else {
      float const r2 = base_unit_pick_radius * base_unit_pick_radius;
      if (d2 <= r2 && d2 < best_unit_dist2) {
        best_unit_dist2 = d2;
        best_unit_id = e->getId();
      }
    }
  }
  if (preferBuildingsFirst) {
    if ((best_building_id != 0U) &&
        ((best_unit_id == 0U) || best_building_dist2 <= best_unit_dist2)) {
      return best_building_id;
    }
    if (best_unit_id != 0U) {
      return best_unit_id;
    }
  } else {
    if (best_unit_id != 0U) {
      return best_unit_id;
    }
    if (best_building_id != 0U) {
      return best_building_id;
    }
  }
  return 0;
}

auto PickingService::pickUnitFirst(float sx, float sy,
                                   Engine::Core::World &world,
                                   const Render::GL::Camera &camera, int viewW,
                                   int viewH,
                                   int ownerFilter) -> Engine::Core::EntityID {

  auto id = pickSingle(sx, sy, world, camera, viewW, viewH, ownerFilter, false);
  if (id != 0) {
    return id;
  }

  return pickSingle(sx, sy, world, camera, viewW, viewH, ownerFilter, true);
}

auto PickingService::pickInRect(
    float x1, float y1, float x2, float y2, Engine::Core::World &world,
    const Render::GL::Camera &camera, int viewW, int viewH,
    int ownerFilter) -> std::vector<Engine::Core::EntityID> {
  float const minX = std::min(x1, x2);
  float const maxX = std::max(x1, x2);
  float const minY = std::min(y1, y2);
  float const maxY = std::max(y1, y2);
  std::vector<Engine::Core::EntityID> picked;
  auto ents = world.getEntitiesWith<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->hasComponent<Engine::Core::UnitComponent>()) {
      continue;
    }
    if (e->hasComponent<Engine::Core::BuildingComponent>()) {
      continue;
    }
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if ((u == nullptr) || u->owner_id != ownerFilter) {
      continue;
    }
    auto *t = e->getComponent<Engine::Core::TransformComponent>();
    QPointF sp;
    if (!camera.worldToScreen(
            QVector3D(t->position.x, t->position.y, t->position.z), viewW,
            viewH, sp)) {
      continue;
    }
    if (sp.x() >= minX && sp.x() <= maxX && sp.y() >= minY && sp.y() <= maxY) {
      picked.push_back(e->getId());
    }
  }
  return picked;
}

} // namespace Game::Systems
