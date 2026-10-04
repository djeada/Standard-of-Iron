#include "siege_tower_system.h"

#include <QVector3D>
#include <queue>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_core.h"
#include "../core/component_gameplay.h"
#include "../core/component_presentation.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/map_transformer.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "../util/planar_math.h"
#include "core/wall_walk_geometry.h"
#include "movement/command_service.h"
#include "navigation/wall_walk_path.h"
#include "owner_registry.h"

namespace Game::Systems {

namespace {

using Engine::Core::EntityID;
using Engine::Core::MovementComponent;
using Engine::Core::SiegeTowerComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::WallSegmentComponent;
using Engine::Core::WallWalkerComponent;
using Engine::Core::WallWalkSegment;
using Phase = WallWalkerComponent::Phase;
namespace WW = WallWalk;

constexpr float k_pi = std::numbers::pi_v<float>;

constexpr float k_dock_reach = 5.0F;
constexpr float k_dock_standoff = 1.80F;
constexpr float k_dock_settle_speed = 0.8F;
constexpr float k_dock_turn_degrees_per_second = 70.0F;
constexpr float k_dock_give_up_distance = 9.0F;

constexpr float k_tower_door_depth = 0.20F;
constexpr float k_tower_lip_depth = 0.80F;

constexpr float k_tower_rear_reach = 2.1F;
constexpr float k_tower_ladder_depth = -0.45F;

constexpr float k_escort_call_radius = 14.0F;
constexpr float k_rear_board_radius = 3.2F;
constexpr float k_tower_arrival = 1.1F;
constexpr float k_tower_arrival_idle = 2.6F;

constexpr float k_wall_link_reach = 2.7F;
constexpr float k_node_arrival = 0.12F;
constexpr float k_path_radius = 18.0F;

constexpr float k_board_seconds_per_man = 0.85F;
constexpr float k_board_seconds_slack = 8.0F;
constexpr int k_board_default_company = 24;

constexpr float k_leave_seconds_per_man = 1.6F;
constexpr float k_leave_seconds_slack = 5.0F;
constexpr float k_leave_distance = 16.0F;

constexpr float k_recent_stair_seconds = 8.0F;

constexpr float k_ladder_detour = 4.0F;
constexpr float k_stair_speed = 0.9F;
constexpr float k_deck_speed_scale = 0.75F;
constexpr float k_stair_arrival = 0.9F;
constexpr float k_stair_arrival_idle = 2.2F;
constexpr float k_approach_order_slack = 0.6F;

auto board_seconds(Engine::Core::World& world, EntityID id) -> float {
  int men = k_board_default_company;
  if (auto const* roster =
          world.try_get<Engine::Core::FormationRosterPresentationComponent>(id);
      roster != nullptr && roster->total_count > 0) {
    men = roster->total_count;
  }
  return static_cast<float>(men) * k_board_seconds_per_man + k_board_seconds_slack;
}

struct WallNode {
  EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  std::int8_t inner_x{0};
  std::int8_t inner_z{0};
  bool stair{false};
  bool ladder{false};
  std::uint8_t mask{0};

  [[nodiscard]] auto lane() const -> WW::Point {
    return WW::lane_point(x, z, inner_x, inner_z);
  }
  [[nodiscard]] auto walkable() const -> bool { return inner_x != 0 || inner_z != 0; }
  [[nodiscard]] auto access() const -> bool { return stair || ladder; }

  [[nodiscard]] auto by_ladder() const -> bool { return ladder && !stair; }
};

auto is_live_wall(Engine::Core::World& world,
                  EntityID id,
                  const UnitComponent& unit) -> bool {
  return unit.health > 0 && !world.has<Engine::Core::PendingRemovalComponent>(id) &&
         unit.spawn_type == Game::Units::SpawnType::WallSegment;
}

auto make_node(EntityID id,
               const TransformComponent& transform,
               const WallSegmentComponent* wall) -> WallNode {
  WallNode node{id, transform.position.x, transform.position.z};
  if (wall != nullptr) {
    node.inner_x = wall->inner_x;
    node.inner_z = wall->inner_z;
    node.stair = wall->has_stair;
    node.ladder = wall->ladder == WallSegmentComponent::Ladder::Standing;
    node.mask = wall->connection_mask;
  }
  return node;
}

auto gather_walls(Engine::Core::World& world, int owner_id) -> std::vector<WallNode> {
  std::vector<WallNode> nodes;
  for (auto [id, unit, transform] :
       world.view<const UnitComponent, const TransformComponent>()) {
    if (unit.owner_id != owner_id || !is_live_wall(world, id, unit)) {
      continue;
    }
    auto node = make_node(id, transform, world.try_get<WallSegmentComponent>(id));
    if (node.walkable()) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

auto linked(const WallNode& a, const WallNode& b) -> bool {
  return std::hypot(a.x - b.x, a.z - b.z) <= k_wall_link_reach;
}

auto index_of(const std::vector<WallNode>& nodes, EntityID id) -> int {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

auto nearest_node(const std::vector<WallNode>& nodes, float x, float z) -> int {
  int best = -1;
  float best_d = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    auto const lane = nodes[i].lane();
    float const d = std::min(std::hypot(lane.x - x, lane.z - z),
                             std::hypot(nodes[i].x - x, nodes[i].z - z));
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}

auto distance_to_node(const WallNode& node, float x, float z) -> float {
  auto const lane = node.lane();
  return std::min(std::hypot(lane.x - x, lane.z - z),
                  std::hypot(node.x - x, node.z - z));
}

auto is_wall_order(const WallNode& node, float x, float z) -> bool {
  return WW::is_wall_walk_order(node.x, node.z, node.inner_x, node.inner_z, x, z);
}

class WallIndex {
public:
  explicit WallIndex(std::vector<WallNode> nodes)
      : m_nodes(std::move(nodes)) {
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
      m_cells.emplace(key(m_nodes[i].x, m_nodes[i].z), static_cast<int>(i));
    }
  }
  [[nodiscard]] auto nodes() const -> const std::vector<WallNode>& { return m_nodes; }
  [[nodiscard]] auto wall_order_at(float x, float z) const -> int {
    auto const cx = static_cast<std::int64_t>(std::lround(x / 2.0F));
    auto const cz = static_cast<std::int64_t>(std::lround(z / 2.0F));
    for (std::int64_t ox = -1; ox <= 1; ++ox) {
      for (std::int64_t oz = -1; oz <= 1; ++oz) {
        auto const [first, last] = m_cells.equal_range(pack(cx + ox, cz + oz));
        for (auto it = first; it != last; ++it) {
          if (is_wall_order(m_nodes[static_cast<std::size_t>(it->second)], x, z)) {
            return it->second;
          }
        }
      }
    }
    return -1;
  }

private:
  static auto pack(std::int64_t cx, std::int64_t cz) -> std::int64_t {
    return (cx << 32) ^ (cz & 0xFFFFFFFF);
  }
  static auto key(float x, float z) -> std::int64_t {
    return pack(std::lround(x / 2.0F), std::lround(z / 2.0F));
  }
  std::vector<WallNode> m_nodes;
  std::unordered_multimap<std::int64_t, int> m_cells;
};

auto hop_distances(const std::vector<WallNode>& nodes, int from) -> std::vector<int> {
  std::vector<int> hops(nodes.size(), -1);
  if (from < 0) {
    return hops;
  }
  std::queue<int> frontier;
  frontier.push(from);
  hops[static_cast<std::size_t>(from)] = 0;
  while (!frontier.empty()) {
    int const at = frontier.front();
    frontier.pop();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      if (hops[i] < 0 && linked(nodes[static_cast<std::size_t>(at)], nodes[i])) {
        hops[i] = hops[static_cast<std::size_t>(at)] + 1;
        frontier.push(static_cast<int>(i));
      }
    }
  }
  return hops;
}

auto next_hop(const std::vector<WallNode>& nodes, int from, int to) -> int {
  if (from == to || from < 0 || to < 0) {
    return to;
  }
  auto const hops = hop_distances(nodes, to);
  if (hops[static_cast<std::size_t>(from)] < 0) {
    return from;
  }
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (hops[i] >= 0 && hops[i] == hops[static_cast<std::size_t>(from)] - 1 &&
        linked(nodes[static_cast<std::size_t>(from)], nodes[i])) {
      return static_cast<int>(i);
    }
  }
  return from;
}

auto access_foot(const WallNode& node) -> WW::Point {
  return node.by_ladder()
             ? WW::ladder_approach(node.x, node.z, node.inner_x, node.inner_z)
             : WW::stair_foot(node.x, node.z, node.inner_x, node.inner_z);
}

auto best_stair(const std::vector<WallNode>& nodes,
                int on_deck,
                float x,
                float z,
                int goal) -> int {
  auto const from_goal = hop_distances(nodes, goal);
  auto const from_deck = hop_distances(nodes, on_deck);
  int best = -1;
  float best_cost = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (!nodes[i].access()) {
      continue;
    }
    float cost = nodes[i].by_ladder() ? k_ladder_detour : 0.0F;
    if (goal >= 0) {
      if (from_goal[i] < 0) {
        continue;
      }
      cost += static_cast<float>(from_goal[i]) * 2.0F;
    }
    if (on_deck >= 0) {
      if (from_deck[i] < 0) {
        continue;
      }
      cost += static_cast<float>(from_deck[i]) * 2.0F;
    }
    auto const foot = access_foot(nodes[i]);
    cost += std::hypot(foot.x - x, foot.z - z);
    if (cost < best_cost) {
      best_cost = cost;
      best = static_cast<int>(i);
    }
  }
  return best;
}

struct StairLine {
  WW::Point foot;
  WW::Point ground;
  WW::Point edge;
  WW::Point top;
};

auto stair_line(const WallNode& node) -> StairLine {
  auto const out = [&](float reach) {
    return WW::Point{node.x + static_cast<float>(node.inner_x) * reach,
                     node.z + static_cast<float>(node.inner_z) * reach};
  };
  if (node.by_ladder()) {
    return {WW::ladder_approach(node.x, node.z, node.inner_x, node.inner_z),
            WW::ladder_foot(node.x, node.z, node.inner_x, node.inner_z),
            out(WW::k_deck_outer_edge),
            node.lane()};
  }
  return {WW::stair_foot(node.x, node.z, node.inner_x, node.inner_z),
          out(WW::k_deck_outer_edge + WW::k_stair_run),
          out(WW::k_deck_outer_edge),
          node.lane()};
}

auto stair_length(const StairLine& s) -> float {
  return std::hypot(s.foot.x - s.ground.x, s.foot.z - s.ground.z) +
         std::hypot(std::hypot(s.ground.x - s.edge.x, s.ground.z - s.edge.z),
                    WW::k_deck_height) +
         std::hypot(s.edge.x - s.top.x, s.edge.z - s.top.z);
}

struct StairPose {
  float x{0.0F};
  float z{0.0F};
  float y{0.0F};
};

auto stair_pose(const StairLine& s, float progress) -> StairPose {
  float const a = std::hypot(s.foot.x - s.ground.x, s.foot.z - s.ground.z);
  float const b = std::hypot(std::hypot(s.ground.x - s.edge.x, s.ground.z - s.edge.z),
                             WW::k_deck_height);
  float const c = std::hypot(s.edge.x - s.top.x, s.edge.z - s.top.z);
  float d = std::clamp(progress, 0.0F, 1.0F) * (a + b + c);
  auto lerp = [](WW::Point p, WW::Point q, float t, float y0, float y1) {
    return StairPose{p.x + (q.x - p.x) * t, p.z + (q.z - p.z) * t, y0 + (y1 - y0) * t};
  };
  if (d <= a) {
    return lerp(s.foot, s.ground, a > 0.0F ? d / a : 1.0F, 0.0F, 0.0F);
  }
  d -= a;
  if (d <= b) {
    return lerp(s.ground, s.edge, b > 0.0F ? d / b : 1.0F, 0.0F, WW::k_deck_height);
  }
  d -= b;
  return lerp(s.edge,
              s.top,
              c > 0.0F ? std::min(1.0F, d / c) : 1.0F,
              WW::k_deck_height,
              WW::k_deck_height);
}

auto yaw_towards(float dx, float dz) -> float {
  return yaw_degrees_from_direction(dx, dz);
}

auto approach_angle(float current, float target, float max_step) -> float {
  return turn_yaw_toward(current, target, max_step);
}

void publish_path(WallWalkerComponent& walker,
                  const std::vector<WallNode>& nodes,
                  float x,
                  float z,
                  int stair_node) {
  walker.path.clear();
  float const deck = WW::k_deck_height;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (std::hypot(nodes[i].x - x, nodes[i].z - z) > k_path_radius) {
      continue;
    }
    auto const a = nodes[i].lane();
    bool has_link = false;
    for (std::size_t j = 0; j < nodes.size(); ++j) {
      if (j == i || !linked(nodes[i], nodes[j])) {
        continue;
      }
      has_link = true;
      if (j < i) {
        continue;
      }
      auto const b = nodes[j].lane();
      walker.path.push_back({a.x, a.z, deck, b.x, b.z, deck});
    }
    if (!has_link) {
      walker.path.push_back({a.x, a.z, deck, a.x, a.z, deck});
    }
  }
  walker.exit_chain.clear();
  if (stair_node >= 0) {
    auto const s = stair_line(nodes[static_cast<std::size_t>(stair_node)]);
    walker.path.push_back({s.foot.x, s.foot.z, 0.0F, s.ground.x, s.ground.z, 0.0F});
    walker.path.push_back({s.ground.x, s.ground.z, 0.0F, s.edge.x, s.edge.z, deck});
    walker.path.push_back({s.edge.x, s.edge.z, deck, s.top.x, s.top.z, deck});
    if (walker.phase == Phase::Descending || walker.phase == Phase::Leaving) {
      walker.exit_chain = {{s.top.x, s.top.z, deck, s.edge.x, s.edge.z, deck},
                           {s.edge.x, s.edge.z, deck, s.ground.x, s.ground.z, 0.0F},
                           {s.ground.x, s.ground.z, 0.0F, s.foot.x, s.foot.z, 0.0F}};
    }
  }
  if (walker.phase == Phase::Boarding) {
    if (walker.tower_id != 0) {
      walker.path.push_back({walker.base_x,
                             walker.base_z,
                             0.0F,
                             walker.door_x,
                             walker.door_z,
                             walker.door_y});
    }
    walker.path.push_back({walker.door_x,
                           walker.door_z,
                           walker.door_y,
                           walker.lip_x,
                           walker.lip_z,
                           walker.door_y});
    walker.path.push_back({walker.lip_x,
                           walker.lip_z,
                           walker.door_y,
                           walker.crest_x,
                           walker.crest_z,
                           WW::k_crest_height});
    walker.path.push_back({walker.crest_x,
                           walker.crest_z,
                           WW::k_crest_height,
                           walker.landing_x,
                           walker.landing_z,
                           deck});
  }
}

void publish_leaving_path(WallWalkerComponent& walker,
                          const std::vector<WallNode>& nodes,
                          int stair,
                          const TransformComponent& transform) {
  if (stair < 0) {
    walker.path.clear();
    return;
  }
  auto const& node = nodes[static_cast<std::size_t>(stair)];
  publish_path(walker, nodes, node.x, node.z, stair);
  auto const foot = stair_line(node).foot;
  walker.path.push_back(
      {foot.x, foot.z, 0.0F, transform.position.x, transform.position.z, 0.0F});
}

void enter_wall(UnitComponent& unit, WallWalkerComponent& walker) {
  walker.saved_files_override = unit.formation_files_override;
  unit.formation_files_override = 1;
}

void leave_wall(Engine::Core::World& world, EntityID id, UnitComponent* unit) {
  if (auto const* walker = world.try_get<WallWalkerComponent>(id);
      walker != nullptr && unit != nullptr && walker->phase != Phase::Approaching) {
    unit->formation_files_override = walker->saved_files_override;
  }
  world.remove<WallWalkerComponent>(id);
}

auto along_wall_yaw(const std::vector<WallNode>& nodes, int here) -> float {
  if (here < 0) {
    return 0.0F;
  }
  auto const& node = nodes[static_cast<std::size_t>(here)];
  bool const runs_x = node.inner_z != 0;
  int ahead = 0;
  int behind = 0;
  auto const hops = hop_distances(nodes, here);
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (hops[i] <= 0) {
      continue;
    }
    float const along = runs_x ? nodes[i].x - node.x : nodes[i].z - node.z;
    (along > 0.0F ? ahead : behind) += 1;
  }
  float const sign = ahead >= behind ? 1.0F : -1.0F;
  return runs_x ? yaw_towards(sign, 0.0F) : yaw_towards(0.0F, sign);
}

auto settle_along_wall(const WallNode& node, float yaw) -> float {
  bool const runs_x = node.inner_z != 0;
  float const forward = runs_x ? 90.0F : 0.0F;
  float const backward = forward + 180.0F;
  return std::abs(signed_yaw_delta(forward, yaw)) <=
                 std::abs(signed_yaw_delta(backward, yaw))
             ? forward
             : backward;
}

struct TowerFrame {
  float fx{0.0F};
  float fz{1.0F};
  float x{0.0F};
  float z{0.0F};

  [[nodiscard]] auto ahead(float reach) const -> WW::Point {
    return {x + fx * reach, z + fz * reach};
  }
};

auto tower_frame(const TransformComponent& transform) -> TowerFrame {
  float const yaw = transform.rotation.y * k_pi / 180.0F;
  return {std::sin(yaw), std::cos(yaw), transform.position.x, transform.position.z};
}

auto bridge_is_down(const SiegeTowerComponent& tower) -> bool {
  return tower.state == SiegeTowerComponent::State::Docked && tower.ramp >= 1.0F &&
         tower.docked_wall_id != 0;
}

struct DockedTower {
  EntityID id{0};
  int owner_id{0};
  EntityID wall_id{0};
  int wall_owner{0};
  TowerFrame frame;
};

auto docked_towers(Engine::Core::World& world) -> std::vector<DockedTower> {
  std::vector<DockedTower> out;
  for (auto [id, tower, unit, transform] : world.view<const SiegeTowerComponent,
                                                      const UnitComponent,
                                                      const TransformComponent>()) {
    if (unit.health <= 0 || !bridge_is_down(tower) ||
        world.has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }
    auto const* wall_unit = world.try_get<UnitComponent>(tower.docked_wall_id);
    if (wall_unit == nullptr ||
        !is_live_wall(world, tower.docked_wall_id, *wall_unit)) {
      continue;
    }
    out.push_back({id,
                   unit.owner_id,
                   tower.docked_wall_id,
                   wall_unit->owner_id,
                   tower_frame(transform)});
  }
  return out;
}

void begin_tower_approach(Engine::Core::World& world,
                          EntityID id,
                          const DockedTower& tower,
                          float goal_x,
                          float goal_z,
                          bool has_goal) {
  auto* walker = world.emplace<WallWalkerComponent>(id);
  walker->phase = Phase::Approaching;
  walker->elevation = 0.0F;
  walker->wall_id = tower.wall_id;
  walker->tower_id = tower.id;
  walker->stair_wall_id = 0;
  walker->goal_x = goal_x;
  walker->goal_z = goal_z;
  walker->has_goal = has_goal;
  auto const rear = tower.frame.ahead(-k_tower_rear_reach);
  CommandService::move_unit(world, id, QVector3D(rear.x, 0.0F, rear.z));
}

void board_tower(Engine::Core::World& world,
                 EntityID id,
                 UnitComponent& unit,
                 TransformComponent& transform,
                 WallWalkerComponent& walker,
                 const TransformComponent& tower_transform,
                 const std::vector<WallNode>& nodes,
                 int landing) {
  auto const& wall = nodes[static_cast<std::size_t>(landing)];
  auto const frame = tower_frame(tower_transform);
  auto const crest = frame.ahead(k_dock_standoff);
  auto const lane = WW::lane_point(crest.x, crest.z, wall.inner_x, wall.inner_z);
  auto const door = frame.ahead(k_tower_door_depth);
  auto const lip = frame.ahead(k_tower_lip_depth);
  auto const base = frame.ahead(k_tower_ladder_depth);
  walker.phase = Phase::Boarding;
  walker.boarding_seconds = 0.0F;
  walker.wall_id = wall.id;
  walker.door_x = door.x;
  walker.door_z = door.z;
  walker.door_y = WW::k_crest_height - 0.04F;
  walker.lip_x = lip.x;
  walker.lip_z = lip.z;
  walker.crest_x = crest.x;
  walker.crest_z = crest.z;
  walker.landing_x = lane.x;
  walker.landing_z = lane.z;
  walker.base_x = base.x;
  walker.base_z = base.z;
  walker.elevation = WW::k_deck_height;
  if (auto* movement = world.try_get<MovementComponent>(id)) {
    movement->stop();
  }
  transform.position.x = lane.x;
  transform.position.z = lane.z;
  transform.rotation.y = along_wall_yaw(nodes, landing);
  enter_wall(unit, walker);
  publish_path(walker, nodes, lane.x, lane.z, -1);
}

auto is_wall_climber(Engine::Core::World& world,
                     EntityID id,
                     const UnitComponent& unit) -> bool {
  return Game::Units::is_troop_spawn(unit.spawn_type) &&
         !Game::Units::is_siege_engine_spawn(unit.spawn_type) &&
         !Game::Units::is_cavalry(unit.spawn_type) &&
         unit.spawn_type != Game::Units::SpawnType::Elephant &&
         unit.spawn_type != Game::Units::SpawnType::Builder &&
         !world.has<Engine::Core::BuildingComponent>(id);
}

auto allied(const Game::Session::AmbientServices& services, int a, int b) -> bool {
  return a == b || (services.owners != nullptr && services.owners->are_allies(a, b));
}

void start_climbs(Engine::Core::World& world) {
  auto const& services = Game::Session::services_for(world);
  std::unordered_map<int, WallIndex> walls;
  auto index_for = [&](int owner) -> const WallIndex& {
    auto found = walls.find(owner);
    if (found == walls.end()) {
      found = walls.emplace(owner, WallIndex(gather_walls(world, owner))).first;
    }
    return found->second;
  };
  std::vector<DockedTower> towers;
  bool towers_gathered = false;

  std::vector<EntityID> candidates;
  for (auto [id, unit, movement] :
       world.view<const UnitComponent, const MovementComponent>()) {
    if (unit.health <= 0 || !movement.get_has_requested_goal() ||
        world.has<WallWalkerComponent>(id)) {
      continue;
    }
    candidates.push_back(id);
  }
  for (EntityID const id : candidates) {
    auto* unit = world.try_get<UnitComponent>(id);
    auto* movement = world.try_get<MovementComponent>(id);
    auto* transform = world.try_get<TransformComponent>(id);
    if (unit == nullptr || movement == nullptr || transform == nullptr ||
        !is_wall_climber(world, id, *unit)) {
      continue;
    }
    float const gx = movement->get_requested_goal_x();
    float const gz = movement->get_requested_goal_z();
    auto const& index = index_for(unit->owner_id);
    int const goal = index.nodes().empty() ? -1 : index.wall_order_at(gx, gz);
    if (goal >= 0) {
      auto const& nodes = index.nodes();
      int const stair =
          best_stair(nodes, -1, transform->position.x, transform->position.z, goal);
      if (stair < 0) {
        continue;
      }
      auto const& stair_node = nodes[static_cast<std::size_t>(stair)];
      auto const foot = access_foot(stair_node);
      auto* walker = world.emplace<WallWalkerComponent>(id);
      walker->phase = Phase::Approaching;
      walker->elevation = 0.0F;
      walker->wall_id = stair_node.id;
      walker->stair_wall_id = stair_node.id;
      walker->goal_x = nodes[static_cast<std::size_t>(goal)].x;
      walker->goal_z = nodes[static_cast<std::size_t>(goal)].z;
      walker->has_goal = true;
      CommandService::move_unit(world, id, QVector3D(foot.x, 0.0F, foot.z));
      continue;
    }

    if (!towers_gathered) {
      towers = docked_towers(world);
      towers_gathered = true;
    }
    DockedTower const* best_tower = nullptr;
    float best_d = std::numeric_limits<float>::max();
    float goal_x = 0.0F;
    float goal_z = 0.0F;
    bool has_goal = false;
    for (auto const& tower : towers) {
      if (!allied(services, tower.owner_id, unit->owner_id) ||
          allied(services, tower.wall_owner, unit->owner_id)) {
        continue;
      }
      auto const& hostile = index_for(tower.wall_owner);
      int const on_wall = hostile.wall_order_at(gx, gz);
      bool const at_tower = std::hypot(gx - tower.frame.x, gz - tower.frame.z) < 2.6F;
      bool reachable = false;
      if (on_wall >= 0) {
        auto const hops =
            hop_distances(hostile.nodes(), index_of(hostile.nodes(), tower.wall_id));
        reachable = hops[static_cast<std::size_t>(on_wall)] >= 0;
      }
      if (!reachable && !at_tower) {
        continue;
      }
      float const d = std::hypot(transform->position.x - tower.frame.x,
                                 transform->position.z - tower.frame.z);
      if (d < best_d) {
        best_d = d;
        best_tower = &tower;
        has_goal = reachable;
        if (reachable) {
          goal_x = hostile.nodes()[static_cast<std::size_t>(on_wall)].x;
          goal_z = hostile.nodes()[static_cast<std::size_t>(on_wall)].z;
        }
      }
    }
    if (best_tower != nullptr) {
      begin_tower_approach(world, id, *best_tower, goal_x, goal_z, has_goal);
    }
  }
}

void call_up_escorts(Engine::Core::World& world,
                     const DockedTower& tower,
                     float radius,
                     bool from_rear) {
  auto const& services = Game::Session::services_for(world);
  auto const rear = tower.frame.ahead(-k_tower_rear_reach);
  float const cx = from_rear ? rear.x : tower.frame.x;
  float const cz = from_rear ? rear.z : tower.frame.z;
  std::vector<EntityID> called;
  for (auto [id, unit, transform, movement] : world.view<const UnitComponent,
                                                         const TransformComponent,
                                                         const MovementComponent>()) {
    if (unit.health <= 0 || !allied(services, unit.owner_id, tower.owner_id) ||
        allied(services, unit.owner_id, tower.wall_owner) ||
        world.has<WallWalkerComponent>(id) || !is_wall_climber(world, id, unit) ||
        world.has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }

    bool const moving = movement.get_has_target() || movement.get_has_requested_goal();
    if (moving) {
      bool const escorting =
          !from_rear && std::hypot(movement.get_goal_x() - tower.frame.x,
                                   movement.get_goal_y() - tower.frame.z) < 10.0F;
      if (!escorting) {
        continue;
      }
    }
    if (std::hypot(transform.position.x - cx, transform.position.z - cz) > radius) {
      continue;
    }
    called.push_back(id);
  }
  for (EntityID const id : called) {
    begin_tower_approach(world, id, tower, 0.0F, 0.0F, false);
  }
}

void man_threatened_walls(Engine::Core::World& world) {
  constexpr float k_threat_reach = 30.0F;
  constexpr float k_archer_reach = 40.0F;
  constexpr float k_outside = -0.6F;
  auto const& services = Game::Session::services_for(world);

  struct Troop {
    EntityID id{0};
    int owner{0};
    float x{0.0F};
    float z{0.0F};
  };
  std::vector<Troop> enemies_all;
  std::unordered_map<int, std::vector<Troop>> idle_archers;
  for (auto [id, unit, transform, movement] : world.view<const UnitComponent,
                                                         const TransformComponent,
                                                         const MovementComponent>()) {
    if (unit.health <= 0 || !Game::Units::is_troop_spawn(unit.spawn_type) ||
        world.has<Engine::Core::PendingRemovalComponent>(id)) {
      continue;
    }
    enemies_all.push_back(
        {id, unit.owner_id, transform.position.x, transform.position.z});
    if (unit.spawn_type == Game::Units::SpawnType::Archer &&
        world.has<Engine::Core::AIControlledComponent>(id) &&
        !world.has<WallWalkerComponent>(id) && !movement.get_has_target() &&
        !movement.get_has_requested_goal()) {
      idle_archers[unit.owner_id].push_back(
          {id, unit.owner_id, transform.position.x, transform.position.z});
    }
  }
  for (auto& [owner, archers] : idle_archers) {
    auto const nodes = gather_walls(world, owner);
    bool const reachable = std::any_of(
        nodes.begin(), nodes.end(), [](const WallNode& node) { return node.access(); });
    if (!reachable) {
      continue;
    }

    std::vector<std::pair<float, int>> threatened;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      auto const& node = nodes[i];
      float nearest = k_threat_reach;
      for (auto const& enemy : enemies_all) {
        if (allied(services, enemy.owner, owner)) {
          continue;
        }
        float const dx = enemy.x - node.x;
        float const dz = enemy.z - node.z;
        float const across = dx * static_cast<float>(node.inner_x) +
                             dz * static_cast<float>(node.inner_z);
        float const d = std::hypot(dx, dz);
        if (across < k_outside && d < nearest) {
          nearest = d;
        }
      }
      if (nearest < k_threat_reach) {
        threatened.emplace_back(nearest, static_cast<int>(i));
      }
    }
    if (threatened.empty()) {
      continue;
    }
    std::sort(threatened.begin(), threatened.end());
    std::vector<int> manned;
    for (auto const& archer : archers) {
      int pick = -1;
      float pick_d = k_archer_reach;
      for (auto const& [threat, index] : threatened) {
        (void)threat;
        auto const& node = nodes[static_cast<std::size_t>(index)];
        bool const taken = std::any_of(manned.begin(), manned.end(), [&](int other) {
          auto const& o = nodes[static_cast<std::size_t>(other)];
          return std::hypot(o.x - node.x, o.z - node.z) < 5.0F;
        });
        float const d = std::hypot(archer.x - node.x, archer.z - node.z);
        if (!taken && d < pick_d) {
          pick_d = d;
          pick = index;
        }
      }
      if (pick < 0) {
        continue;
      }
      manned.push_back(pick);
      auto const lane = nodes[static_cast<std::size_t>(pick)].lane();
      CommandService::move_unit(world, archer.id, QVector3D(lane.x, 0.0F, lane.z));
    }
  }
}

} // namespace

auto siege_tower_dock_approach(Engine::Core::World& world,
                               EntityID tower,
                               EntityID wall_id) -> std::optional<QVector3D> {
  constexpr float k_wall_search = 9.0F;
  constexpr float k_approach_margin = 0.3F;
  auto const* tower_unit = world.try_get<UnitComponent>(tower);
  auto const* tower_transform = world.try_get<TransformComponent>(tower);
  auto const* wall_unit = world.try_get<UnitComponent>(wall_id);
  auto const* wall_transform = world.try_get<TransformComponent>(wall_id);
  if (tower_unit == nullptr || tower_transform == nullptr || wall_unit == nullptr ||
      wall_transform == nullptr || wall_unit->owner_id == tower_unit->owner_id) {
    return std::nullopt;
  }
  auto const nodes = gather_walls(world, wall_unit->owner_id);
  std::optional<QVector3D> best;
  float best_d = std::numeric_limits<float>::max();
  for (auto const& node : nodes) {
    if ((node.inner_x == 0) == (node.inner_z == 0) ||
        std::hypot(node.x - wall_transform->position.x,
                   node.z - wall_transform->position.z) > k_wall_search) {
      continue;
    }
    for (auto const& other : nodes) {
      if (other.id == node.id || !linked(node, other) ||
          other.inner_x != node.inner_x || other.inner_z != node.inner_z) {
        continue;
      }
      float const ox = -static_cast<float>(node.inner_x);
      float const oz = -static_cast<float>(node.inner_z);
      float const reach = k_dock_standoff + k_approach_margin;
      QVector3D const spot((node.x + other.x) * 0.5F + ox * reach,
                           0.0F,
                           (node.z + other.z) * 0.5F + oz * reach);

      float const d = std::hypot(spot.x() - wall_transform->position.x,
                                 spot.z() - wall_transform->position.z) *
                          2.0F +
                      std::hypot(spot.x() - tower_transform->position.x,
                                 spot.z() - tower_transform->position.z) *
                          0.1F;
      if (d < best_d) {
        best_d = d;
        best = spot;
      }
    }
  }
  return best;
}

void SiegeTowerSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto const& services = Game::Session::services_for(*world);

  std::vector<EntityID> towers;
  for (auto [id, tower] : world->view<SiegeTowerComponent>()) {
    (void)tower;
    towers.push_back(id);
  }

  for (EntityID const id : towers) {
    auto* entity = world->get_entity(id);
    auto* tower = world->try_get<SiegeTowerComponent>(id);
    auto* unit = world->try_get<UnitComponent>(id);
    auto* transform = world->try_get<TransformComponent>(id);
    auto* movement = world->try_get<MovementComponent>(id);
    if (entity == nullptr || tower == nullptr || unit == nullptr ||
        transform == nullptr || unit->health <= 0) {
      continue;
    }

    if (tower->state != SiegeTowerComponent::State::Rolling) {
      auto const* wall_unit = world->try_get<UnitComponent>(tower->docked_wall_id);
      bool const wall_live = wall_unit != nullptr &&
                             is_live_wall(*world, tower->docked_wall_id, *wall_unit);
      float const off = std::hypot(transform->position.x - tower->dock_x,
                                   transform->position.z - tower->dock_z);
      if (!wall_live || (tower->state == SiegeTowerComponent::State::Approaching &&
                         off > k_dock_give_up_distance)) {
        tower->state = SiegeTowerComponent::State::Rolling;
        tower->docked_wall_id = 0;
        tower->ramp = 0.0F;
        continue;
      }
    }

    if (tower->state == SiegeTowerComponent::State::Docked) {
      float const dx = tower->dock_x - transform->position.x;
      float const dz = tower->dock_z - transform->position.z;
      float const off = std::hypot(dx, dz);
      float const step = std::min(off, k_dock_settle_speed * delta_time);
      if (off > 1.0e-4F) {
        transform->position.x += dx / off * step;
        transform->position.z += dz / off * step;
      }
      float const turned = approach_angle(transform->rotation.y,
                                          tower->dock_yaw,
                                          k_dock_turn_degrees_per_second * delta_time);
      transform->rotation.y = turned;
      bool const square = std::abs(signed_yaw_delta(tower->dock_yaw, turned)) < 0.5F &&
                          off - step < 0.02F;
      if (movement != nullptr && movement->get_has_target()) {
        movement->stop();
      }
      if (!square && tower->ramp <= 0.0F) {
        continue;
      }
      tower->ramp = std::min(
          1.0F, tower->ramp + delta_time / SiegeTowerComponent::k_ramp_drop_seconds);
      if (tower->ramp >= 1.0F) {
        auto const* wall_unit = world->try_get<UnitComponent>(tower->docked_wall_id);
        DockedTower const docked{id,
                                 unit->owner_id,
                                 tower->docked_wall_id,
                                 wall_unit->owner_id,
                                 tower_frame(*transform)};
        if (tower->garrison_aboard) {
          tower->garrison_aboard = false;
          call_up_escorts(*world, docked, k_escort_call_radius, false);
        } else {
          call_up_escorts(*world, docked, k_rear_board_radius, true);
        }
      }
      continue;
    }

    if (tower->state == SiegeTowerComponent::State::Approaching) {
      tower->approach_seconds += delta_time;
      float const off = std::hypot(transform->position.x - tower->dock_x,
                                   transform->position.z - tower->dock_z);
      bool const idle = movement == nullptr || !movement->get_has_target();
      if (off < 0.35F || (idle && off < 2.0F) ||
          (tower->approach_seconds > 14.0F && off < 3.0F)) {
        tower->state = SiegeTowerComponent::State::Docked;
        if (movement != nullptr) {
          movement->stop();
        }
      } else if (idle) {
        CommandService::move_unit(
            *world, id, QVector3D(tower->dock_x, 0.0F, tower->dock_z));
      }
      continue;
    }

    std::vector<std::pair<int, WallNode>> hostile;
    for (auto [wall_id, wall_unit, wall_transform] :
         world->view<const UnitComponent, const TransformComponent>()) {
      if (!is_live_wall(*world, wall_id, wall_unit) ||
          wall_unit.owner_id == unit->owner_id ||
          (services.owners != nullptr &&
           services.owners->are_allies(wall_unit.owner_id, unit->owner_id))) {
        continue;
      }
      auto const node = make_node(
          wall_id, wall_transform, world->try_get<WallSegmentComponent>(wall_id));
      if ((node.inner_x == 0) != (node.inner_z == 0)) {
        hostile.emplace_back(wall_unit.owner_id, node);
      }
    }
    WallNode best;
    float best_d = k_dock_reach;
    float best_crest_x = 0.0F;
    float best_crest_z = 0.0F;
    for (auto const& [owner, node] : hostile) {
      float const ox = -static_cast<float>(node.inner_x);
      float const oz = -static_cast<float>(node.inner_z);
      for (auto const& [other_owner, other] : hostile) {
        if (other_owner != owner || other.id == node.id || !linked(node, other) ||
            other.inner_x != node.inner_x || other.inner_z != node.inner_z) {
          continue;
        }
        float const cx = (node.x + other.x) * 0.5F;
        float const cz = (node.z + other.z) * 0.5F;
        float const rx = transform->position.x - cx;
        float const rz = transform->position.z - cz;
        if (rx * ox + rz * oz < 0.4F) {
          continue;
        }
        float const d = std::hypot(rx, rz);
        if (d < best_d) {
          best_d = d;
          best = node;
          best_crest_x = cx;
          best_crest_z = cz;
        }
      }
    }
    if (best.id == 0) {
      continue;
    }

    float const ox = -static_cast<float>(best.inner_x);
    float const oz = -static_cast<float>(best.inner_z);
    tower->state = SiegeTowerComponent::State::Approaching;
    tower->docked_wall_id = best.id;
    tower->dock_x = best_crest_x + ox * k_dock_standoff;
    tower->dock_z = best_crest_z + oz * k_dock_standoff;
    tower->dock_yaw = yaw_towards(-ox, -oz);
    tower->approach_seconds = 0.0F;
    CommandService::move_unit(
        *world, id, QVector3D(tower->dock_x, 0.0F, tower->dock_z));
  }
}

void WallWalkSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_garrison_timer += delta_time;
  if (m_garrison_timer >= k_garrison_check_seconds) {
    m_garrison_timer = 0.0F;
    man_threatened_walls(*world);
  }
  start_climbs(*world);

  std::unordered_map<int, std::vector<WallNode>> networks;
  std::vector<EntityID> walkers;
  for (auto [id, walker] : world->view<WallWalkerComponent>()) {
    (void)walker;
    walkers.push_back(id);
  }

  for (EntityID const id : walkers) {
    auto* walker = world->try_get<WallWalkerComponent>(id);
    auto* unit = world->try_get<UnitComponent>(id);
    auto* transform = world->try_get<TransformComponent>(id);
    auto* movement = world->try_get<MovementComponent>(id);
    if (walker == nullptr || unit == nullptr || transform == nullptr) {
      continue;
    }
    if (unit->health <= 0 || world->has<Engine::Core::PendingRemovalComponent>(id)) {
      leave_wall(*world, id, unit);
      continue;
    }

    auto const* anchor_unit = world->try_get<UnitComponent>(walker->wall_id);
    if (anchor_unit == nullptr ||
        !is_live_wall(*world, walker->wall_id, *anchor_unit)) {
      leave_wall(*world, id, unit);
      continue;
    }
    auto network = networks.find(anchor_unit->owner_id);
    if (network == networks.end()) {
      network = networks
                    .emplace(anchor_unit->owner_id,
                             gather_walls(*world, anchor_unit->owner_id))
                    .first;
    }
    auto const& nodes = network->second;
    int const stair = index_of(nodes, walker->stair_wall_id);

    if (walker->phase == Phase::Approaching && walker->tower_id != 0) {
      auto const* tower = world->try_get<SiegeTowerComponent>(walker->tower_id);
      auto const* tower_unit = world->try_get<UnitComponent>(walker->tower_id);
      auto const* tower_transform =
          world->try_get<TransformComponent>(walker->tower_id);
      int const landing = index_of(nodes, tower != nullptr ? tower->docked_wall_id : 0);
      if (tower == nullptr || tower_unit == nullptr || tower_transform == nullptr ||
          tower_unit->health <= 0 || !bridge_is_down(*tower) || landing < 0 ||
          movement == nullptr) {
        leave_wall(*world, id, unit);
        continue;
      }
      auto const rear = tower_frame(*tower_transform).ahead(-k_tower_rear_reach);
      bool const reordered = movement->get_has_requested_goal() &&
                             std::hypot(movement->get_requested_goal_x() - rear.x,
                                        movement->get_requested_goal_z() - rear.z) >
                                 k_approach_order_slack;
      if (reordered) {
        leave_wall(*world, id, unit);
        continue;
      }
      float const off =
          std::hypot(transform->position.x - rear.x, transform->position.z - rear.z);
      bool const idle = !movement->get_has_target();
      if (off > k_tower_arrival && !(idle && off < k_tower_arrival_idle)) {
        if (idle) {
          CommandService::move_unit(*world, id, QVector3D(rear.x, 0.0F, rear.z));
        }
        continue;
      }
      board_tower(
          *world, id, *unit, *transform, *walker, *tower_transform, nodes, landing);
      continue;
    }

    if (walker->phase == Phase::Approaching) {
      if (stair < 0 || movement == nullptr) {
        leave_wall(*world, id, unit);
        continue;
      }
      auto const line = stair_line(nodes[static_cast<std::size_t>(stair)]);
      bool const reordered = movement->get_has_requested_goal() &&
                             std::hypot(movement->get_requested_goal_x() - line.foot.x,
                                        movement->get_requested_goal_z() -
                                            line.foot.z) > k_approach_order_slack;
      if (reordered) {
        leave_wall(*world, id, unit);
        continue;
      }
      float const off = std::hypot(transform->position.x - line.foot.x,
                                   transform->position.z - line.foot.z);
      bool const idle = !movement->get_has_target();
      if (off > k_stair_arrival && !(idle && off < k_stair_arrival_idle)) {
        if (idle) {
          CommandService::move_unit(
              *world, id, QVector3D(line.foot.x, 0.0F, line.foot.z));
        }
        continue;
      }
      movement->stop();
      walker->phase = Phase::Climbing;
      walker->stair_progress = 0.0F;
      enter_wall(*unit, *walker);
    }

    float const deck_speed = std::max(unit->speed, 0.5F) * k_deck_speed_scale;

    if (walker->phase == Phase::Climbing || walker->phase == Phase::Descending) {
      if (stair < 0) {
        leave_wall(*world, id, unit);
        continue;
      }
      auto const line = stair_line(nodes[static_cast<std::size_t>(stair)]);
      float const length = std::max(stair_length(line), 0.1F);
      float const speed = nodes[static_cast<std::size_t>(stair)].by_ladder()
                              ? WW::k_ladder_climb_speed
                              : k_stair_speed;
      float const climb = speed * delta_time / length;
      bool const up = walker->phase == Phase::Climbing;
      walker->stair_progress =
          std::clamp(walker->stair_progress + (up ? climb : -climb), 0.0F, 1.0F);
      auto const pose = stair_pose(line, walker->stair_progress);
      float const dx = pose.x - transform->position.x;
      float const dz = pose.z - transform->position.z;
      transform->position.x = pose.x;
      transform->position.z = pose.z;
      if (std::hypot(dx, dz) > 1.0e-4F) {
        transform->rotation.y = yaw_towards(dx, dz);
      }
      walker->elevation = pose.y;
      if (movement != nullptr && movement->get_has_target()) {
        movement->stop();
      }
      publish_path(*walker, nodes, transform->position.x, transform->position.z, stair);
      if (up && walker->stair_progress >= 1.0F) {
        walker->phase = Phase::OnDeck;
        walker->wall_id = walker->stair_wall_id;
        walker->recent_stair_id = walker->stair_wall_id;
        walker->recent_stair_seconds = k_recent_stair_seconds;
        walker->stair_wall_id = 0;
        transform->rotation.y = along_wall_yaw(nodes, stair);
      } else if (!up && walker->stair_progress <= 0.0F) {

        walker->phase = Phase::Leaving;
        walker->elevation = 0.0F;
        walker->boarding_seconds = 0.0F;
        if (walker->has_goal) {
          CommandService::move_unit(
              *world, id, QVector3D(walker->goal_x, 0.0F, walker->goal_z));
        }
        walker->has_goal = false;
        publish_leaving_path(*walker, nodes, stair, *transform);
      }
      continue;
    }

    if (walker->phase == Phase::Leaving) {
      walker->boarding_seconds += delta_time;
      auto const foot = stair >= 0
                            ? stair_line(nodes[static_cast<std::size_t>(stair)]).foot
                            : WW::Point{transform->position.x, transform->position.z};
      float const away =
          std::hypot(transform->position.x - foot.x, transform->position.z - foot.z);
      float const leave_seconds = (board_seconds(*world, id) - k_board_seconds_slack) /
                                      k_board_seconds_per_man *
                                      k_leave_seconds_per_man +
                                  k_leave_seconds_slack;
      if (stair < 0 || walker->boarding_seconds > leave_seconds ||
          away > k_leave_distance) {
        leave_wall(*world, id, unit);
        continue;
      }
      publish_leaving_path(*walker, nodes, stair, *transform);
      continue;
    }

    walker->elevation = WW::k_deck_height;
    int here = index_of(nodes, walker->wall_id);
    if (here < 0) {
      here = nearest_node(nodes, transform->position.x, transform->position.z);
    }
    if (here < 0 || distance_to_node(nodes[static_cast<std::size_t>(here)],
                                     transform->position.x,
                                     transform->position.z) > 3.0F) {
      leave_wall(*world, id, unit);
      continue;
    }

    if (walker->phase == Phase::Boarding) {
      walker->boarding_seconds += delta_time;
      if (walker->boarding_seconds >= board_seconds(*world, id)) {
        walker->phase = Phase::OnDeck;
        walker->tower_id = 0;
      }
    }

    if (walker->phase == Phase::OnDeck && movement != nullptr &&
        movement->get_has_target()) {
      bool const ordered = movement->get_has_requested_goal();
      float const gx =
          ordered ? movement->get_requested_goal_x() : movement->get_goal_x();
      float const gz =
          ordered ? movement->get_requested_goal_z() : movement->get_goal_y();
      movement->stop();
      int const goal_node = nearest_node(nodes, gx, gz);
      bool const on_wall =
          goal_node >= 0 &&
          (is_wall_order(nodes[static_cast<std::size_t>(goal_node)], gx, gz) ||
           (!ordered &&
            distance_to_node(nodes[static_cast<std::size_t>(goal_node)], gx, gz) <
                1.5F));
      if (on_wall) {
        walker->has_goal = true;
        walker->goal_x = nodes[static_cast<std::size_t>(goal_node)].x;
        walker->goal_z = nodes[static_cast<std::size_t>(goal_node)].z;
        walker->stair_wall_id = 0;
      } else if (ordered) {
        int const exit = best_stair(nodes, here, gx, gz, -1);
        if (exit < 0) {
          leave_wall(*world, id, unit);
          walker = nullptr;
        } else {
          walker->has_goal = true;
          walker->goal_x = gx;
          walker->goal_z = gz;
          walker->stair_wall_id = nodes[static_cast<std::size_t>(exit)].id;
        }
      }
      if (walker == nullptr) {
        continue;
      }
    }

    int target = here;
    bool const leaving = walker->stair_wall_id != 0;
    if (walker->phase == Phase::OnDeck && walker->has_goal) {
      int const goal = leaving ? index_of(nodes, walker->stair_wall_id)
                               : nearest_node(nodes, walker->goal_x, walker->goal_z);
      if (goal < 0) {
        walker->has_goal = false;
        walker->stair_wall_id = 0;
      } else {
        target = next_hop(nodes, here, goal);
        if (goal == here) {
          target = here;
        }
      }
    }

    auto const lane = nodes[static_cast<std::size_t>(target)].lane();
    float const dx = lane.x - transform->position.x;
    float const dz = lane.z - transform->position.z;
    float const dist = std::hypot(dx, dz);
    if (target != here || dist > k_node_arrival) {
      float const step = std::min(dist, deck_speed * delta_time);
      if (dist > 1.0e-4F) {
        transform->position.x += dx / dist * step;
        transform->position.z += dz / dist * step;
        if (step > 1.0e-4F) {
          transform->rotation.y = yaw_towards(dx, dz);
        }
      }
      if (distance_to_node(nodes[static_cast<std::size_t>(target)],
                           transform->position.x,
                           transform->position.z) < 0.6F) {
        walker->wall_id = nodes[static_cast<std::size_t>(target)].id;
      }
    } else if (walker->phase == Phase::OnDeck && walker->has_goal) {
      transform->position.x = lane.x;
      transform->position.z = lane.z;
      if (leaving && nodes[static_cast<std::size_t>(here)].access() &&
          nodes[static_cast<std::size_t>(here)].id == walker->stair_wall_id) {
        walker->phase = Phase::Descending;
        walker->stair_progress = 1.0F;
      } else if (!leaving) {
        walker->has_goal = false;
      }
    }
    walker->watching = walker->phase == Phase::OnDeck && !walker->has_goal &&
                       target == here && dist <= k_node_arrival;
    if (walker->watching) {
      auto const& node = nodes[static_cast<std::size_t>(here)];
      float const facing = anchor_unit->owner_id == unit->owner_id ? -1.0F : 1.0F;
      walker->watch_yaw = yaw_towards(facing * static_cast<float>(node.inner_x),
                                      facing * static_cast<float>(node.inner_z));
    }
    if (walker->phase == Phase::OnDeck && !walker->has_goal) {
      float const settled = settle_along_wall(nodes[static_cast<std::size_t>(here)],
                                              transform->rotation.y);
      transform->rotation.y =
          approach_angle(transform->rotation.y, settled, 240.0F * delta_time);
    }

    walker->recent_stair_seconds =
        std::max(0.0F, walker->recent_stair_seconds - delta_time);
    int const recent = walker->recent_stair_seconds > 0.0F
                           ? index_of(nodes, walker->recent_stair_id)
                           : -1;
    publish_path(*walker,
                 nodes,
                 transform->position.x,
                 transform->position.z,
                 walker->phase == Phase::Descending
                     ? index_of(nodes, walker->stair_wall_id)
                     : recent);
  }
}

} // namespace Game::Systems
