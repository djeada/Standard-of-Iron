#include "camera_service.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../game_config.h"
#include "camera_controller.h"
#include "camera_follow_system.h"
#include "selection_system.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Game {
namespace Systems {

CameraService::CameraService()
    : m_controller(std::make_unique<CameraController>()),
      m_followSystem(std::make_unique<CameraFollowSystem>()) {}

CameraService::~CameraService() = default;

void CameraService::move(Render::GL::Camera &camera, float dx, float dz) {
  float dist = camera.getDistance();
  float scale = std::max(0.12f, dist * 0.05f);
  m_controller->move(camera, dx * scale, dz * scale);
}

void CameraService::elevate(Render::GL::Camera &camera, float dy) {
  float distance = camera.getDistance();
  float scale = std::clamp(distance * 0.05f, 0.1f, 5.0f);
  m_controller->moveUp(camera, dy * scale);
}

void CameraService::zoom(Render::GL::Camera &camera, float delta) {
  m_controller->zoomDistance(camera, delta);
}

float CameraService::getDistance(const Render::GL::Camera &camera) const {
  return camera.getDistance();
}

void CameraService::yaw(Render::GL::Camera &camera, float degrees) {
  m_controller->yaw(camera, degrees);
}

void CameraService::orbit(Render::GL::Camera &camera, float yawDeg,
                          float pitchDeg) {
  if (!std::isfinite(yawDeg) || !std::isfinite(pitchDeg)) {
    return;
  }
  m_controller->orbit(camera, yawDeg, pitchDeg);
}

void CameraService::orbitDirection(Render::GL::Camera &camera, int direction,
                                   bool shift) {
  const auto &camConfig = Game::GameConfig::instance().camera();
  float step = shift ? camConfig.orbitStepShift : camConfig.orbitStepNormal;
  float pitch = step * float(direction);
  orbit(camera, 0.0f, pitch);
}

void CameraService::followSelection(Render::GL::Camera &camera,
                                    Engine::Core::World &world, bool enable) {
  m_controller->setFollowEnabled(camera, enable);

  if (enable) {
    if (auto *selectionSystem = world.getSystem<SelectionSystem>()) {
      m_followSystem->snapToSelection(world, *selectionSystem, camera);
    }
  } else {
    auto pos = camera.getPosition();
    auto tgt = camera.getTarget();
    camera.lookAt(pos, tgt, QVector3D(0, 1, 0));
  }
}

void CameraService::setFollowLerp(Render::GL::Camera &camera, float alpha) {
  float a = std::clamp(alpha, 0.0f, 1.0f);
  m_controller->setFollowLerp(camera, a);
}

void CameraService::resetCamera(Render::GL::Camera &camera,
                                Engine::Core::World &world, int localOwnerId,
                                unsigned int playerUnitId) {
  Engine::Core::Entity *focusEntity = nullptr;
  for (auto *e : world.getEntitiesWith<Engine::Core::UnitComponent>()) {
    if (!e)
      continue;
    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u)
      continue;
    if (u->unitType == "barracks" && u->ownerId == localOwnerId &&
        u->health > 0) {
      focusEntity = e;
      break;
    }
  }
  if (!focusEntity && playerUnitId != 0)
    focusEntity = world.getEntity(playerUnitId);

  if (focusEntity) {
    snapToEntity(camera, *focusEntity);
  }
}

void CameraService::snapToEntity(Render::GL::Camera &camera,
                                 Engine::Core::Entity &entity) {
  if (auto *t = entity.getComponent<Engine::Core::TransformComponent>()) {
    QVector3D center(t->position.x, t->position.y, t->position.z);
    const auto &camConfig = Game::GameConfig::instance().camera();
    camera.setRTSView(center, camConfig.defaultDistance, camConfig.defaultPitch,
                      camConfig.defaultYaw);
  }
}

void CameraService::updateFollow(Render::GL::Camera &camera,
                                 Engine::Core::World &world,
                                 bool followEnabled) {
  if (followEnabled) {
    if (auto *selectionSystem = world.getSystem<SelectionSystem>()) {
      m_followSystem->update(world, *selectionSystem, camera);
    }
  }
}

} // namespace Systems
} // namespace Game
