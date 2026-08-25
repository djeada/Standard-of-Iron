#pragma once

namespace Game::Accessibility::CommanderInput {

void set_look_sensitivity_x(float scale);
[[nodiscard]] auto look_sensitivity_x() -> float;

void set_look_sensitivity_y(float scale);
[[nodiscard]] auto look_sensitivity_y() -> float;

void set_invert_look_y(bool enabled);
[[nodiscard]] auto invert_look_y() -> bool;

void set_camera_impulse_enabled(bool enabled);
[[nodiscard]] auto camera_impulse_enabled() -> bool;

void set_head_bob_enabled(bool enabled);
[[nodiscard]] auto head_bob_enabled() -> bool;

void set_field_of_view_scale(float scale);
[[nodiscard]] auto field_of_view_scale() -> float;

void set_guard_is_toggle(bool enabled);
[[nodiscard]] auto guard_is_toggle() -> bool;

void reset_to_defaults();

} // namespace Game::Accessibility::CommanderInput
