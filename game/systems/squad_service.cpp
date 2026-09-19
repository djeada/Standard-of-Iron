#include "squad_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/component_core.h"
#include "../core/world.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/squad.h"
#include "../units/unit.h"
#include "formation_combat_geometry.h"
#include "nav_grid.h"

namespace Game::Systems {

namespace {

auto unit_of(Engine::Core::World& world,
             Engine::Core::EntityID id) -> Engine::Core::UnitComponent* {
  return world.try_get<Engine::Core::UnitComponent>(id);
}

auto unit_of(const Engine::Core::World& world,
             Engine::Core::EntityID id) -> const Engine::Core::UnitComponent* {
  return world.try_get<Engine::Core::UnitComponent>(id);
}

auto transform_of(Engine::Core::World& world,
                  Engine::Core::EntityID id) -> Engine::Core::TransformComponent* {
  return world.try_get<Engine::Core::TransformComponent>(id);
}

auto full_max_health(const Engine::Core::UnitComponent& unit) -> int {
  const float fraction = std::max(0.01F, Game::Units::squad_fraction(unit));
  return std::max(
      1, static_cast<int>(std::lround(static_cast<float>(unit.max_health) / fraction)));
}

auto max_health_for(int men, int establishment, int establishment_max_health) -> int {
  return std::max(1,
                  static_cast<int>(std::lround(
                      static_cast<double>(establishment_max_health) *
                      static_cast<double>(men) / static_cast<double>(establishment))));
}

void apply_roster(Engine::Core::UnitComponent& unit,
                  const SquadRoster& roster,
                  int establishment,
                  int establishment_max_health) {
  unit.squad_strength = roster.men >= establishment ? 0 : roster.men;
  unit.max_health = max_health_for(roster.men, establishment, establishment_max_health);
  unit.health = std::clamp(roster.health, 1, unit.max_health);
}

auto detachment_position(const Engine::Core::TransformComponent& parent) -> QVector3D {
  constexpr float k_step = 2.5F;
  const Point origin =
      NavGrid::world_to_grid(parent.position.x + k_step, parent.position.z + k_step);
  if (const auto cell = NavGrid::find_nearest_walkable_grid(origin, 6)) {
    const QVector3D placed = NavGrid::grid_to_world(*cell);
    return {placed.x(), parent.position.y, placed.z()};
  }
  return {parent.position.x + k_step, parent.position.y, parent.position.z + k_step};
}

struct FieldPosition {
  float x = 0.0F;
  float z = 0.0F;
};

// Where the living men of a squad stand on the field, as last presented.
auto soldier_positions(const Engine::Core::World& world,
                       Engine::Core::EntityID id) -> std::vector<FieldPosition> {
  std::vector<FieldPosition> positions;
  const auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
  const auto* presentation =
      world.try_get<Engine::Core::FormationPresentationComponent>(id);
  if (transform == nullptr || presentation == nullptr) {
    return positions;
  }
  const float yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  const float sin_yaw = std::sin(yaw);
  const float cos_yaw = std::cos(yaw);
  for (const auto& soldier : presentation->soldiers) {
    if (!soldier.alive) {
      continue;
    }
    positions.push_back(FieldPosition{
        .x = transform->position.x + (cos_yaw * soldier.local_x) +
             (sin_yaw * soldier.local_z),
        .z = transform->position.z - (sin_yaw * soldier.local_x) +
             (cos_yaw * soldier.local_z),
    });
  }
  return positions;
}

// Sends the men who stood at `from` to the new slots of `squads`, closest pairs
// first, so a split or a join is a short walk rather than men appearing in
// their new ranks.
void walk_into_new_slots(Engine::Core::World& world,
                         const std::vector<Engine::Core::EntityID>& squads,
                         const std::vector<FieldPosition>& from) {
  constexpr float k_reform_seconds = 8.0F;
  constexpr float k_already_there_sq = 0.05F * 0.05F;

  struct Target {
    std::size_t squad = 0;
    std::uint16_t slot = 0;
    float x = 0.0F;
    float z = 0.0F;
  };
  std::vector<Target> targets;
  for (std::size_t squad = 0; squad < squads.size(); ++squad) {
    const auto* entity = world.get_entity(squads[squad]);
    if (entity == nullptr) {
      continue;
    }
    for (const auto& slot : FormationCombat::resolve_layout(*entity).live_slots) {
      targets.push_back(Target{
          .squad = squad, .slot = slot.index, .x = slot.world_x, .z = slot.world_z});
    }
  }

  struct Pairing {
    float distance_sq = 0.0F;
    std::size_t target = 0;
    std::size_t source = 0;
  };
  std::vector<Pairing> pairings;
  pairings.reserve(targets.size() * from.size());
  for (std::size_t target = 0; target < targets.size(); ++target) {
    for (std::size_t source = 0; source < from.size(); ++source) {
      const float dx = targets[target].x - from[source].x;
      const float dz = targets[target].z - from[source].z;
      pairings.push_back(Pairing{
          .distance_sq = (dx * dx) + (dz * dz), .target = target, .source = source});
    }
  }
  std::sort(pairings.begin(), pairings.end(), [](const Pairing& a, const Pairing& b) {
    if (a.distance_sq != b.distance_sq) {
      return a.distance_sq < b.distance_sq;
    }
    return a.target != b.target ? a.target < b.target : a.source < b.source;
  });

  std::vector<std::vector<Engine::Core::SquadReformSoldier>> walkers(squads.size());
  std::vector<bool> target_taken(targets.size(), false);
  std::vector<bool> source_taken(from.size(), false);
  for (const auto& pairing : pairings) {
    if (target_taken[pairing.target] || source_taken[pairing.source]) {
      continue;
    }
    target_taken[pairing.target] = true;
    source_taken[pairing.source] = true;
    if (pairing.distance_sq <= k_already_there_sq) {
      continue;
    }
    const auto& target = targets[pairing.target];
    walkers[target.squad].push_back(Engine::Core::SquadReformSoldier{
        .slot_index = target.slot,
        .world_x = from[pairing.source].x,
        .world_z = from[pairing.source].z,
    });
  }

  for (std::size_t squad = 0; squad < squads.size(); ++squad) {
    if (walkers[squad].empty()) {
      world.remove<Engine::Core::SquadReformComponent>(squads[squad]);
      continue;
    }
    auto* reform = world.try_get<Engine::Core::SquadReformComponent>(squads[squad]);
    if (reform == nullptr) {
      reform = world.emplace<Engine::Core::SquadReformComponent>(squads[squad]);
    }
    if (reform == nullptr) {
      continue;
    }
    reform->remaining_seconds = k_reform_seconds;
    reform->soldiers = std::move(walkers[squad]);
  }
}

struct JoinCandidate {
  Engine::Core::EntityID id = 0;
  int owner = 0;
  Game::Units::SpawnType type{};
  Game::Systems::NationID nation{};
  float x = 0.0F;
  float z = 0.0F;
  int men = 0;
  int health = 0;
  int establishment_max_health = 0;
};

auto same_group(const JoinCandidate& a, const JoinCandidate& b) -> bool {
  return a.owner == b.owner && a.type == b.type && a.nation == b.nation;
}

auto join_candidates(const Engine::Core::World& world,
                     const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<JoinCandidate> {
  std::vector<JoinCandidate> candidates;
  candidates.reserve(units.size());
  for (const auto id : units) {
    const auto* unit = unit_of(world, id);
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        Game::Units::is_building_spawn(unit->spawn_type) ||
        Game::Units::squad_establishment(unit->spawn_type) <= 1) {
      continue;
    }
    candidates.push_back(JoinCandidate{
        .id = id,
        .owner = unit->owner_id,
        .type = unit->spawn_type,
        .nation = unit->nation_id,
        .x = transform->position.x,
        .z = transform->position.z,
        .men = Game::Units::squad_survivors(*unit),
        .health = unit->health,
        .establishment_max_health = full_max_health(*unit),
    });
  }
  std::sort(candidates.begin(),
            candidates.end(),
            [](const JoinCandidate& a, const JoinCandidate& b) { return a.id < b.id; });
  candidates.erase(std::unique(candidates.begin(),
                               candidates.end(),
                               [](const JoinCandidate& a, const JoinCandidate& b) {
                                 return a.id == b.id;
                               }),
                   candidates.end());
  return candidates;
}

// Single-link clusters: a squad joins a cluster when it stands within
// k_merge_radius of any member, so a line of squads folds as one group.
auto join_clusters(const std::vector<JoinCandidate>& candidates)
    -> std::vector<std::vector<std::size_t>> {
  std::vector<std::size_t> parent(candidates.size());
  for (std::size_t i = 0; i < parent.size(); ++i) {
    parent[i] = i;
  }
  const auto root = [&parent](std::size_t i) {
    while (parent[i] != i) {
      parent[i] = parent[parent[i]];
      i = parent[i];
    }
    return i;
  };
  constexpr float k_radius_sq =
      SquadService::k_merge_radius * SquadService::k_merge_radius;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    for (std::size_t j = i + 1; j < candidates.size(); ++j) {
      if (!same_group(candidates[i], candidates[j])) {
        continue;
      }
      const float dx = candidates[i].x - candidates[j].x;
      const float dz = candidates[i].z - candidates[j].z;
      if ((dx * dx) + (dz * dz) <= k_radius_sq) {
        parent[root(j)] = root(i);
      }
    }
  }
  std::unordered_map<std::size_t, std::vector<std::size_t>> by_root;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    by_root[root(i)].push_back(i);
  }
  std::vector<std::vector<std::size_t>> clusters;
  for (auto& [key, members] : by_root) {
    if (members.size() >= 2U) {
      clusters.push_back(std::move(members));
    }
  }
  std::sort(clusters.begin(), clusters.end());
  return clusters;
}

} // namespace

auto SquadService::share_health(const std::vector<int>& men,
                                int health,
                                int establishment,
                                int establishment_max_health)
    -> std::vector<SquadRoster> {
  std::vector<SquadRoster> rosters;
  std::vector<int> floors;
  std::vector<int> ceilings;
  rosters.reserve(men.size());
  long long total_men = 0;
  long long floor_sum = 0;
  long long ceiling_sum = 0;
  for (const int count : men) {
    const int squad_men = std::max(0, count);
    const int ceiling =
        squad_men > 0
            ? max_health_for(squad_men, establishment, establishment_max_health)
            : 0;
    // The least health at which the squad still shows all of its men.
    const int floor =
        squad_men > 0
            ? static_cast<int>((static_cast<long long>(squad_men - 1) * ceiling) /
                               squad_men) +
                  1
            : 0;
    rosters.push_back(SquadRoster{.men = squad_men, .health = 0});
    floors.push_back(floor);
    ceilings.push_back(ceiling);
    total_men += squad_men;
    floor_sum += floor;
    ceiling_sum += ceiling;
  }
  if (total_men == 0) {
    return rosters;
  }

  const long long target =
      std::clamp(static_cast<long long>(std::max(0, health)), floor_sum, ceiling_sum);

  // Deal the pool in proportion to men, then keep each share inside its band.
  long long running_men = 0;
  long long handed_out = 0;
  long long dealt = 0;
  for (std::size_t i = 0; i < rosters.size(); ++i) {
    running_men += rosters[i].men;
    const long long due = ((target * running_men) + (total_men / 2)) / total_men;
    rosters[i].health =
        std::clamp(static_cast<int>(due - handed_out), floors[i], ceilings[i]);
    handed_out = due;
    dealt += rosters[i].health;
  }

  // Clamping can leave the total off by a little; settle it one point at a
  // time against the squads with room, so the total is exactly the target.
  while (dealt != target) {
    const int step = dealt < target ? 1 : -1;
    bool moved = false;
    for (std::size_t i = 0; i < rosters.size() && dealt != target; ++i) {
      const int next = rosters[i].health + step;
      if (next < floors[i] || next > ceilings[i]) {
        continue;
      }
      rosters[i].health = next;
      dealt += step;
      moved = true;
    }
    if (!moved) {
      break;
    }
  }
  return rosters;
}

auto SquadService::pack(int men,
                        int health,
                        int establishment,
                        int establishment_max_health) -> std::vector<SquadRoster> {
  const int full = std::max(1, establishment);
  std::vector<int> sizes;
  for (int left = std::max(0, men); left > 0; left -= full) {
    sizes.push_back(std::min(full, left));
  }
  return share_health(sizes, health, full, establishment_max_health);
}

auto SquadService::can_divide(const Engine::Core::World& world,
                              Engine::Core::EntityID unit_id) -> bool {
  const auto* unit = unit_of(world, unit_id);
  return unit != nullptr && Game::Units::squad_can_divide(*unit);
}

void SquadService::apply_strength(Engine::Core::World& world,
                                  Engine::Core::EntityID unit_id,
                                  int strength) {
  auto* unit = unit_of(world, unit_id);
  if (unit == nullptr || unit->max_health <= 0) {
    return;
  }
  const int establishment = Game::Units::squad_establishment(unit->spawn_type);
  const int establishment_max_health = full_max_health(*unit);
  const int men = std::clamp(strength, 1, establishment);
  const double ratio = std::clamp(static_cast<double>(unit->health) /
                                      static_cast<double>(unit->max_health),
                                  0.01,
                                  1.0);
  const int ceiling = max_health_for(men, establishment, establishment_max_health);
  apply_roster(*unit,
               SquadRoster{.men = men,
                           .health = static_cast<int>(
                               std::lround(static_cast<double>(ceiling) * ratio))},
               establishment,
               establishment_max_health);
}

auto SquadService::divide(Engine::Core::World& world,
                          Engine::Core::EntityID unit_id) -> SquadDivision {
  SquadDivision result;
  auto* unit = unit_of(world, unit_id);
  auto* transform = transform_of(world, unit_id);
  if (unit == nullptr || transform == nullptr ||
      !Game::Units::squad_can_divide(*unit)) {
    return result;
  }

  auto registry = Game::Map::MapTransformer::get_factory_registry();
  if (!registry) {
    return result;
  }

  const int establishment = Game::Units::squad_establishment(unit->spawn_type);
  const int establishment_max_health = full_max_health(*unit);
  const int survivors = Game::Units::squad_survivors(*unit);
  const int detached = survivors / 2;
  const auto rosters = share_health({survivors - detached, detached},
                                    unit->health,
                                    establishment,
                                    establishment_max_health);
  const auto ranks = soldier_positions(world, unit_id);

  Game::Units::SpawnParams params;
  params.position = detachment_position(*transform);
  params.rotation_y = transform->rotation.y;
  params.player_id = unit->owner_id;
  params.spawn_type = unit->spawn_type;
  params.ai_controlled =
      world.try_get<Engine::Core::AIControlledComponent>(unit_id) != nullptr;
  params.nation_id = unit->nation_id;
  params.is_initial_spawn = false;

  auto spawned = registry->create(unit->spawn_type, world, params);
  if (!spawned) {
    return result;
  }

  auto* parent_unit = unit_of(world, unit_id);
  auto* child_unit = unit_of(world, spawned->id());
  if (parent_unit == nullptr || child_unit == nullptr) {
    return result;
  }

  apply_roster(*parent_unit, rosters[0], establishment, establishment_max_health);
  apply_roster(*child_unit, rosters[1], establishment, establishment_max_health);
  walk_into_new_slots(world, {unit_id, spawned->id()}, ranks);
  result.parent = unit_id;
  result.detachment = spawned->id();
  return result;
}

auto SquadService::plan_joins(const Engine::Core::World& world,
                              const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<SquadJoinPlan> {
  const auto candidates = join_candidates(world, units);
  std::vector<SquadJoinPlan> plans;
  for (auto cluster : join_clusters(candidates)) {
    std::sort(
        cluster.begin(), cluster.end(), [&candidates](std::size_t a, std::size_t b) {
          if (candidates[a].men != candidates[b].men) {
            return candidates[a].men > candidates[b].men;
          }
          return candidates[a].id < candidates[b].id;
        });

    int men = 0;
    int health = 0;
    int establishment_max_health = 0;
    for (const auto index : cluster) {
      men += candidates[index].men;
      health += candidates[index].health;
      establishment_max_health = std::max(establishment_max_health,
                                          candidates[index].establishment_max_health);
    }
    const auto& lead = candidates[cluster.front()];
    auto rosters = pack(men,
                        health,
                        Game::Units::squad_establishment(lead.type),
                        establishment_max_health);

    bool unchanged = rosters.size() == cluster.size();
    for (std::size_t i = 0; unchanged && i < rosters.size(); ++i) {
      unchanged = rosters[i].men == candidates[cluster[i]].men;
    }
    if (unchanged) {
      continue;
    }

    SquadJoinPlan plan;
    plan.rosters = std::move(rosters);
    for (const auto index : cluster) {
      plan.members.push_back(candidates[index].id);
    }
    plans.push_back(std::move(plan));
  }
  return plans;
}

auto SquadService::can_merge(const Engine::Core::World& world,
                             Engine::Core::EntityID kept,
                             Engine::Core::EntityID absorbed) -> bool {
  return kept != absorbed && !plan_joins(world, {kept, absorbed}).empty();
}

auto SquadService::merge(Engine::Core::World& world,
                         Engine::Core::EntityID kept,
                         Engine::Core::EntityID absorbed) -> bool {
  return kept != absorbed && !merge_all(world, {kept, absorbed}).empty();
}

auto SquadService::divide_all(Engine::Core::World& world,
                              const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<SquadDivision> {
  std::vector<SquadDivision> divisions;
  divisions.reserve(units.size());
  for (const auto id : units) {
    auto division = divide(world, id);
    if (division.detachment != 0) {
      divisions.push_back(division);
    }
  }
  return divisions;
}

auto SquadService::merge_all(Engine::Core::World& world,
                             const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<SquadMerge> {
  std::vector<SquadMerge> merges;
  for (const auto& plan : plan_joins(world, units)) {
    auto* lead = unit_of(world, plan.members.front());
    if (lead == nullptr) {
      continue;
    }
    const int establishment = Game::Units::squad_establishment(lead->spawn_type);
    const int establishment_max_health = full_max_health(*lead);
    std::vector<FieldPosition> ranks;
    for (const auto member : plan.members) {
      const auto positions = soldier_positions(world, member);
      ranks.insert(ranks.end(), positions.begin(), positions.end());
    }
    for (std::size_t i = 0; i < plan.members.size(); ++i) {
      if (i < plan.rosters.size()) {
        if (auto* unit = unit_of(world, plan.members[i]); unit != nullptr) {
          apply_roster(*unit, plan.rosters[i], establishment, establishment_max_health);
        }
        continue;
      }
      merges.push_back(
          SquadMerge{.kept = plan.members.front(), .absorbed = plan.members[i]});
      world.destroy_entity(plan.members[i]);
    }
    if (plan.rosters.size() == plan.members.size()) {
      merges.push_back(SquadMerge{.kept = plan.members.front(), .absorbed = 0});
    }
    walk_into_new_slots(
        world,
        std::vector<Engine::Core::EntityID>(
            plan.members.begin(),
            plan.members.begin() + static_cast<std::ptrdiff_t>(std::min(
                                       plan.rosters.size(), plan.members.size()))),
        ranks);
  }
  return merges;
}

} // namespace Game::Systems
