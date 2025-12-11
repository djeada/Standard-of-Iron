#include "camera_service.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../game_config.h"
#include "camera_controller.h"
#include "camera_follow_system.h"
#include "selection_system.h"
#include "units/spawn_type.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <memory>

namespace Game::Systems {

CameraService::CameraService()
    : m_controller(std::make_unique<CameraController>()),
      m_followSystem(std::make_unique<CameraFollowSystem>()) {}

CameraService::~CameraService() = default;

void CameraService::move(Render::GL::Camera &camera, float dx, float dz) {
  float const dist = camera.get_distance();
  float const scale = std::max(0.12F, dist * 0.05F);
  m_controller->move(camera, dx * scale, dz * scale);
}

void CameraService::elevate(Render::GL::Camera &camera, float dy) {
  float const distance = camera.get_distance();
  float const scale = std::clamp(distance * 0.05F, 0.1F, 5.0F);
  m_controller->moveUp(camera, dy * scale);
}

void CameraService::zoom(Render::GL::Camera &camera, float delta) {
  m_controller->zoomDistance(camera, delta);
}

auto CameraService::get_distance(const Render::GL::Camera &camera) -> float {
  return camera.get_distance();
}

void CameraService::yaw(Render::GL::Camera &camera, float degrees) {
  m_controller->yaw(camera, degrees);
}

void CameraService::orbit(Render::GL::Camera &camera, float yaw_deg,
                          float pitch_deg) {
  if (!std::isfinite(yaw_deg) || !std::isfinite(pitch_deg)) {
    return;
  }
  m_controller->orbit(camera, yaw_deg, pitch_deg);
}

void CameraService::orbit_direction(Render::GL::Camera &camera, int direction,
                                    bool shift) {
  const auto &cam_config = Game::GameConfig::instance().camera();
  float const step =
      shift ? cam_config.orbitStepShift : cam_config.orbitStepNormal;
  float const pitch = step * float(direction);
  orbit(camera, 0.0F, pitch);
}

void CameraService::follow_selection(Render::GL::Camera &camera,
                                     Engine::Core::World &world, bool enable) {
  m_controller->setFollowEnabled(camera, enable);

  if (enable) {
    if (auto *selection_system = world.get_system<SelectionSystem>()) {
      m_followSystem->snapToSelection(world, *selection_system, camera);
    }
  } else {
    auto pos = camera.get_position();
    auto tgt = camera.get_target();
    camera.lookAt(pos, tgt, QVector3D(0, 1, 0));
  }
}

void CameraService::setFollowLerp(Render::GL::Camera &camera, float alpha) {
  float const a = std::clamp(alpha, 0.0F, 1.0F);
  m_controller->setFollowLerp(camera, a);
}

void CameraService::resetCamera(Render::GL::Camera &camera,
                                Engine::Core::World &world, int local_owner_id,
                                unsigned int player_unit_id) {
  Engine::Core::Entity *focus_entity = nullptr;
  for (auto *e : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (e == nullptr) {
      continue;
    }
    auto *u = e->get_component<Engine::Core::UnitComponent>();
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
    snapToEntity(camera, *focus_entity);
  }
}

void CameraService::snapToEntity(Render::GL::Camera &camera,
                                 Engine::Core::Entity &entity) {
  if (auto *t = entity.get_component<Engine::Core::TransformComponent>()) {
    QVector3D const center(t->position.x, t->position.y, t->position.z);
    const auto &cam_config = Game::GameConfig::instance().camera();
    camera.setRTSView(center, cam_config.defaultDistance,
                      cam_config.defaultPitch, cam_config.defaultYaw);
  }
}

void CameraService::update_follow(Render::GL::Camera &camera,
                                  Engine::Core::World &world,
                                  bool follow_enabled) {
  if (follow_enabled) {
    if (auto *selection_system = world.get_system<SelectionSystem>()) {
      m_followSystem->update(world, *selection_system, camera);
    }
  }
}

} // namespace Game::Systems
