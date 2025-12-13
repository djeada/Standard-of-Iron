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

void CameraController::set_follow_enabled(Render::GL::Camera &camera,
                                          bool enable) {
  camera.set_follow_enabled(enable);
}

void CameraController::set_follow_lerp(Render::GL::Camera &camera,
                                       float alpha) {
  camera.setFollowLerp(alpha);
}

} // namespace Game::Systems
