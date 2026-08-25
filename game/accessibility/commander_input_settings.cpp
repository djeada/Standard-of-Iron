#include "commander_input_settings.h"

#include <algorithm>
#include <atomic>

namespace Game::Accessibility::CommanderInput {

namespace {

constexpr float k_min_sensitivity = 0.20F;
constexpr float k_max_sensitivity = 4.00F;
constexpr float k_min_fov_scale = 0.75F;
constexpr float k_max_fov_scale = 1.35F;

std::atomic<float> g_look_sensitivity_x{1.0F};
std::atomic<float> g_look_sensitivity_y{1.0F};
std::atomic<bool> g_invert_look_y{false};
std::atomic<bool> g_camera_impulse_enabled{true};
std::atomic<bool> g_head_bob_enabled{true};
std::atomic<float> g_field_of_view_scale{1.0F};
std::atomic<bool> g_guard_is_toggle{false};

} // namespace

void set_look_sensitivity_x(float scale) {
  g_look_sensitivity_x.store(std::clamp(scale, k_min_sensitivity, k_max_sensitivity),
                             std::memory_order_relaxed);
}

auto look_sensitivity_x() -> float {
  return g_look_sensitivity_x.load(std::memory_order_relaxed);
}

void set_look_sensitivity_y(float scale) {
  g_look_sensitivity_y.store(std::clamp(scale, k_min_sensitivity, k_max_sensitivity),
                             std::memory_order_relaxed);
}

auto look_sensitivity_y() -> float {
  return g_look_sensitivity_y.load(std::memory_order_relaxed);
}

void set_invert_look_y(bool enabled) {
  g_invert_look_y.store(enabled, std::memory_order_relaxed);
}

auto invert_look_y() -> bool {
  return g_invert_look_y.load(std::memory_order_relaxed);
}

void set_camera_impulse_enabled(bool enabled) {
  g_camera_impulse_enabled.store(enabled, std::memory_order_relaxed);
}

auto camera_impulse_enabled() -> bool {
  return g_camera_impulse_enabled.load(std::memory_order_relaxed);
}

void set_head_bob_enabled(bool enabled) {
  g_head_bob_enabled.store(enabled, std::memory_order_relaxed);
}

auto head_bob_enabled() -> bool {
  return g_head_bob_enabled.load(std::memory_order_relaxed);
}

void set_field_of_view_scale(float scale) {
  g_field_of_view_scale.store(std::clamp(scale, k_min_fov_scale, k_max_fov_scale),
                              std::memory_order_relaxed);
}

auto field_of_view_scale() -> float {
  return g_field_of_view_scale.load(std::memory_order_relaxed);
}

void set_guard_is_toggle(bool enabled) {
  g_guard_is_toggle.store(enabled, std::memory_order_relaxed);
}

auto guard_is_toggle() -> bool {
  return g_guard_is_toggle.load(std::memory_order_relaxed);
}

void reset_to_defaults() {
  set_look_sensitivity_x(1.0F);
  set_look_sensitivity_y(1.0F);
  set_invert_look_y(false);
  set_camera_impulse_enabled(true);
  set_head_bob_enabled(true);
  set_field_of_view_scale(1.0F);
  set_guard_is_toggle(false);
}

} // namespace Game::Accessibility::CommanderInput
