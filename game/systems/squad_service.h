#pragma once

#include <vector>

#include "../core/component.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct SquadDivision {

  Engine::Core::EntityID parent = 0;

  Engine::Core::EntityID detachment = 0;
};

// One squad folded into another; `absorbed` is 0 when a join only moved men
// between squads that all survive it (two squads of ten become twelve and eight).
struct SquadMerge {

  Engine::Core::EntityID kept = 0;

  Engine::Core::EntityID absorbed = 0;
};

// The men and health one squad ends up with after a split or a join.
struct SquadRoster {
  int men = 0;
  int health = 0;
};

// One group of squads that fold together: the first `rosters.size()` members
// are kept and take those rosters in order, every later member is absorbed.
struct SquadJoinPlan {
  std::vector<Engine::Core::EntityID> members;
  std::vector<SquadRoster> rosters;
};

class SquadService {
public:
  static constexpr float k_merge_radius = 14.0F;

  // Deals `health` over squads of the given sizes in proportion to their men.
  // Every squad lands between the least health that still shows all of its
  // men and a full pool, and the total is exactly `health` clamped to the sum
  // of those bands -- a join never loses a man and never heals a squad whole.
  [[nodiscard]] static auto
  share_health(const std::vector<int>& men,
               int health,
               int establishment,
               int establishment_max_health) -> std::vector<SquadRoster>;

  // Packs `men` into as few squads as the establishment allows: full squads
  // first, the remainder in one last squad.
  [[nodiscard]] static auto
  pack(int men, int health, int establishment, int establishment_max_health)
      -> std::vector<SquadRoster>;

  [[nodiscard]] static auto can_divide(const Engine::Core::World& world,
                                       Engine::Core::EntityID unit_id) -> bool;

  [[nodiscard]] static auto divide(Engine::Core::World& world,
                                   Engine::Core::EntityID unit_id) -> SquadDivision;

  // Groups the units by owner, kind and nation, links every squad within
  // k_merge_radius of another into one cluster, and plans each cluster whose
  // men would pack differently.
  [[nodiscard]] static auto plan_joins(const Engine::Core::World& world,
                                       const std::vector<Engine::Core::EntityID>& units)
      -> std::vector<SquadJoinPlan>;

  [[nodiscard]] static auto can_merge(const Engine::Core::World& world,
                                      Engine::Core::EntityID kept,
                                      Engine::Core::EntityID absorbed) -> bool;

  [[nodiscard]] static auto merge(Engine::Core::World& world,
                                  Engine::Core::EntityID kept,
                                  Engine::Core::EntityID absorbed) -> bool;

  static auto divide_all(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& units)
      -> std::vector<SquadDivision>;

  static auto merge_all(Engine::Core::World& world,
                        const std::vector<Engine::Core::EntityID>& units)
      -> std::vector<SquadMerge>;

  static void apply_strength(Engine::Core::World& world,
                             Engine::Core::EntityID unit_id,
                             int strength);
};

} // namespace Game::Systems
