#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "nav_grid_types.h"

namespace Game::Systems {

class Pathfinding;

class NavigationService {
public:
  NavigationService();
  ~NavigationService();

  NavigationService(const NavigationService&) = delete;
  auto operator=(const NavigationService&) -> NavigationService& = delete;
  NavigationService(NavigationService&&) = delete;
  auto operator=(NavigationService&&) -> NavigationService& = delete;

  void initialize(int world_width, int world_height);

  void clear();

  [[nodiscard]] auto pathfinder() -> Pathfinding* { return m_pathfinder.get(); }
  [[nodiscard]] auto pathfinder() const -> const Pathfinding* {
    return m_pathfinder.get();
  }

  [[nodiscard]] auto gate_blockers() -> std::vector<GateBlocker>& {
    return m_gate_blockers;
  }
  [[nodiscard]] auto gate_blockers() const -> const std::vector<GateBlocker>& {
    return m_gate_blockers;
  }

  [[nodiscard]] auto instance_id() const -> std::uint64_t { return m_instance_id; }

  [[nodiscard]] static auto active() -> NavigationService&;

  [[nodiscard]] static auto active_or_null() -> NavigationService*;

private:
  std::unique_ptr<Pathfinding> m_pathfinder;
  std::vector<GateBlocker> m_gate_blockers;
  std::uint64_t m_instance_id;
};

} // namespace Game::Systems
