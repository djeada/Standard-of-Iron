#include "navigation_service.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "../core/ambient_session.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

std::atomic<std::uint64_t> g_navigation_instances{0};

} // namespace

NavigationService::NavigationService()
    : m_instance_id(g_navigation_instances.fetch_add(1, std::memory_order_relaxed) +
                    1) {
}

NavigationService::~NavigationService() = default;

void NavigationService::initialize(int world_width, int world_height) {
  m_gate_blockers.clear();
  m_pathfinder = std::make_unique<Pathfinding>(world_width, world_height);

  const float offset_x = -((static_cast<float>(world_width) * 0.5F) - 0.5F);
  const float offset_z = -((static_cast<float>(world_height) * 0.5F) - 0.5F);
  m_pathfinder->set_grid_offset(offset_x, offset_z);
}

void NavigationService::clear() {
  m_gate_blockers.clear();
  m_pathfinder.reset();
}

auto NavigationService::active_or_null() -> NavigationService* {
  const auto* services = Game::Session::ambient_services_or_null();
  return services != nullptr ? services->navigation : nullptr;
}

auto NavigationService::active() -> NavigationService& {
  if (auto* service = active_or_null()) {
    return *service;
  }
  std::fputs("No Game::Session::SessionContext is active on this thread or "
             "process. Construct one and make it active (ScopedSession) before "
             "using navigation.\n",
             stderr);
  std::abort();
}

} // namespace Game::Systems
