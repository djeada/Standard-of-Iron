#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "resource_types.h"

namespace Game::Systems {

class PlayerResourceRegistry {
public:
  static auto instance() -> PlayerResourceRegistry& {
    static PlayerResourceRegistry s_instance;
    return s_instance;
  }

  void clear() { m_resources_by_owner.clear(); }

  void ensure_owner(int owner_id) {
    if (owner_id <= 0) {
      return;
    }
    m_resources_by_owner.try_emplace(owner_id);
  }

  [[nodiscard]] auto get(int owner_id, ResourceType type) const -> int {
    const auto it = m_resources_by_owner.find(owner_id);
    return it != m_resources_by_owner.end() ? it->second.get(type) : 0;
  }

  [[nodiscard]] auto get_all(int owner_id) const -> ResourceAmounts {
    const auto it = m_resources_by_owner.find(owner_id);
    return it != m_resources_by_owner.end() ? it->second : ResourceAmounts{};
  }

  [[nodiscard]] auto has_at_least(int owner_id,
                                  const ResourceAmounts& required) const -> bool {
    return get_all(owner_id).can_cover(required);
  }

  void set(int owner_id, ResourceType type, int amount) {
    if (owner_id <= 0) {
      return;
    }
    m_resources_by_owner[owner_id].set(type, amount);
  }

  void add(int owner_id, ResourceType type, int delta) {
    if (owner_id <= 0) {
      return;
    }
    m_resources_by_owner[owner_id].add(type, delta);
  }

  void spend(int owner_id, const ResourceAmounts& cost) {
    if (owner_id <= 0) {
      return;
    }
    for (ResourceType const type : k_all_resource_types) {
      int const amount = cost.get(type);
      if (amount > 0) {
        add(owner_id, type, -amount);
      }
    }
  }

  [[nodiscard]] auto snapshot() const -> std::vector<OwnerResourceState> {
    std::vector<OwnerResourceState> rows;
    rows.reserve(m_resources_by_owner.size());
    for (const auto& [owner_id, amounts] : m_resources_by_owner) {
      rows.push_back({.owner_id = owner_id, .amounts = amounts});
    }
    std::sort(rows.begin(),
              rows.end(),
              [](const OwnerResourceState& lhs, const OwnerResourceState& rhs) {
                return lhs.owner_id < rhs.owner_id;
              });
    return rows;
  }

  void restore(const std::vector<OwnerResourceState>& resources_by_owner) {
    m_resources_by_owner.clear();
    for (const auto& row : resources_by_owner) {
      if (row.owner_id <= 0) {
        continue;
      }
      m_resources_by_owner[row.owner_id] = row.amounts;
    }
  }

private:
  PlayerResourceRegistry() = default;

  std::unordered_map<int, ResourceAmounts> m_resources_by_owner;
};

} // namespace Game::Systems
