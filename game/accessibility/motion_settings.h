#pragma once

namespace Game::Accessibility::MotionSettings {

void set_camera_motion_scale(float scale);
[[nodiscard]] auto camera_motion_scale() -> float;

} // namespace Game::Accessibility::MotionSettings
