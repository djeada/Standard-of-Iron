#include "render/entity/health_bar_visibility.h"

namespace Render::GL {

auto health_bar_visible(const HealthBarVisibilityInputs& inputs) -> bool {
  if (!inputs.alive) {
    return false;
  }
  if (inputs.camera_distance > k_health_bar_max_camera_distance) {
    return false;
  }
  if (inputs.selected || inputs.hovered || inputs.commander_target) {
    return true;
  }
  return inputs.recently_damaged && !inputs.full_health;
}

} // namespace Render::GL
