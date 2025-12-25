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

auto PickingService::world_to_screen(const Render::GL::Camera &cam, int view_w,
                                     int view_h, const QVector3D &world,
                                     QPointF &out) -> bool {
  return cam.world_to_screen(world, qreal(view_w), qreal(view_h), out);
}

auto PickingService::screen_to_ground(const Render::GL::Camera &cam, int view_w,
                                      int view_h, const QPointF &screen_pt,
                                      QVector3D &out_world) -> bool {
  if (view_w <= 0 || view_h <= 0) {
    return false;
  }
  return cam.screen_to_ground(screen_pt.x(), screen_pt.y(), qreal(view_w),
                              qreal(view_h), out_world);
}

auto PickingService::project_bounds(const Render::GL::Camera &cam,
                                    const QVector3D &center, float hx, float hz,
                                    int view_w, int view_h,
                                    QRectF &out) -> bool {
  QVector3D const corners[4] = {
      QVector3D(center.x() - hx, center.y(), center.z() - hz),
      QVector3D(center.x() + hx, center.y(), center.z() - hz),
      QVector3D(center.x() + hx, center.y(), center.z() + hz),
      QVector3D(center.x() - hx, center.y(), center.z() + hz)};
  QPointF screen_pts[4];
  for (int i = 0; i < 4; ++i) {
    if (!world_to_screen(cam, view_w, view_h, corners[i], screen_pts[i])) {
      return false;
    }
  }
  qreal min_x = screen_pts[0].x();
  qreal max_x = screen_pts[0].x();
  qreal min_y = screen_pts[0].y();
  qreal max_y = screen_pts[0].y();
  for (int i = 1; i < 4; ++i) {
    min_x = std::min(min_x, screen_pts[i].x());
    max_x = std::max(max_x, screen_pts[i].x());
    min_y = std::min(min_y, screen_pts[i].y());
    max_y = std::max(max_y, screen_pts[i].y());
  }
  out = QRectF(QPointF(min_x, min_y), QPointF(max_x, max_y));
  return true;
}

auto PickingService::update_hover(float sx, float sy,
                                  Engine::Core::World &world,
                                  const Render::GL::Camera &camera, int view_w,
                                  int view_h) -> Engine::Core::EntityID {
  if (sx < 0 || sy < 0 || sx >= view_w || sy >= view_h) {
    m_prev_hoverId = 0;
    return 0;
  }
  auto prev_hover = m_prev_hoverId;

  Engine::Core::EntityID const picked =
      pick_single(sx, sy, world, camera, view_w, view_h, 0, false);

  if (picked != 0 && picked != prev_hover) {
    m_hover_grace_ticks = 6;
  }

  Engine::Core::EntityID current_hover = picked;
  if (current_hover == 0 && prev_hover != 0 && m_hover_grace_ticks > 0) {

    current_hover = prev_hover;
  }

  if (m_hover_grace_ticks > 0) {
    --m_hover_grace_ticks;
  }
  m_prev_hoverId = current_hover;
  return current_hover;
}

auto PickingService::pick_single(
    float sx, float sy, Engine::Core::World &world,
    const Render::GL::Camera &camera, int view_w, int view_h, int owner_filter,
    bool prefer_buildings_first) -> Engine::Core::EntityID {

  const float base_unit_pick_radius = 30.0F;
  const float base_building_pick_radius = 30.0F;
  float best_unit_dist2 = std::numeric_limits<float>::max();
  float best_building_dist2 = std::numeric_limits<float>::max();
  Engine::Core::EntityID best_unit_id = 0;
  Engine::Core::EntityID best_building_id = 0;
  auto ents = world.get_entities_with<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->has_component<Engine::Core::UnitComponent>()) {
      continue;
    }
    auto *t = e->get_component<Engine::Core::TransformComponent>();
    auto *u = e->get_component<Engine::Core::UnitComponent>();

    if (owner_filter != 0 && u->owner_id != owner_filter) {
      continue;
    }

    QPointF sp;
    if (!camera.world_to_screen(
            QVector3D(t->position.x, t->position.y, t->position.z), view_w,
            view_h, sp)) {
      continue;
    }
    auto const dx = float(sx - sp.x());
    auto const dy = float(sy - sp.y());
    float const d2 = dx * dx + dy * dy;
    if (e->has_component<Engine::Core::BuildingComponent>()) {
      bool hit = false;
      float pick_dist2 = d2;
      const float margin_x_z = 1.6F;
      const float margin_y = 1.2F;
      float const hx = std::max(0.6F, t->scale.x * margin_x_z);
      float const hz = std::max(0.6F, t->scale.z * margin_x_z);
      float const hy = std::max(0.5F, t->scale.y * margin_y);
      QPointF pts[8];
      int ok_count = 0;
      auto project = [&](const QVector3D &w, QPointF &out) {
        return camera.world_to_screen(w, view_w, view_h, out);
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
        qreal min_x = pts[0].x();
        qreal max_x = pts[0].x();
        qreal min_y = pts[0].y();
        qreal max_y = pts[0].y();
        for (int i = 1; i < 8; ++i) {
          min_x = std::min(min_x, pts[i].x());
          max_x = std::max(max_x, pts[i].x());
          min_y = std::min(min_y, pts[i].y());
          max_y = std::max(max_y, pts[i].y());
        }
        if (sx >= min_x && sx <= max_x && sy >= min_y && sy <= max_y) {
          hit = true;
          pick_dist2 = d2;
        }
      }
      if (!hit) {
        float const scale_x_z =
            std::max(std::max(t->scale.x, t->scale.z), 1.0F);
        float const rp = base_building_pick_radius * scale_x_z;
        float const r2 = rp * rp;
        if (d2 <= r2) {
          hit = true;
        }
      }
      if (hit && pick_dist2 < best_building_dist2) {
        best_building_dist2 = pick_dist2;
        best_building_id = e->get_id();
      }
    } else {
      float const r2 = base_unit_pick_radius * base_unit_pick_radius;
      if (d2 <= r2 && d2 < best_unit_dist2) {
        best_unit_dist2 = d2;
        best_unit_id = e->get_id();
      }
    }
  }
  if (prefer_buildings_first) {
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

auto PickingService::pick_unit_first(
    float sx, float sy, Engine::Core::World &world,
    const Render::GL::Camera &camera, int view_w, int view_h,
    int owner_filter) -> Engine::Core::EntityID {

  auto id =
      pick_single(sx, sy, world, camera, view_w, view_h, owner_filter, false);
  if (id != 0) {
    return id;
  }

  return pick_single(sx, sy, world, camera, view_w, view_h, owner_filter, true);
}

auto PickingService::pick_in_rect(
    float x1, float y1, float x2, float y2, Engine::Core::World &world,
    const Render::GL::Camera &camera, int view_w, int view_h,
    int owner_filter) -> std::vector<Engine::Core::EntityID> {
  float const min_x = std::min(x1, x2);
  float const max_x = std::max(x1, x2);
  float const min_y = std::min(y1, y2);
  float const max_y = std::max(y1, y2);
  std::vector<Engine::Core::EntityID> picked;
  auto ents = world.get_entities_with<Engine::Core::TransformComponent>();
  for (auto *e : ents) {
    if (!e->has_component<Engine::Core::UnitComponent>()) {
      continue;
    }
    if (e->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    auto *u = e->get_component<Engine::Core::UnitComponent>();
    if ((u == nullptr) || u->owner_id != owner_filter) {
      continue;
    }
    auto *t = e->get_component<Engine::Core::TransformComponent>();
    QPointF sp;
    if (!camera.world_to_screen(
            QVector3D(t->position.x, t->position.y, t->position.z), view_w,
            view_h, sp)) {
      continue;
    }
    if (sp.x() >= min_x && sp.x() <= max_x && sp.y() >= min_y &&
        sp.y() <= max_y) {
      picked.push_back(e->get_id());
    }
  }
  return picked;
}

} 
