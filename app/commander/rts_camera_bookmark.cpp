#include "app/commander/rts_camera_bookmark.h"

#include "scene/camera.h"

namespace App::Core {

auto RtsCameraBookmark::capture(const Render::GL::Camera& camera) -> RtsCameraBookmark {
  RtsCameraBookmark bookmark;
  bookmark.position = camera.get_position();
  bookmark.target = camera.get_target();
  bookmark.up = camera.get_up_vector();
  bookmark.fov = camera.get_fov();
  bookmark.near_plane = camera.get_near();
  bookmark.far_plane = camera.get_far();
  bookmark.valid = true;
  return bookmark;
}

void RtsCameraBookmark::restore(Render::GL::Camera& camera) const {
  if (!valid) {
    return;
  }
  camera.set_perspective(fov, camera.get_aspect(), near_plane, far_plane);
  camera.look_at(position, target, up);
}

} // namespace App::Core
