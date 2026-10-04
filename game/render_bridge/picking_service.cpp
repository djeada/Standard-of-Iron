#include "picking_service.h"

#include <qglobal.h>
#include <qpoint.h>
#include <qvectornd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/component_structures.h"
#include "../core/wall_walk_geometry.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../systems/building_collision_registry.h"
#include "../units/spawn_type.h"
#include "scene/camera.h"

namespace Game::Systems {

namespace {

constexpr float k_building_pick_height = 4.5F;

std::atomic<const Game::Map::TerrainService*> g_bound_terrain{nullptr};
std::atomic<const Engine::Core::World*> g_bound_world{nullptr};

auto ray_hits_terrain(const Game::Map::TerrainService& terrain_service,
                      const QVector3D& ray_origin,
                      const QVector3D& ray_dir,
                      float max_t) -> float {
  auto height_delta = [&](float t) {
    QVector3D const point = ray_origin + ray_dir * t;
    return point.y() -
           terrain_service.sample_surface_height(point.x(), point.z()).world_y;
  };
  constexpr float k_step = 1.0F;
  float prev_t = 0.0F;
  float prev_delta = height_delta(prev_t);
  for (float t = k_step; t <= max_t; t += k_step) {
    float const delta = height_delta(t);
    if ((prev_delta >= 0.0F && delta <= 0.0F) ||
        (prev_delta <= 0.0F && delta >= 0.0F)) {
      float lo = prev_t;
      float hi = t;
      for (int i = 0; i < 16; ++i) {
        float const mid = (lo + hi) * 0.5F;
        float const mid_delta = height_delta(mid);
        if ((prev_delta >= 0.0F && mid_delta >= 0.0F) ||
            (prev_delta <= 0.0F && mid_delta <= 0.0F)) {
          lo = mid;
          prev_delta = mid_delta;
        } else {
          hi = mid;
        }
      }
      return (lo + hi) * 0.5F;
    }
    prev_t = t;
    prev_delta = delta;
  }
  return -1.0F;
}

} // namespace

void PickingService::bind_surface(const Game::Map::TerrainService* terrain,
                                  const Engine::Core::World* world) {
  g_bound_terrain.store(terrain);
  g_bound_world.store(world);
}

void PickingService::unbind_surface(const Engine::Core::World* world) {
  const Engine::Core::World* expected = world;
  if (g_bound_world.compare_exchange_strong(expected, nullptr)) {
    g_bound_terrain.store(nullptr);
  }
}

auto PickingService::surface_height_at(float world_x, float world_z) -> float {
  auto const* terrain = g_bound_terrain.load();
  auto const* world = g_bound_world.load();
  float height = 0.0F;
  if (terrain != nullptr && terrain->is_initialized()) {
    height = terrain->sample_surface_height(world_x, world_z).world_y;
  }
  if (world != nullptr) {

    constexpr float k_above = 50.0F;
    QVector3D const origin(world_x, height + k_above, world_z);
    float const t = ray_hits_wall_walk(
        *world, terrain, origin, QVector3D(0.0F, -1.0F, 0.0F), k_above);
    if (t >= 0.0F) {
      height = std::max(height, origin.y() - t);
    }
  }
  return height;
}

auto PickingService::ray_hits_wall_walk(const Engine::Core::World& world,
                                        const Game::Map::TerrainService* terrain,
                                        const QVector3D& origin,
                                        const QVector3D& direction,
                                        float max_t) -> float {
  namespace WW = Game::Systems::WallWalk;
  if (direction.y() > -1.0e-4F) {
    return -1.0F;
  }
  float best = -1.0F;

  auto try_slab = [&](float node_x,
                      float node_z,
                      float base_y,
                      bool runs_x,
                      float inward_sign,
                      float across_lo,
                      float across_hi,
                      float height) {
    float const top = base_y + height;
    float const t = (top - origin.y()) / direction.y();
    if (t <= 0.0F || t > max_t || (best >= 0.0F && t >= best)) {
      return;
    }
    QVector3D const hit = origin + direction * t;
    float const along = runs_x ? hit.x() - node_x : hit.z() - node_z;
    float const across = (runs_x ? hit.z() - node_z : hit.x() - node_x) * inward_sign;
    if (std::abs(along) <= 1.0F && across >= across_lo && across <= across_hi) {
      best = t;
    }
  };
  for (auto [id, unit, transform, wall] :
       const_cast<Engine::Core::World&>(world)
           .view<const Engine::Core::UnitComponent,
                 const Engine::Core::TransformComponent,
                 const Engine::Core::WallSegmentComponent>()) {
    (void)id;
    if (unit.health <= 0 || unit.spawn_type != Game::Units::SpawnType::WallSegment) {
      continue;
    }
    bool const runs_x = wall.inner_z != 0 && wall.inner_x == 0;
    bool const runs_z = wall.inner_x != 0 && wall.inner_z == 0;
    if (!runs_x && !runs_z) {
      continue;
    }
    float const node_x = transform.position.x;
    float const node_z = transform.position.z;
    float const base_y =
        terrain != nullptr && terrain->is_initialized()
            ? terrain->sample_surface_height(node_x, node_z, transform.position.y)
                  .world_y
            : transform.position.y;
    float const inward = static_cast<float>(runs_x ? wall.inner_z : wall.inner_x);
    try_slab(node_x, node_z, base_y, runs_x, inward, -0.22F, 0.22F, WW::k_crest_height);
    try_slab(node_x,
             node_z,
             base_y,
             runs_x,
             inward,
             WW::k_deck_inner_edge - 0.05F,
             WW::k_deck_outer_edge + 0.05F,
             WW::k_deck_height);
  }
  return best;
}

auto PickingService::world_to_screen(const Render::GL::Camera& cam,
                                     int view_w,
                                     int view_h,
                                     const QVector3D& world,
                                     QPointF& out) -> bool {
  return cam.world_to_screen(world, qreal(view_w), qreal(view_h), out);
}

auto PickingService::screen_to_plane(const Render::GL::Camera& cam,
                                     int view_w,
                                     int view_h,
                                     const QPointF& screen_pt,
                                     QVector3D& out_world) -> bool {
  if (view_w <= 0 || view_h <= 0) {
    return false;
  }
  return cam.screen_to_ground(
      screen_pt.x(), screen_pt.y(), qreal(view_w), qreal(view_h), out_world);
}

auto PickingService::screen_to_ground(const Render::GL::Camera& cam,
                                      int view_w,
                                      int view_h,
                                      const QPointF& screen_pt,
                                      QVector3D& out_world) -> bool {
  auto const* terrain = g_bound_terrain.load();
  auto const* world = g_bound_world.load();
  if (terrain == nullptr && world == nullptr) {
    return screen_to_plane(cam, view_w, view_h, screen_pt, out_world);
  }
  if (terrain != nullptr) {
    if (!screen_to_surface(*terrain, cam, view_w, view_h, screen_pt, out_world)) {
      return false;
    }
  } else if (!screen_to_plane(cam, view_w, view_h, screen_pt, out_world)) {
    return false;
  }
  if (world == nullptr) {
    return true;
  }
  QVector3D ray_origin;
  QVector3D ray_dir;
  if (!cam.screen_to_world_ray(screen_pt.x(),
                               screen_pt.y(),
                               qreal(view_w),
                               qreal(view_h),
                               ray_origin,
                               ray_dir)) {
    return true;
  }
  float const ground_t = (out_world - ray_origin).length();
  float const wall_t =
      ray_hits_wall_walk(*world, terrain, ray_origin, ray_dir, ground_t);
  if (wall_t >= 0.0F && wall_t < ground_t) {
    out_world = ray_origin + ray_dir * wall_t;
  }
  return true;
}

auto PickingService::screen_to_surface(const Game::Map::TerrainService& terrain_service,
                                       const Render::GL::Camera& cam,
                                       int view_w,
                                       int view_h,
                                       const QPointF& screen_pt,
                                       QVector3D& out_world) -> bool {
  if (view_w <= 0 || view_h <= 0 || !terrain_service.is_initialized() ||
      terrain_service.get_height_map() == nullptr) {
    return screen_to_plane(cam, view_w, view_h, screen_pt, out_world);
  }

  QVector3D ray_origin;
  QVector3D ray_dir;
  if (!cam.screen_to_world_ray(screen_pt.x(),
                               screen_pt.y(),
                               qreal(view_w),
                               qreal(view_h),
                               ray_origin,
                               ray_dir)) {
    return false;
  }

  float const max_t = std::max(cam.get_far(), 1.0F);
  float const t = ray_hits_terrain(terrain_service, ray_origin, ray_dir, max_t);
  if (t >= 0.0F) {
    out_world = ray_origin + ray_dir * t;
    out_world.setY(
        terrain_service.sample_surface_height(out_world.x(), out_world.z()).world_y);
    return true;
  }
  return screen_to_plane(cam, view_w, view_h, screen_pt, out_world);
}

auto PickingService::project_bounds(const Render::GL::Camera& cam,
                                    const QVector3D& center,
                                    float hx,
                                    float hz,
                                    int view_w,
                                    int view_h,
                                    QRectF& out) -> bool {
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

auto PickingService::update_hover(float sx,
                                  float sy,
                                  Engine::Core::World& world,
                                  const Render::GL::Camera& camera,
                                  int view_w,
                                  int view_h) -> Engine::Core::EntityID {
  if (sx < 0 || sy < 0 || sx >= view_w || sy >= view_h) {
    m_prev_hover_id = 0;
    return 0;
  }
  auto prev_hover = m_prev_hover_id;

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
  m_prev_hover_id = current_hover;
  return current_hover;
}

auto PickingService::pick_nearest(float sx,
                                  float sy,
                                  Engine::Core::World& world,
                                  const Render::GL::Camera& camera,
                                  int view_w,
                                  int view_h,
                                  int owner_filter) -> NearestPicks {

  const float base_unit_pick_radius = 30.0F;
  const float base_building_pick_radius = 30.0F;
  float best_unit_dist2 = std::numeric_limits<float>::max();
  float best_building_dist2 = std::numeric_limits<float>::max();
  Engine::Core::EntityID best_unit_id = 0;
  Engine::Core::EntityID best_building_id = 0;
  const Render::GL::ScreenProjector projector =
      camera.screen_projector(qreal(view_w), qreal(view_h));
  auto ents = world.collect_entities_with<Engine::Core::TransformComponent>();
  for (auto* e : ents) {
    if (!e->has_component<Engine::Core::UnitComponent>()) {
      continue;
    }
    auto* t = e->get_component<Engine::Core::TransformComponent>();
    auto* u = e->get_component<Engine::Core::UnitComponent>();

    if (owner_filter != 0 && u->owner_id != owner_filter) {
      continue;
    }

    if (u->health <= 0 && world.has<Engine::Core::BuildingComponent>(e->get_id())) {
      continue;
    }

    QPointF sp;
    if (!projector.project(QVector3D(t->position.x, t->position.y, t->position.z),
                           sp)) {
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

      auto const footprint =
          BuildingCollisionRegistry::get_building_size(u->spawn_type);
      float const footprint_half = 0.5F * std::max(footprint.width, footprint.depth) *
                                   std::max(std::max(t->scale.x, t->scale.z), 1.0F);
      float const hx = std::max({0.6F, t->scale.x * margin_x_z, footprint_half});
      float const hz = std::max({0.6F, t->scale.z * margin_x_z, footprint_half});
      float const hy = std::max({0.5F, t->scale.y * margin_y, k_building_pick_height});
      QPointF pts[8];
      int ok_count = 0;
      auto project = [&](const QVector3D& w, QPointF& out) {
        return projector.project(w, out);
      };
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x - hx, t->position.y + 0.0F, t->position.z - hz),
          pts[0]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x + hx, t->position.y + 0.0F, t->position.z - hz),
          pts[1]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x + hx, t->position.y + 0.0F, t->position.z + hz),
          pts[2]));
      ok_count += static_cast<int>(project(
          QVector3D(t->position.x - hx, t->position.y + 0.0F, t->position.z + hz),
          pts[3]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x - hx, t->position.y + hy, t->position.z - hz),
                  pts[4]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x + hx, t->position.y + hy, t->position.z - hz),
                  pts[5]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x + hx, t->position.y + hy, t->position.z + hz),
                  pts[6]));
      ok_count += static_cast<int>(
          project(QVector3D(t->position.x - hx, t->position.y + hy, t->position.z + hz),
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
        float const scale_x_z = std::max(std::max(t->scale.x, t->scale.z), 1.0F);
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
  return {.unit_id = best_unit_id,
          .unit_dist2 = best_unit_dist2,
          .building_id = best_building_id,
          .building_dist2 = best_building_dist2};
}

auto PickingService::pick_single(float sx,
                                 float sy,
                                 Engine::Core::World& world,
                                 const Render::GL::Camera& camera,
                                 int view_w,
                                 int view_h,
                                 int owner_filter,
                                 bool prefer_buildings_first)
    -> Engine::Core::EntityID {
  auto const picks = pick_nearest(sx, sy, world, camera, view_w, view_h, owner_filter);
  if (prefer_buildings_first) {
    if ((picks.building_id != 0U) &&
        ((picks.unit_id == 0U) || picks.building_dist2 <= picks.unit_dist2)) {
      return picks.building_id;
    }
    if (picks.unit_id != 0U) {
      return picks.unit_id;
    }
  } else {
    if (picks.unit_id != 0U) {
      return picks.unit_id;
    }
    if (picks.building_id != 0U) {
      return picks.building_id;
    }
  }
  return 0;
}

auto PickingService::pick_building(float sx,
                                   float sy,
                                   Engine::Core::World& world,
                                   const Render::GL::Camera& camera,
                                   int view_w,
                                   int view_h) -> Engine::Core::EntityID {
  return pick_nearest(sx, sy, world, camera, view_w, view_h, 0).building_id;
}

auto PickingService::pick_unit_first(float sx,
                                     float sy,
                                     Engine::Core::World& world,
                                     const Render::GL::Camera& camera,
                                     int view_w,
                                     int view_h,
                                     int owner_filter) -> Engine::Core::EntityID {

  auto id = pick_single(sx, sy, world, camera, view_w, view_h, owner_filter, false);
  if (id != 0) {
    return id;
  }

  return pick_single(sx, sy, world, camera, view_w, view_h, owner_filter, true);
}

auto PickingService::pick_in_rect(float x1,
                                  float y1,
                                  float x2,
                                  float y2,
                                  Engine::Core::World& world,
                                  const Render::GL::Camera& camera,
                                  int view_w,
                                  int view_h,
                                  int owner_filter)
    -> std::vector<Engine::Core::EntityID> {
  float const min_x = std::min(x1, x2);
  float const max_x = std::max(x1, x2);
  float const min_y = std::min(y1, y2);
  float const max_y = std::max(y1, y2);
  std::vector<Engine::Core::EntityID> picked;
  const Render::GL::ScreenProjector projector =
      camera.screen_projector(qreal(view_w), qreal(view_h));
  auto ents = world.collect_entities_with<Engine::Core::TransformComponent>();
  for (auto* e : ents) {
    if (!e->has_component<Engine::Core::UnitComponent>()) {
      continue;
    }
    if (e->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    auto* u = e->get_component<Engine::Core::UnitComponent>();
    if ((u == nullptr) || u->owner_id != owner_filter) {
      continue;
    }
    auto* t = e->get_component<Engine::Core::TransformComponent>();
    QPointF sp;
    if (!projector.project(QVector3D(t->position.x, t->position.y, t->position.z),
                           sp)) {
      continue;
    }
    if (sp.x() >= min_x && sp.x() <= max_x && sp.y() >= min_y && sp.y() <= max_y) {
      picked.push_back(e->get_id());
    }
  }
  return picked;
}

} // namespace Game::Systems
