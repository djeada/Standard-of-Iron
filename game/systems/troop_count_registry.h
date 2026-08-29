#pragma once

#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class TroopCountRegistry {
public:
  TroopCountRegistry() = default;
  ~TroopCountRegistry() = default;
  TroopCountRegistry(const TroopCountRegistry&) = delete;
  TroopCountRegistry(TroopCountRegistry&&) = delete;
  auto operator=(const TroopCountRegistry&) -> TroopCountRegistry& = delete;
  auto operator=(TroopCountRegistry&&) -> TroopCountRegistry& = delete;

  static auto instance() -> TroopCountRegistry&;

  void clear();

  [[nodiscard]] auto get_troop_count(int owner_id) const -> int;

  void rebuild_from_world(const Engine::Core::World& world);

private:
  std::unordered_map<int, int> m_troop_counts;
};

} // namespace Game::Systems
