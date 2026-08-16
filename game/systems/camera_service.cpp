#include "camera_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <memory>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/visibility_service.h"
#include "camera_controller.h"
#include "camera_follow_system.h"
#include "scene/camera.h"
#include "selection_system.h"
#include "units/spawn_type.h"

namespace Game::Systems {

CameraService::CameraService()
    : m_controller(std::make_unique<CameraController>())
    , m_follow_system(std::make_unique<CameraFollowSystem>()) {
}

CameraService::~CameraService() = default;

void CameraService::sync_map_bounds(Render::GL::Camera& camera) {
  const auto& visibility = Game::Map::VisibilityService::instance();
  if (!visibility.is_initialized()) {
    camera.clear_map_bounds();
    return;
  }
  camera.set_map_bounds({.tile_size = visibility.get_tile_size(),
                         .width = visibility.get_width(),
                         .height = visibility.get_height()});
}

void CameraService::move(Render::GL::Camera& camera, float dx, float dz) {
  sync_map_bounds(camera);
  float const dist = camera.get_distance();
  float const scale = std::max(0.12F, dist * 0.05F);
  m_controller->move(camera, dx * scale, dz * scale);
}

void CameraService::elevate(Render::GL::Camera& camera, float dy) {
  sync_map_bounds(camera);
  float const distance = camera.get_distance();
  float const scale = std::clamp(distance * 0.05F, 0.1F, 5.0F);
  m_controller->move_up(camera, dy * scale);
}

void CameraService::zoom(Render::GL::Camera& camera, float delta) {
  sync_map_bounds(camera);
  m_controller->zoom_distance(camera, delta);
}

auto CameraService::get_distance(const Render::GL::Camera& camera) -> float {
  return camera.get_distance();
}

void CameraService::yaw(Render::GL::Camera& camera, float degrees) {
  sync_map_bounds(camera);
  m_controller->yaw(camera, degrees);
}

void CameraService::orbit(Render::GL::Camera& camera, float yaw_deg, float pitch_deg) {
  sync_map_bounds(camera);
  if (!std::isfinite(yaw_deg) || !std::isfinite(pitch_deg)) {
    return;
  }
  m_controller->orbit(camera, yaw_deg, pitch_deg);
}

void CameraService::tilt(Render::GL::Camera& camera, int direction, bool shift) {
  sync_map_bounds(camera);
  const auto& cam_config = Game::GameConfig::instance().camera();
  float const step = shift ? cam_config.tilt_step_shift : cam_config.tilt_step_normal;
  orbit(camera, 0.0F, -step * float(direction));
}

void CameraService::follow_selection(Render::GL::Camera& camera,
                                     Engine::Core::World& world,
                                     bool enable) {
  sync_map_bounds(camera);
  m_controller->set_follow_enabled(camera, enable);

  if (enable) {
    if (auto* selection_system = world.get_system<SelectionSystem>()) {
      m_follow_system->snap_to_selection(world, *selection_system, camera);
    }
  } else {
    auto pos = camera.get_position();
    auto tgt = camera.get_target();
    camera.look_at(pos, tgt, QVector3D(0, 1, 0));
  }
}

void CameraService::set_follow_lerp(Render::GL::Camera& camera, float alpha) {
  float const a = std::clamp(alpha, 0.0F, 1.0F);
  m_controller->set_follow_lerp(camera, a);
}

void CameraService::reset_camera(Render::GL::Camera& camera,
                                 Engine::Core::World& world,
                                 int local_owner_id,
                                 Engine::Core::EntityID player_unit_id) {
  sync_map_bounds(camera);
  Engine::Core::Entity* focus_entity = nullptr;
  for (auto* e : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (e == nullptr) {
      continue;
    }
    auto* u = e->get_component<Engine::Core::UnitComponent>();
    if (u == nullptr) {
      continue;
    }
    if (u->spawn_type == Game::Units::SpawnType::Barracks &&
        u->owner_id == local_owner_id && u->health > 0) {
      focus_entity = e;
      break;
    }
  }
  if ((focus_entity == nullptr) && player_unit_id != 0U) {
    focus_entity = world.get_entity(player_unit_id);
  }

  if (focus_entity != nullptr) {
    snap_to_entity(camera, *focus_entity);
  }
}

void CameraService::snap_to_entity(Render::GL::Camera& camera,
                                   Engine::Core::Entity& entity) {
  sync_map_bounds(camera);
  if (auto* t = entity.get_component<Engine::Core::TransformComponent>()) {
    QVector3D const center(t->position.x, t->position.y, t->position.z);
    const auto framing = Game::GameConfig::instance().camera_reset_framing();
    camera.set_rts_view(center, framing.distance, framing.pitch, framing.yaw);
  }
}

void CameraService::update_follow(Render::GL::Camera& camera,
                                  Engine::Core::World& world,
                                  bool follow_enabled) {
  sync_map_bounds(camera);
  if (follow_enabled) {
    if (auto* selection_system = world.get_system<SelectionSystem>()) {
      m_follow_system->update(world, *selection_system, camera);
    }
  }
}

} // namespace Game::Systems
