#include "camera_controller.h"
#include "../../render/gl/camera.h"

namespace Game::Systems {

void CameraController::move(Render::GL::Camera &camera, float dx, float dz) {
  camera.pan(dx, dz);
  if (camera.isFollowEnabled()) {
    camera.captureFollowOffset();
  }
}

void CameraController::elevate(Render::GL::Camera &camera, float dy) {
  camera.elevate(dy);
  if (camera.isFollowEnabled()) {
    camera.captureFollowOffset();
  }
}

void CameraController::moveUp(Render::GL::Camera &camera, float dy) {
  camera.moveUp(dy);
  if (camera.isFollowEnabled()) {
    camera.captureFollowOffset();
  }
}

void CameraController::yaw(Render::GL::Camera &camera, float degrees) {
  camera.yaw(degrees);
  if (camera.isFollowEnabled()) {
    camera.captureFollowOffset();
  }
}

void CameraController::orbit(Render::GL::Camera &camera, float yaw_deg,
                             float pitch_deg) {
  camera.orbit(yaw_deg, pitch_deg);
  if (camera.isFollowEnabled()) {
    camera.captureFollowOffset();
  }
}

void CameraController::zoomDistance(Render::GL::Camera &camera, float delta) {
  camera.zoomDistance(delta);
  if (camera.isFollowEnabled()) {
    camera.captureFollowOffset();
  }
}

void CameraController::setFollowEnabled(Render::GL::Camera &camera,
                                        bool enable) {
  camera.setFollowEnabled(enable);
}

void CameraController::setFollowLerp(Render::GL::Camera &camera, float alpha) {
  camera.setFollowLerp(alpha);
}

} // namespace Game::Systems
