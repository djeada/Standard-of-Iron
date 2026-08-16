#pragma once

namespace Game::Accessibility::MotionSettings {

void set_camera_motion_scale(float scale);
[[nodiscard]] auto camera_motion_scale() -> float;

void set_reduced_motion(bool enabled);
[[nodiscard]] auto reduced_motion() -> bool;

} // namespace Game::Accessibility::MotionSettings
