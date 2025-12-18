#include "camera_controller.h"
#include "../../render/gl/camera.h"

namespace Game::Systems {

void CameraController::move(Render::GL::Camera &camera, float dx, float dz) {
  camera.pan(dx, dz);
  if (camera.is_follow_enabled()) {
    camera.capture_follow_offset();
  }
}

void CameraController::elevate(Render::GL::Camera &camera, float dy) {
  camera.elevate(dy);
  if (camera.is_follow_enabled()) {
    camera.capture_follow_offset();
  }
}

void CameraController::move_up(Render::GL::Camera &camera, float dy) {
  camera.move_up(dy);
  if (camera.is_follow_enabled()) {
    camera.capture_follow_offset();
  }
}

void CameraController::yaw(Render::GL::Camera &camera, float degrees) {
  camera.yaw(degrees);
  if (camera.is_follow_enabled()) {
    camera.capture_follow_offset();
  }
}

void CameraController::orbit(Render::GL::Camera &camera, float yaw_deg,
                             float pitch_deg) {
  camera.orbit(yaw_deg, pitch_deg);
  if (camera.is_follow_enabled()) {
    camera.capture_follow_offset();
  }
}

void CameraController::zoom_distance(Render::GL::Camera &camera, float delta) {
  camera.zoom_distance(delta);
  if (camera.is_follow_enabled()) {
    camera.capture_follow_offset();
  }
}

void CameraController::set_follow_enabled(Render::GL::Camera &camera,
                                          bool enable) {
  camera.set_follow_enabled(enable);
}

void CameraController::set_follow_lerp(Render::GL::Camera &camera,
                                       float alpha) {
  camera.set_follow_lerp(alpha);
}

} 
