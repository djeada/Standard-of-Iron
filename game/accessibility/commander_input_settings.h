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

// How far the chase camera stands off, as a multiple of the authored boom:
// below one for a tight shoulder view, above it to see the fight around the
// commander. The bow's aiming view keeps its own distance.
void set_camera_distance_scale(float scale);
[[nodiscard]] auto camera_distance_scale() -> float;

void set_guard_is_toggle(bool enabled);
[[nodiscard]] auto guard_is_toggle() -> bool;

void reset_to_defaults();

} // namespace Game::Accessibility::CommanderInput
