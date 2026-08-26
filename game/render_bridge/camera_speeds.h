#pragma once

#include <algorithm>
#include <atomic>

namespace Game::Systems::CameraSpeeds {

inline constexpr float k_min_scale = 0.25F;
inline constexpr float k_max_scale = 3.0F;
inline constexpr float k_default_scale = 1.0F;

namespace Detail {

inline auto pan_slot() -> std::atomic<float>& {
  static std::atomic<float> slot{k_default_scale};
  return slot;
}

inline auto zoom_slot() -> std::atomic<float>& {
  static std::atomic<float> slot{k_default_scale};
  return slot;
}

inline auto rotation_slot() -> std::atomic<float>& {
  static std::atomic<float> slot{k_default_scale};
  return slot;
}

} // namespace Detail

inline void set_pan_scale(float scale) {
  Detail::pan_slot().store(std::clamp(scale, k_min_scale, k_max_scale),
                           std::memory_order_relaxed);
}

[[nodiscard]] inline auto pan_scale() -> float {
  return Detail::pan_slot().load(std::memory_order_relaxed);
}

inline void set_zoom_scale(float scale) {
  Detail::zoom_slot().store(std::clamp(scale, k_min_scale, k_max_scale),
                            std::memory_order_relaxed);
}

[[nodiscard]] inline auto zoom_scale() -> float {
  return Detail::zoom_slot().load(std::memory_order_relaxed);
}

inline void set_rotation_scale(float scale) {
  Detail::rotation_slot().store(std::clamp(scale, k_min_scale, k_max_scale),
                                std::memory_order_relaxed);
}

[[nodiscard]] inline auto rotation_scale() -> float {
  return Detail::rotation_slot().load(std::memory_order_relaxed);
}

} // namespace Game::Systems::CameraSpeeds
