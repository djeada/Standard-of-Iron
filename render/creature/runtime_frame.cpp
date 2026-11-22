#include "render/creature/runtime_frame.h"

#include <atomic>

namespace Render::Creature {

namespace {

auto frame_serial() noexcept -> std::atomic<std::uint32_t>& {
  static std::atomic<std::uint32_t> serial{1U};
  return serial;
}

auto runtime_generation() noexcept -> std::atomic<std::uint32_t>& {
  static std::atomic<std::uint32_t> generation{0U};
  return generation;
}

} // namespace

auto creature_frame_serial() noexcept -> std::uint32_t {
  return frame_serial().load(std::memory_order_acquire);
}

void advance_creature_frame_serial() noexcept {
  frame_serial().fetch_add(1U, std::memory_order_acq_rel);
}

auto creature_runtime_generation() noexcept -> std::uint32_t {
  return runtime_generation().load(std::memory_order_relaxed);
}

void advance_creature_runtime_generation() noexcept {
  runtime_generation().fetch_add(1U, std::memory_order_relaxed);
}

} // namespace Render::Creature
