#include "render/humanoid/runtime/runtime_context.h"

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

auto current_humanoid_runtime_context() noexcept -> HumanoidRuntimeContext& {
  auto* context = installed_context();
  return context != nullptr ? *context : fallback_context();
}

} // namespace Render::Humanoid
