#include "render/creature/humanoid/runtime/runtime_context.h"

#include "render/creature/runtime_frame.h"

namespace Render::Humanoid {

namespace {

auto fallback_context() noexcept -> HumanoidRuntimeContext& {
  thread_local HumanoidRuntimeContext context;
  return context;
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
  return Render::Creature::creature_frame_serial();
}

void advance_humanoid_frame_serial() noexcept {
  Render::Creature::advance_creature_frame_serial();
}

auto current_humanoid_runtime_context() noexcept -> HumanoidRuntimeContext& {
  auto* context = installed_context();
  return context != nullptr ? *context : fallback_context();
}

} // namespace Render::Humanoid
