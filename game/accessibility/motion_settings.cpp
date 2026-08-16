#include "motion_settings.h"

#include <algorithm>
#include <atomic>

namespace Game::Accessibility::MotionSettings {

namespace {
std::atomic<float> g_camera_motion_scale{1.0F};
std::atomic<bool> g_reduced_motion{false};
} // namespace

void set_camera_motion_scale(float scale) {
  g_camera_motion_scale.store(std::clamp(scale, 0.0F, 1.0F), std::memory_order_relaxed);
}

auto camera_motion_scale() -> float {
  return g_camera_motion_scale.load(std::memory_order_relaxed);
}

void set_reduced_motion(bool enabled) {
  g_reduced_motion.store(enabled, std::memory_order_relaxed);
}

auto reduced_motion() -> bool {
  return g_reduced_motion.load(std::memory_order_relaxed);
}

} // namespace Game::Accessibility::MotionSettings
