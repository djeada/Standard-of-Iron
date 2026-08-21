#include "system_schedule.h"

#include <algorithm>
#include <utility>

namespace Engine::Core {

auto phase_name(SystemPhase phase) noexcept -> const char* {
  switch (phase) {
  case SystemPhase::Input:
    return "input";
  case SystemPhase::Movement:
    return "movement";
  case SystemPhase::Combat:
    return "combat";
  case SystemPhase::Strategy:
    return "strategy";
  case SystemPhase::Economy:
    return "economy";
  case SystemPhase::Ambient:
    return "ambient";
  case SystemPhase::Presentation:
    return "presentation";
  case SystemPhase::Cleanup:
    return "cleanup";
  case SystemPhase::_Count:
    break;
  }
  return "?";
}

namespace {

auto intersects(const std::vector<ComponentTypeId>& lhs,
                const std::vector<ComponentTypeId>& rhs) -> bool {
  for (const ComponentTypeId id : lhs) {
    if (std::find(rhs.begin(), rhs.end(), id) != rhs.end()) {
      return true;
    }
  }
  return false;
}

} // namespace

auto SystemAccess::conflicts_with(const SystemAccess& other) const -> bool {

  if (exclusive || other.exclusive) {
    return true;
  }

  return intersects(writes, other.writes) || intersects(writes, other.reads) ||
         intersects(reads, other.writes);
}

auto plan_phase_batches(std::span<const SystemAccess> systems)
    -> std::vector<std::vector<std::size_t>> {
  std::vector<std::vector<std::size_t>> batches;
  if (systems.empty()) {
    return batches;
  }

  std::vector<std::size_t> current;
  for (std::size_t index = 0; index < systems.size(); ++index) {
    const bool collides =
        std::any_of(current.begin(), current.end(), [&](std::size_t member) {
          return systems[index].conflicts_with(systems[member]);
        });
    if (collides && !current.empty()) {
      batches.push_back(std::move(current));
      current.clear();
    }
    current.push_back(index);
  }
  if (!current.empty()) {
    batches.push_back(std::move(current));
  }
  return batches;
}

} // namespace Engine::Core
