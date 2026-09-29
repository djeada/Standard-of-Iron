#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../../core/entity.h"

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct CombatQueryContext;

class FormationRanks {
public:
  explicit FormationRanks(Engine::Core::World& world);

  [[nodiscard]] auto is_reserve(Engine::Core::Entity* entity) const -> bool;

private:
  struct Member {
    int rank = 0;
    Engine::Core::EntityID id = 0;
  };

  Engine::Core::World& m_world;
  std::unordered_map<std::uint64_t, std::vector<Member>> m_members;
};

[[nodiscard]] auto
should_prioritize_healing(Engine::Core::Entity* healer,
                          const CombatQueryContext& query_context) -> bool;

} // namespace Game::Systems::Combat
