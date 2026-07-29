#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "resource_types.h"

namespace Game::Systems {

class PlayerResourceRegistry {
public:
  PlayerResourceRegistry() = default;
  ~PlayerResourceRegistry() = default;
  PlayerResourceRegistry(const PlayerResourceRegistry&) = delete;
  PlayerResourceRegistry(PlayerResourceRegistry&&) = delete;
  auto operator=(const PlayerResourceRegistry&) -> PlayerResourceRegistry& = delete;
  auto operator=(PlayerResourceRegistry&&) -> PlayerResourceRegistry& = delete;

  static auto instance() -> PlayerResourceRegistry&;

  void clear() {
    m_resources_by_owner.clear();
    m_harvested_by_owner.clear();
  }

  void ensure_owner(int owner_id) {
    if (owner_id <= 0) {
      return;
    }
    m_resources_by_owner.try_emplace(owner_id);
    m_harvested_by_owner.try_emplace(owner_id);
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

  void add_harvested(int owner_id, ResourceType type, int amount) {
    if (owner_id <= 0 || amount <= 0) {
      return;
    }
    add(owner_id, type, amount);
    m_harvested_by_owner[owner_id].add(type, amount);
  }

  [[nodiscard]] auto get_harvested_all(int owner_id) const -> ResourceAmounts {
    const auto it = m_harvested_by_owner.find(owner_id);
    return it != m_harvested_by_owner.end() ? it->second : ResourceAmounts{};
  }

  [[nodiscard]] auto
  has_harvested_at_least(int owner_id, const ResourceAmounts& required) const -> bool {
    return get_harvested_all(owner_id).can_cover(required);
  }

  [[nodiscard]] auto snapshot() const -> std::vector<OwnerResourceState> {
    return build_snapshot(m_resources_by_owner);
  }

  [[nodiscard]] auto harvested_snapshot() const -> std::vector<OwnerResourceState> {
    return build_snapshot(m_harvested_by_owner);
  }

  void restore(const std::vector<OwnerResourceState>& resources_by_owner) {
    apply_snapshot(resources_by_owner, m_resources_by_owner);
  }

  void restore_harvested(const std::vector<OwnerResourceState>& harvested_by_owner) {
    apply_snapshot(harvested_by_owner, m_harvested_by_owner);
  }

private:
  using OwnerAmounts = std::unordered_map<int, ResourceAmounts>;

  [[nodiscard]] static auto
  build_snapshot(const OwnerAmounts& source) -> std::vector<OwnerResourceState> {
    std::vector<OwnerResourceState> rows;
    rows.reserve(source.size());
    for (const auto& [owner_id, amounts] : source) {
      rows.push_back({.owner_id = owner_id, .amounts = amounts});
    }
    std::sort(rows.begin(),
              rows.end(),
              [](const OwnerResourceState& lhs, const OwnerResourceState& rhs) {
                return lhs.owner_id < rhs.owner_id;
              });
    return rows;
  }

  static void apply_snapshot(const std::vector<OwnerResourceState>& rows,
                             OwnerAmounts& target) {
    target.clear();
    for (const auto& row : rows) {
      if (row.owner_id <= 0) {
        continue;
      }
      target[row.owner_id] = row.amounts;
    }
  }

  OwnerAmounts m_resources_by_owner;
  OwnerAmounts m_harvested_by_owner;
};

} // namespace Game::Systems
