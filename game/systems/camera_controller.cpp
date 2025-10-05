#include "camera_controller.h"
#include "../../render/gl/camera.h"

namespace Game {
namespace Systems {

void CameraController::move(Render::GL::Camera &camera, float dx,
                            float dz) const {
  camera.pan(dx, dz);
  if (camera.isFollowEnabled())
    camera.captureFollowOffset();
}

void CameraController::elevate(Render::GL::Camera &camera, float dy) const {
  camera.elevate(dy);
  if (camera.isFollowEnabled())
    camera.captureFollowOffset();
}

void CameraController::yaw(Render::GL::Camera &camera, float degrees) const {
  camera.yaw(degrees);
  if (camera.isFollowEnabled())
    camera.captureFollowOffset();
}

void CameraController::orbit(Render::GL::Camera &camera, float yawDeg,
                             float pitchDeg) const {
  camera.orbit(yawDeg, pitchDeg);
  if (camera.isFollowEnabled())
    camera.captureFollowOffset();
}

void CameraController::setFollowEnabled(Render::GL::Camera &camera,
                                        bool enable) const {
  camera.setFollowEnabled(enable);
}

void CameraController::setFollowLerp(Render::GL::Camera &camera,
                                     float alpha) const {
  camera.setFollowLerp(alpha);
}

} // namespace Systems
} // namespace Game
