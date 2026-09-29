#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "../ai_types.h"

namespace Game::Systems::AI {

using SourNodes = std::unordered_map<std::uint64_t, float>;

class WorkerStallWatch {
public:
  void review(const AISnapshot& snapshot, float now);

  [[nodiscard]] auto is_stalled(Engine::Core::EntityID builder_id) const -> bool {
    return m_stalled_builders.contains(builder_id);
  }
  [[nodiscard]] auto sour_nodes() const noexcept -> const SourNodes& {
    return m_sour_nodes;
  }

private:
  struct WorkerWatch {
    std::uint64_t task_target_id = 0;
    float site_x = 0.0F;
    float site_z = 0.0F;
    float since = 0.0F;
  };

  std::unordered_map<Engine::Core::EntityID, WorkerWatch> m_worker_watch;
  SourNodes m_sour_nodes;
  std::unordered_set<Engine::Core::EntityID> m_stalled_builders;
};

[[nodiscard]] auto
node_is_sour(const SourNodes& sour, std::uint64_t node_id, float now) -> bool;

} // namespace Game::Systems::AI
