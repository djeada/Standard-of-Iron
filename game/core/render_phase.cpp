#include "render_phase.h"

#include <QtGlobal>

#include <atomic>

namespace Engine::Core::RenderPhase {
namespace {

std::atomic<std::uint64_t> g_violations{0};
constexpr std::uint64_t k_logged_violations = 20;

} // namespace

void report_violation(const char* accessor) noexcept {
  if (g_violations.fetch_add(1, std::memory_order_relaxed) < k_logged_violations) {
    qCritical("render phase reached the live world through %s; draw from the "
              "render snapshot or the presentation frame",
              accessor);
  }
}

auto violation_count() noexcept -> std::uint64_t {
  return g_violations.load(std::memory_order_relaxed);
}

} // namespace Engine::Core::RenderPhase
