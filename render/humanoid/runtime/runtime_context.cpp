#include "render/humanoid/runtime/runtime_context.h"

#include <atomic>

namespace Render::Humanoid {

namespace {

auto fallback_context() noexcept -> HumanoidRuntimeContext& {
  thread_local HumanoidRuntimeContext context;
  return context;
}

auto frame_serial() noexcept -> std::atomic<std::uint32_t>& {
  static std::atomic<std::uint32_t> serial{1U};
  return serial;
}

auto installed_context() noexcept -> HumanoidRuntimeContext*& {
  thread_local HumanoidRuntimeContext* context = nullptr;
  return context;
}

} // namespace

ScopedHumanoidRuntimeContext::ScopedHumanoidRuntimeContext(
    HumanoidRuntimeContext& context) noexcept
    : m_previous(installed_context()) {
  installed_context() = &context;
}

ScopedHumanoidRuntimeContext::~ScopedHumanoidRuntimeContext() {
  installed_context() = m_previous;
}

auto humanoid_frame_serial() noexcept -> std::uint32_t {
  return frame_serial().load(std::memory_order_acquire);
}

void advance_humanoid_frame_serial() noexcept {
  frame_serial().fetch_add(1U, std::memory_order_acq_rel);
}

auto current_humanoid_runtime_context() noexcept -> HumanoidRuntimeContext& {
  auto* context = installed_context();
  return context != nullptr ? *context : fallback_context();
}

} // namespace Render::Humanoid
