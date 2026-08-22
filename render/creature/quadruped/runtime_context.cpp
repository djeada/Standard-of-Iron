#include "runtime_context.h"

namespace Render::Creature::Quadruped {

namespace {

auto fallback_context() noexcept -> QuadrupedRuntimeContext& {
  thread_local QuadrupedRuntimeContext context;
  return context;
}

auto installed_context() noexcept -> QuadrupedRuntimeContext*& {
  thread_local QuadrupedRuntimeContext* context = nullptr;
  return context;
}

} // namespace

ScopedQuadrupedRuntimeContext::ScopedQuadrupedRuntimeContext(
    QuadrupedRuntimeContext& context) noexcept
    : m_previous(installed_context()) {
  installed_context() = &context;
}

ScopedQuadrupedRuntimeContext::~ScopedQuadrupedRuntimeContext() {
  installed_context() = m_previous;
}

auto current_quadruped_runtime_context() noexcept -> QuadrupedRuntimeContext& {
  auto* context = installed_context();
  return context != nullptr ? *context : fallback_context();
}

} // namespace Render::Creature::Quadruped
