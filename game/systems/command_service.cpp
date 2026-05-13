#include "command_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "pathfinding.h"
#include "units/spawn_type.h"
#include <QDebug>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <qvectornd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Game::Systems {

namespace {
constexpr float same_target_threshold_sq = 0.01F;

constexpr float pathfinding_request_cooldown = 1.0F;

constexpr float target_movement_threshold_sq = 4.0F;

constexpr float k_unit_radius_threshold = 0.5F;
constexpr int k_recovery_search_radius = 16;

auto is_direct_path_walkable(const QVector3D &from, const QVector3D &to,
                             const Pathfinding &pathfinder,
                             float unit_radius) -> bool {
  auto const is_walkable_func = [&pathfinder, unit_radius](int x,
                                                           int y) -> bool {
    if (unit_radius <= k_unit_radius_threshold) {
      return pathfinder.is_walkable(x, y);
    }
    return pathfinder.is_walkable_with_radius(x, y, unit_radius);
  };

  Point const end_grid = CommandService::world_to_grid(to.x(), to.z());
  if (!is_walkable_func(end_grid.x, end_grid.y)) {
    return false;
  }

  QVector3D const direction = to - from;
  float const length = direction.length();
  if (length < 0.5F) {
    return true;
  }

  constexpr float sample_interval = 0.5F;
  int const num_samples = static_cast<int>(length / sample_interval);

  for (int i = 1; i <= num_samples; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(num_samples + 1);
    QVector3D const sample_pos = from + direction * t;
    Point const sample_grid =
        CommandService::world_to_grid(sample_pos.x(), sample_pos.z());
    if (!is_walkable_func(sample_grid.x, sample_grid.y)) {
      return false;
    }
  }

  return true;
}

auto are_all_surrounding_cells_invalid(const Point &position,
                                       const Pathfinding &pathfinder,
                                       float unit_radius) -> bool {

  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }

      int const check_x = position.x + dx;
      int const check_y = position.y + dy;

      if (unit_radius <= k_unit_radius_threshold) {
        if (pathfinder.is_walkable(check_x, check_y)) {
          return false;
        }
      } else {
        if (pathfinder.is_walkable_with_radius(check_x, check_y, unit_radius)) {
          return false;
        }
      }
    }
  }

  return true;
}

auto is_walkable_for_radius(const Pathfinding &pathfinder, int x, int y,
                            float unit_radius) -> bool {
  if (unit_radius <= k_unit_radius_threshold) {
    return pathfinder.is_walkable(x, y);
  }
  return pathfinder.is_walkable_with_radius(x, y, unit_radius);
}

auto find_recovery_cell(const Point &origin, const Pathfinding &pathfinder,
                        float unit_radius, Point &recovery_cell) -> bool {
  std::array<float, 4> const candidate_radii = {
      unit_radius, std::max(k_unit_radius_threshold, unit_radius * 0.85F),
      k_unit_radius_threshold, 0.0F};

  for (float const candidate_radius : candidate_radii) {
    for (int radius = 1; radius <= k_recovery_search_radius; ++radius) {
      bool found_candidate = false;
      float best_distance_sq = std::numeric_limits<float>::infinity();
      Point best_candidate{};

      for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          if (std::abs(dx) != radius && std::abs(dy) != radius) {
            continue;
          }

          int const check_x = origin.x + dx;
          int const check_y = origin.y + dy;
          if (!is_walkable_for_radius(pathfinder, check_x, check_y,
                                      candidate_radius)) {
            continue;
          }

          float const distance_sq = static_cast<float>(dx * dx + dy * dy);
          if (distance_sq < best_distance_sq) {
            best_distance_sq = distance_sq;
            best_candidate = {check_x, check_y};
            found_candidate = true;
          }
        }
      }

      if (found_candidate) {
        recovery_cell = best_candidate;
        return true;
      }
    }
  }

  return false;
}

void clear_pending_movement_state(
    Engine::Core::MovementComponent *movement_component) {
  if (movement_component == nullptr) {
    return;
  }

  movement_component->path_pending = false;
  movement_component->pending_request_id = 0;
  movement_component->clear_path();
  movement_component->vx = 0.0F;
  movement_component->vz = 0.0F;
}
} // namespace

std::unique_ptr<Pathfinding> CommandService::s_pathfinder = nullptr;
std::unordered_map<std::uint64_t, CommandService::PendingPathRequest>
    CommandService::s_pending_requests;
std::unordered_map<Engine::Core::EntityID, std::uint64_t>
    CommandService::s_entity_to_request;
std::mutex CommandService::s_pending_mutex;
std::atomic<std::uint64_t> CommandService::s_next_request_id{1};

void CommandService::initialize(int world_width, int world_height) {
  s_pathfinder = std::make_unique<Pathfinding>(world_width, world_height);
  {
    std::lock_guard<std::mutex> const lock(s_pending_mutex);
    s_pending_requests.clear();
    s_entity_to_request.clear();
  }
  s_next_request_id.store(1, std::memory_order_release);

  float const offset_x = -(world_width * 0.5F - 0.5F);
  float const offset_z = -(world_height * 0.5F - 0.5F);
  s_pathfinder->set_grid_offset(offset_x, offset_z);
}

auto CommandService::get_pathfinder() -> Pathfinding * {
  return s_pathfinder.get();
}
auto CommandService::world_to_grid(float world_x, float world_z) -> Point {
  if (s_pathfinder) {
    int const grid_x = static_cast<int>(
        std::round(world_x - s_pathfinder->get_grid_offset_x()));
    int const grid_z = static_cast<int>(
        std::round(world_z - s_pathfinder->get_grid_offset_z()));
    return {grid_x, grid_z};
  }

  return {static_cast<int>(std::round(world_x)),
          static_cast<int>(std::round(world_z))};
}

auto CommandService::grid_to_world(const Point &grid_pos) -> QVector3D {
  if (s_pathfinder) {
    return {static_cast<float>(grid_pos.x) + s_pathfinder->get_grid_offset_x(),
            0.0F,
            static_cast<float>(grid_pos.y) + s_pathfinder->get_grid_offset_z()};
  }
  return {static_cast<float>(grid_pos.x), 0.0F, static_cast<float>(grid_pos.y)};
}

auto CommandService::get_unit_radius(
    Engine::Core::World &world, Engine::Core::EntityID entity_id) -> float {
  auto *entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return 0.5F;
  }

  auto *unit_comp = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr) {
    return 0.5F;
  }

  float const selection_ring_size =
      Game::Units::TroopConfig::instance().get_selection_ring_size(
          unit_comp->spawn_type);

  return std::max(selection_ring_size, k_unit_radius_threshold);
}

auto CommandService::try_queue_local_recovery_move(
    Engine::Core::World &world, Engine::Core::EntityID entity_id,
    const QVector3D &current_position, const QVector3D &goal,
    Engine::Core::MovementComponent *movement) -> bool {
  auto *pathfinder = get_pathfinder();
  if (pathfinder == nullptr || movement == nullptr) {
    return false;
  }

  float const unit_radius = get_unit_radius(world, entity_id);
  Point const current_grid =
      world_to_grid(current_position.x(), current_position.z());

  Point recovery_cell{};
  if (!find_recovery_cell(current_grid, *pathfinder, unit_radius,
                          recovery_cell)) {

    constexpr int k_emergency_search_radius = 64;
    Point const nearest = Pathfinding::find_nearest_walkable_point(
        current_grid, k_emergency_search_radius, *pathfinder, 0.0F);
    if (!pathfinder->is_walkable(nearest.x, nearest.y)) {
      return false;
    }
    recovery_cell = nearest;
  }

  QVector3D const safe_pos = grid_to_world(recovery_cell);
  clear_pending_movement_state(movement);
  movement->target_x = safe_pos.x();
  movement->target_y = safe_pos.z();
  movement->goal_x = goal.x();
  movement->goal_y = goal.z();
  movement->has_target = true;
  movement->repath_cooldown = 0.0F;
  return true;
}

void CommandService::clear_pending_request(Engine::Core::EntityID entity_id) {
  std::lock_guard<std::mutex> const lock(s_pending_mutex);
  auto it = s_entity_to_request.find(entity_id);
  if (it == s_entity_to_request.end()) {
    return;
  }

  std::uint64_t const request_id = it->second;
  s_entity_to_request.erase(it);

  auto pending_it = s_pending_requests.find(request_id);
  if (pending_it == s_pending_requests.end()) {
    return;
  }

  auto members = pending_it->second.group_members;
  s_pending_requests.erase(pending_it);

  for (auto member_id : members) {
    auto member_entry = s_entity_to_request.find(member_id);
    if (member_entry != s_entity_to_request.end() &&
        member_entry->second == request_id) {
      s_entity_to_request.erase(member_entry);
    }
  }
}

void CommandService::move_unit(Engine::Core::World &world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D &target) {
  move_unit(world, unit_id, target, MoveOptions{});
}

void CommandService::move_unit(Engine::Core::World &world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D &target,
                               const MoveOptions &options) {
  auto *e = world.get_entity(unit_id);
  if (e == nullptr) {
    return;
  }

  auto *hold_mode = e->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    hold_mode->begin_exit();
  }

  auto *guard_mode = e->get_component<Engine::Core::GuardModeComponent>();
  if ((guard_mode != nullptr) && guard_mode->active &&
      !guard_mode->returning_to_guard_position) {
    guard_mode->active = false;
  }

  auto *formation_mode =
      e->get_component<Engine::Core::FormationModeComponent>();
  if ((formation_mode != nullptr) && formation_mode->active) {
    formation_mode->active = false;
  }

  auto *atk = e->get_component<Engine::Core::AttackComponent>();
  if ((atk != nullptr) && atk->in_melee_lock) {
    return;
  }

  auto *transform = e->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  auto *mv = e->get_component<Engine::Core::MovementComponent>();
  if (mv == nullptr) {
    mv = e->add_component<Engine::Core::MovementComponent>();
  }
  if (mv == nullptr) {
    return;
  }

  if (options.clear_attack_intent) {
    e->remove_component<Engine::Core::AttackTargetComponent>();
  }

  float const target_x = target.x();
  float const target_z = target.z();

  bool matched_pending = false;
  if (mv->path_pending) {
    std::lock_guard<std::mutex> const lock(s_pending_mutex);
    auto request_it = s_entity_to_request.find(unit_id);
    if (request_it != s_entity_to_request.end()) {
      auto pending_it = s_pending_requests.find(request_it->second);
      if (pending_it != s_pending_requests.end()) {
        float const pdx = pending_it->second.target.x() - target_x;
        float const pdz = pending_it->second.target.z() - target_z;
        if (pdx * pdx + pdz * pdz <= same_target_threshold_sq) {
          pending_it->second.options = options;
          matched_pending = true;
        }
      } else {
        s_entity_to_request.erase(request_it);
      }
    }
  }

  mv->goal_x = target_x;
  mv->goal_y = target_z;

  if (matched_pending) {
    return;
  }

  bool should_suppress_path_request = false;
  if (mv->time_since_last_path_request < pathfinding_request_cooldown) {
    float const last_goal_dx = mv->last_goal_x - target_x;
    float const last_goal_dz = mv->last_goal_y - target_z;
    float const goal_movement_sq =
        last_goal_dx * last_goal_dx + last_goal_dz * last_goal_dz;

    if (goal_movement_sq < target_movement_threshold_sq) {
      should_suppress_path_request = true;

      if (mv->has_target || mv->path_pending) {
        return;
      }
    }
  }

  if (!mv->path_pending) {
    bool const current_target_matches = mv->has_target && !mv->has_waypoints();
    if (current_target_matches) {
      float const dx = mv->target_x - target_x;
      float const dz = mv->target_y - target_z;
      if (dx * dx + dz * dz <= same_target_threshold_sq) {
        return;
      }
    }

    if (!mv->path.empty()) {
      const auto &last_waypoint = mv->path.back();
      float const dx = last_waypoint.first - target_x;
      float const dz = last_waypoint.second - target_z;
      if (dx * dx + dz * dz <= same_target_threshold_sq) {
        return;
      }
    }
  }

  if (s_pathfinder) {
    float const unit_radius = get_unit_radius(world, unit_id);
    Point const start =
        world_to_grid(transform->position.x, transform->position.z);
    Point const end = world_to_grid(target.x(), target.z());

    if (start == end) {
      mv->target_x = target_x;
      mv->target_y = target_z;
      mv->has_target = true;
      mv->clear_path();
      mv->path_pending = false;
      mv->pending_request_id = 0;
      mv->vx = 0.0F;
      mv->vz = 0.0F;
      clear_pending_request(unit_id);
      return;
    }

    int const dx = std::abs(end.x - start.x);
    int const dz = std::abs(end.y - start.y);
    QVector3D const current_pos(transform->position.x, 0.0F,
                                transform->position.z);
    bool const use_direct_path =
        ((dx + dz) <= CommandService::DIRECT_PATH_THRESHOLD) &&
        is_direct_path_walkable(current_pos, target, *s_pathfinder,
                                unit_radius);

    if (use_direct_path) {
      mv->target_x = target_x;
      mv->target_y = target_z;
      mv->has_target = true;
      mv->clear_path();
      mv->path_pending = false;
      mv->pending_request_id = 0;
      mv->vx = 0.0F;
      mv->vz = 0.0F;
      clear_pending_request(unit_id);

      mv->time_since_last_path_request = 0.0F;
      mv->last_goal_x = target_x;
      mv->last_goal_y = target_z;
    } else {
      bool skip_new_request = false;
      {
        std::lock_guard<std::mutex> const lock(s_pending_mutex);
        auto existing_it = s_entity_to_request.find(unit_id);
        if (existing_it != s_entity_to_request.end()) {
          auto pending_it = s_pending_requests.find(existing_it->second);
          if (pending_it != s_pending_requests.end()) {
            float const dx = pending_it->second.target.x() - target_x;
            float const dz = pending_it->second.target.z() - target_z;
            if (dx * dx + dz * dz <= same_target_threshold_sq) {
              pending_it->second.options = options;
              skip_new_request = true;
            } else {
              s_pending_requests.erase(pending_it);
              s_entity_to_request.erase(existing_it);
            }
          } else {
            s_entity_to_request.erase(existing_it);
          }
        }
      }

      if (skip_new_request) {
        return;
      }

      mv->clear_path();
      mv->has_target = options.allow_direct_fallback;
      if (options.allow_direct_fallback) {
        mv->target_x = target_x;
        mv->target_y = target_z;
      }
      mv->vx = 0.0F;
      mv->vz = 0.0F;
      mv->path_pending = true;

      std::uint64_t const request_id =
          s_next_request_id.fetch_add(1, std::memory_order_relaxed);
      mv->pending_request_id = request_id;

      {
        std::lock_guard<std::mutex> const lock(s_pending_mutex);
        PendingPathRequest pending;
        pending.entity_id = unit_id;
        pending.target = target;
        pending.options = options;
        pending.unit_radius = unit_radius;
        s_pending_requests[request_id] = std::move(pending);
        s_entity_to_request[unit_id] = request_id;
      }

      s_pathfinder->submit_path_request(request_id, start, end, unit_radius);

      mv->time_since_last_path_request = 0.0F;
      mv->last_goal_x = target_x;
      mv->last_goal_y = target_z;
    }
  } else {
    mv->target_x = target_x;
    mv->target_y = target_z;
    mv->has_target = true;
    mv->clear_path();
    mv->path_pending = false;
    mv->pending_request_id = 0;
    mv->vx = 0.0F;
    mv->vz = 0.0F;
    clear_pending_request(unit_id);
  }
}

void CommandService::move_units(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &units,
    const std::vector<QVector3D> &targets) {
  move_units(world, units, targets, MoveOptions{});
}

void CommandService::move_units(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &units,
    const std::vector<QVector3D> &targets, const MoveOptions &options) {
  if (units.size() != targets.size()) {
    return;
  }

  if (options.group_move && units.size() > 1) {
    move_group(world, units, targets, options);
    return;
  }

  for (std::size_t i = 0; i < units.size(); ++i) {
    move_unit(world, units[i], targets[i], options);
  }
}

void CommandService::move_units(Engine::Core::World &world,
                                const std::vector<MoveIntent> &intents) {
  move_units(world, intents, MoveOptions{});
}

void CommandService::move_units(Engine::Core::World &world,
                                const std::vector<MoveIntent> &intents,
                                const MoveOptions &options) {
  if (options.group_move && intents.size() > 1) {
    std::vector<Engine::Core::EntityID> unit_ids;
    std::vector<QVector3D> move_targets;
    unit_ids.reserve(intents.size());
    move_targets.reserve(intents.size());

    for (const auto &intent : intents) {
      unit_ids.push_back(intent.unit_id);
      move_targets.push_back(intent.target);
    }

    move_units(world, unit_ids, move_targets, options);
    return;
  }

  for (const auto &intent : intents) {
    move_unit(world, intent.unit_id, intent.target, options);
  }
}

void CommandService::move_group(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &units,
    const std::vector<QVector3D> &targets, const MoveOptions &options) {
  struct MemberInfo {
    Engine::Core::EntityID id;
    Engine::Core::Entity *entity;
    Engine::Core::TransformComponent *transform;
    Engine::Core::MovementComponent *movement;
    QVector3D target;
    bool is_engaged;
    float speed;
    Game::Units::SpawnType spawn_type;
    float unit_radius;
    float distance_to_target;
  };

  std::vector<MemberInfo> members;
  members.reserve(units.size());

  for (size_t i = 0; i < units.size(); ++i) {
    auto *entity = world.get_entity(units[i]);
    if (entity == nullptr) {
      continue;
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->begin_exit();
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active &&
        !guard_mode->returning_to_guard_position) {
      guard_mode->active = false;
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_mode->active = false;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    auto *movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement == nullptr) {
      movement = entity->add_component<Engine::Core::MovementComponent>();
    }
    if (movement == nullptr) {
      continue;
    }

    bool engaged =
        entity->get_component<Engine::Core::AttackTargetComponent>() != nullptr;

    if (options.clear_attack_intent) {
      entity->remove_component<Engine::Core::AttackTargetComponent>();
      engaged = false;
    }

    auto *unit_component = entity->get_component<Engine::Core::UnitComponent>();
    float const member_speed = (unit_component != nullptr)
                                   ? std::max(0.1F, unit_component->speed)
                                   : 1.0F;
    Game::Units::SpawnType const spawn_type =
        (unit_component != nullptr) ? unit_component->spawn_type
                                    : Game::Units::SpawnType::Archer;
    float const member_radius = get_unit_radius(world, units[i]);

    members.push_back({units[i], entity, transform, movement, targets[i],
                       engaged, member_speed, spawn_type, member_radius, 0.0F});
  }

  if (members.empty()) {
    return;
  }

  if (members.size() == 1) {
    std::vector<Engine::Core::EntityID> const single_unit = {members[0].id};
    std::vector<QVector3D> const single_target = {members[0].target};
    MoveOptions single_options = options;
    single_options.group_move = false;
    move_units(world, single_unit, single_target, single_options);
    return;
  }

  std::vector<MemberInfo> moving_members;
  std::vector<MemberInfo> engaged_members;

  for (const auto &member : members) {
    if (member.is_engaged) {
      engaged_members.push_back(member);
    } else {
      moving_members.push_back(member);
    }
  }

  if (moving_members.empty()) {
    return;
  }

  if (s_pathfinder) {
    bool any_target_invalid = false;
    for (const auto &member : moving_members) {
      Point const target_grid =
          world_to_grid(member.target.x(), member.target.z());

      if (target_grid.x < 0 || target_grid.y < 0) {
        any_target_invalid = true;
        break;
      }

      bool const target_is_walkable =
          member.unit_radius <= k_unit_radius_threshold
              ? s_pathfinder->is_walkable(target_grid.x, target_grid.y)
              : s_pathfinder->is_walkable_with_radius(
                    target_grid.x, target_grid.y, member.unit_radius);
      if (!target_is_walkable) {
        any_target_invalid = true;
        break;
      }
    }

    if (any_target_invalid) {
      return;
    }
  }

  members = moving_members;

  if (members.empty()) {
    return;
  }

  QVector3D target_centroid(0.0F, 0.0F, 0.0F);
  QVector3D position_centroid(0.0F, 0.0F, 0.0F);
  float speed_sum = 0.0F;
  for (auto &member : members) {
    target_centroid += member.target;
    position_centroid += QVector3D(member.transform->position.x, 0.0F,
                                   member.transform->position.z);
    speed_sum += member.speed;
  }

  target_centroid /= static_cast<float>(members.size());
  position_centroid /= static_cast<float>(members.size());

  float target_distance_sum = 0.0F;
  float max_target_distance = 0.0F;
  float centroid_distance_sum = 0.0F;
  for (auto &member : members) {
    QVector3D const current_pos(member.transform->position.x, 0.0F,
                                member.transform->position.z);
    float const to_target = (current_pos - member.target).length();
    float const to_centroid = (current_pos - position_centroid).length();

    member.distance_to_target = to_target;
    target_distance_sum += to_target;
    centroid_distance_sum += to_centroid;
    max_target_distance = std::max(max_target_distance, to_target);
  }

  float const avg_target_distance =
      members.empty()
          ? 0.0F
          : target_distance_sum / static_cast<float>(members.size());
  float const avg_scatter =
      members.empty()
          ? 0.0F
          : centroid_distance_sum / static_cast<float>(members.size());
  float const avg_speed =
      members.empty() ? 0.0F : speed_sum / static_cast<float>(members.size());

  float const near_threshold =
      std::clamp(avg_target_distance * 0.5F, 4.0F, 12.0F);
  if (max_target_distance <= near_threshold) {
    MoveOptions direct_options = options;
    direct_options.group_move = false;

    std::vector<Engine::Core::EntityID> direct_ids;
    std::vector<QVector3D> direct_targets;
    direct_ids.reserve(members.size());
    direct_targets.reserve(members.size());

    for (const auto &member : members) {
      direct_ids.push_back(member.id);
      direct_targets.push_back(member.target);
    }

    move_units(world, direct_ids, direct_targets, direct_options);
    return;
  }

  float const scatter_threshold = std::max(avg_scatter, 2.5F);

  std::vector<MemberInfo> regroup_members;
  std::vector<MemberInfo> direct_members;
  regroup_members.reserve(members.size());
  direct_members.reserve(members.size());

  for (const auto &member : members) {
    QVector3D const current_pos(member.transform->position.x, 0.0F,
                                member.transform->position.z);
    float const to_target = member.distance_to_target;
    float const to_centroid = (current_pos - position_centroid).length();
    bool const near_destination = to_target <= near_threshold;
    bool const far_from_group = to_centroid > scatter_threshold * 1.5F;
    bool const fast_unit =
        member.speed >= avg_speed + 0.5F ||
        member.spawn_type == Game::Units::SpawnType::MountedKnight;

    bool should_advance = near_destination;
    if (!should_advance && fast_unit && to_target <= near_threshold * 1.5F) {
      should_advance = true;
    }
    if (!should_advance && far_from_group &&
        to_target <= near_threshold * 2.0F) {
      should_advance = true;
    }

    if (should_advance) {
      direct_members.push_back(member);
    } else {
      regroup_members.push_back(member);
    }
  }

  if (!direct_members.empty()) {
    MoveOptions direct_options = options;
    direct_options.group_move = false;

    std::vector<Engine::Core::EntityID> direct_ids;
    std::vector<QVector3D> direct_targets;
    direct_ids.reserve(direct_members.size());
    direct_targets.reserve(direct_members.size());

    for (const auto &member : direct_members) {
      direct_ids.push_back(member.id);
      direct_targets.push_back(member.target);
    }

    move_units(world, direct_ids, direct_targets, direct_options);
  }

  if (regroup_members.size() <= 1) {
    if (!regroup_members.empty()) {
      MoveOptions direct_options = options;
      direct_options.group_move = false;
      std::vector<Engine::Core::EntityID> const single_ids = {
          regroup_members.front().id};
      std::vector<QVector3D> const single_targets = {
          regroup_members.front().target};
      move_units(world, single_ids, single_targets, direct_options);
    }
    return;
  }

  members = std::move(regroup_members);

  QVector3D average(0.0F, 0.0F, 0.0F);
  for (const auto &member : members) {
    average += member.target;
  }
  average /= static_cast<float>(members.size());

  std::size_t leader_index = 0;
  float best_dist_sq = std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < members.size(); ++i) {
    float const dist_sq = (members[i].target - average).lengthSquared();
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      leader_index = i;
    }
  }

  auto &leader = members[leader_index];
  QVector3D const leader_target = leader.target;
  float const shared_path_radius = [&members, &leader_target]() -> float {
    float max_member_radius = k_unit_radius_threshold;
    float max_offset = 0.0F;
    for (const auto &member : members) {
      max_member_radius = std::max(max_member_radius, member.unit_radius);
      QVector3D const offset = member.target - leader_target;
      max_offset = std::max(max_offset, offset.length());
    }
    return max_member_radius + max_offset;
  }();

  std::vector<MemberInfo *> units_needing_new_path;

  for (auto &member : members) {
    auto *mv = member.movement;

    mv->goal_x = member.target.x();
    mv->goal_y = member.target.z();

    clear_pending_request(member.id);
    mv->target_x = member.transform->position.x;
    mv->target_y = member.transform->position.z;
    mv->has_target = false;
    mv->vx = 0.0F;
    mv->vz = 0.0F;
    mv->clear_path();
    mv->path_pending = false;
    mv->pending_request_id = 0;
    units_needing_new_path.push_back(&member);
  }

  if (units_needing_new_path.empty()) {
    return;
  }

  if (!s_pathfinder) {
    for (auto *member : units_needing_new_path) {
      member->movement->target_x = member->target.x();
      member->movement->target_y = member->target.z();
      member->movement->has_target = true;
    }
    return;
  }

  Point const start =
      world_to_grid(leader.transform->position.x, leader.transform->position.z);
  Point const end = world_to_grid(leader_target.x(), leader_target.z());

  if (start == end) {
    for (auto *member : units_needing_new_path) {
      member->movement->target_x = member->target.x();
      member->movement->target_y = member->target.z();
      member->movement->has_target = true;
    }
    return;
  }

  int const dx = std::abs(end.x - start.x);
  int const dz = std::abs(end.y - start.y);
  QVector3D const leader_pos(leader.transform->position.x, 0.0F,
                             leader.transform->position.z);
  bool const use_direct_path =
      ((dx + dz) <= CommandService::DIRECT_PATH_THRESHOLD) &&
      is_direct_path_walkable(leader_pos, leader_target, *s_pathfinder,
                              shared_path_radius);

  if (use_direct_path) {
    for (auto *member : units_needing_new_path) {
      member->movement->target_x = member->target.x();
      member->movement->target_y = member->target.z();
      member->movement->has_target = true;

      member->movement->time_since_last_path_request = 0.0F;
      member->movement->last_goal_x = member->target.x();
      member->movement->last_goal_y = member->target.z();
    }
    return;
  }

  std::uint64_t const request_id =
      s_next_request_id.fetch_add(1, std::memory_order_relaxed);

  for (auto *member : units_needing_new_path) {
    member->movement->path_pending = true;
    member->movement->pending_request_id = request_id;

    member->movement->time_since_last_path_request = 0.0F;
    member->movement->last_goal_x = member->target.x();
    member->movement->last_goal_y = member->target.z();
  }

  PendingPathRequest pending;
  pending.entity_id = leader.id;
  pending.target = leader_target;
  pending.options = options;
  pending.unit_radius = shared_path_radius;
  pending.group_members.reserve(units_needing_new_path.size());
  pending.group_targets.reserve(units_needing_new_path.size());
  for (const auto *member : units_needing_new_path) {
    pending.group_members.push_back(member->id);
    pending.group_targets.push_back(member->target);
  }

  {
    std::lock_guard<std::mutex> const lock(s_pending_mutex);
    s_pending_requests[request_id] = std::move(pending);
    for (const auto *member : units_needing_new_path) {
      s_entity_to_request[member->id] = request_id;
    }
  }

  s_pathfinder->submit_path_request(request_id, start, end, shared_path_radius);
}

void CommandService::process_path_results(Engine::Core::World &world) {
  if (!s_pathfinder) {
    return;
  }

  auto results = s_pathfinder->fetch_completed_paths();
  if (results.empty()) {
    return;
  }

  for (auto &result : results) {
    PendingPathRequest request_info;
    bool found = false;

    {
      std::lock_guard<std::mutex> const lock(s_pending_mutex);
      auto pending_it = s_pending_requests.find(result.request_id);
      if (pending_it != s_pending_requests.end()) {
        request_info = pending_it->second;
        s_pending_requests.erase(pending_it);

        found = true;
      }
    }

    if (!found) {
      continue;
    }

    std::vector<Point> resolved_path = std::move(result.path);
    if (resolved_path.empty() && request_info.options.group_move &&
        s_pathfinder != nullptr) {
      auto *leader_entity = world.get_entity(request_info.entity_id);
      auto *leader_transform =
          leader_entity != nullptr
              ? leader_entity->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      if (leader_transform != nullptr) {
        float relaxed_radius = get_unit_radius(world, request_info.entity_id);
        for (auto member_id : request_info.group_members) {
          relaxed_radius =
              std::max(relaxed_radius, get_unit_radius(world, member_id));
        }

        Point const start = world_to_grid(leader_transform->position.x,
                                          leader_transform->position.z);
        Point const end =
            world_to_grid(request_info.target.x(), request_info.target.z());
        auto bridge_path = s_pathfinder->find_path(start, end, relaxed_radius);
        bool const path_uses_bridge = std::any_of(
            bridge_path.begin(), bridge_path.end(), [](const Point &point) {
              QVector3D const world_pos = grid_to_world(point);
              return Game::Map::TerrainService::instance().is_on_bridge(
                  world_pos.x(), world_pos.z());
            });
        if (path_uses_bridge) {
          resolved_path = std::move(bridge_path);
        }
      }
    }

    const auto &path_points = resolved_path;

    const float skip_threshold_sq = CommandService::WAYPOINT_SKIP_THRESHOLD_SQ;
    const bool has_path = path_points.size() > 1;

    auto remove_entry = [&](Engine::Core::EntityID id) {
      auto entry = s_entity_to_request.find(id);
      if (entry != s_entity_to_request.end() &&
          entry->second == result.request_id) {
        s_entity_to_request.erase(entry);
      }
    };

    {
      std::lock_guard<std::mutex> const lock(s_pending_mutex);
      remove_entry(request_info.entity_id);
      for (auto member_id : request_info.group_members) {
        remove_entry(member_id);
      }
    }

    if (resolved_path.empty() && request_info.options.group_move &&
        request_info.options.retry_individual_on_group_failure &&
        !request_info.group_members.empty()) {
      MoveOptions individual_options = request_info.options;
      individual_options.group_move = false;

      std::vector<Engine::Core::EntityID> retry_ids;
      std::vector<QVector3D> retry_targets;
      retry_ids.reserve(request_info.group_members.size() + 1);
      retry_targets.reserve(request_info.group_members.size() + 1);

      auto add_retry_target = [&](Engine::Core::EntityID member_id,
                                  const QVector3D &target) {
        if (std::find(retry_ids.begin(), retry_ids.end(), member_id) !=
            retry_ids.end()) {
          return;
        }

        auto *member_entity = world.get_entity(member_id);
        if (member_entity == nullptr) {
          return;
        }

        auto *movement_component =
            member_entity->get_component<Engine::Core::MovementComponent>();
        if (movement_component == nullptr) {
          return;
        }

        clear_pending_movement_state(movement_component);
        movement_component->has_target = false;
        retry_ids.push_back(member_id);
        retry_targets.push_back(target);
      };

      add_retry_target(request_info.entity_id, request_info.target);
      for (std::size_t idx = 0; idx < request_info.group_members.size();
           ++idx) {
        QVector3D const target = (idx < request_info.group_targets.size())
                                     ? request_info.group_targets[idx]
                                     : request_info.target;
        add_retry_target(request_info.group_members[idx], target);
      }

      if (!retry_ids.empty()) {
        move_units(world, retry_ids, retry_targets, individual_options);
      }
      continue;
    }

    auto apply_to_member = [&](Engine::Core::EntityID member_id,
                               const QVector3D &target,
                               const QVector3D &offset) {
      auto *member_entity = world.get_entity(member_id);
      if (member_entity == nullptr) {
        return;
      }

      auto *movement_component =
          member_entity->get_component<Engine::Core::MovementComponent>();
      if (movement_component == nullptr) {
        return;
      }

      auto *member_transform =
          member_entity->get_component<Engine::Core::TransformComponent>();
      if (member_transform == nullptr) {
        return;
      }

      float const member_unit_radius = get_unit_radius(world, member_id);

      if (!movement_component->path_pending ||
          movement_component->pending_request_id != result.request_id) {
        clear_pending_movement_state(movement_component);
        return;
      }

      clear_pending_movement_state(movement_component);
      movement_component->goal_x = target.x();
      movement_component->goal_y = target.z();

      if (has_path) {
        movement_component->path.reserve(path_points.size() - 1);
        for (size_t idx = 1; idx < path_points.size(); ++idx) {
          QVector3D const world_pos = grid_to_world(path_points[idx]);
          QVector3D waypoint = world_pos;
          if (auto const bridge_center = Game::Map::TerrainService::instance()
                                             .get_bridge_traversal_position(
                                                 world_pos.x(), world_pos.z());
              bridge_center.has_value()) {
            waypoint.setX(bridge_center->x());
            waypoint.setZ(bridge_center->z());
          } else {
            waypoint.setX(world_pos.x() + offset.x());
            waypoint.setZ(world_pos.z() + offset.z());
          }
          movement_component->path.emplace_back(waypoint.x(), waypoint.z());
        }

        while (movement_component->has_waypoints()) {
          const auto &wp = movement_component->current_waypoint();
          float const dx = wp.first - member_transform->position.x;
          float const dz = wp.second - member_transform->position.z;
          if (dx * dx + dz * dz <= skip_threshold_sq) {
            movement_component->advance_waypoint();
          } else {
            break;
          }
        }

        if (movement_component->has_waypoints()) {
          const auto &wp = movement_component->current_waypoint();
          movement_component->target_x = wp.first;
          movement_component->target_y = wp.second;
          movement_component->has_target = true;
          return;
        }
      }

      if (!has_path && s_pathfinder) {
        Point const current_grid = world_to_grid(member_transform->position.x,
                                                 member_transform->position.z);

        bool const current_cell_invalid =
            member_unit_radius <= k_unit_radius_threshold
                ? !s_pathfinder->is_walkable(current_grid.x, current_grid.y)
                : !s_pathfinder->is_walkable_with_radius(
                      current_grid.x, current_grid.y, member_unit_radius);

        if (current_cell_invalid) {
          QVector3D const current_position(member_transform->position.x, 0.0F,
                                           member_transform->position.z);
          if (try_queue_local_recovery_move(world, member_id, current_position,
                                            target, movement_component)) {
            return;
          }
          movement_component->has_target = false;
          return;
        }

        if (are_all_surrounding_cells_invalid(current_grid, *s_pathfinder,
                                              member_unit_radius)) {
          QVector3D const current_position(member_transform->position.x, 0.0F,
                                           member_transform->position.z);
          if (try_queue_local_recovery_move(world, member_id, current_position,
                                            target, movement_component)) {
            return;
          }
          movement_component->has_target = false;
          return;
        }
      }

      if (request_info.options.allow_direct_fallback) {
        movement_component->target_x = target.x();
        movement_component->target_y = target.z();
        movement_component->has_target = true;
      } else {
        movement_component->has_target = false;
      }
    };

    QVector3D leader_target = request_info.target;
    std::vector<Engine::Core::EntityID> processed;
    processed.reserve(request_info.group_members.size() + 1);

    auto add_member = [&](Engine::Core::EntityID id, const QVector3D &target) {
      if (std::find(processed.begin(), processed.end(), id) !=
          processed.end()) {
        return;
      }
      QVector3D const offset = target - leader_target;
      apply_to_member(id, target, offset);
      processed.push_back(id);
    };

    add_member(request_info.entity_id, leader_target);

    if (!request_info.group_members.empty()) {
      const std::size_t count = request_info.group_members.size();
      for (std::size_t idx = 0; idx < count; ++idx) {
        auto member_id = request_info.group_members[idx];
        QVector3D const target = (idx < request_info.group_targets.size())
                                     ? request_info.group_targets[idx]
                                     : leader_target;
        add_member(member_id, target);
      }
    }
  }
}

void CommandService::attack_target(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &units,
    Engine::Core::EntityID target_id, bool should_chase) {
  if (target_id == 0) {
    return;
  }
  for (auto unit_id : units) {
    auto *e = world.get_entity(unit_id);
    if (e == nullptr) {
      continue;
    }

    auto *hold_mode = e->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->begin_exit();
    }

    auto *guard_mode = e->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active) {
      guard_mode->active = false;
    }

    auto *formation_mode =
        e->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_mode->active = false;
    }

    auto *attack_target =
        e->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target == nullptr) {
      attack_target = e->add_component<Engine::Core::AttackTargetComponent>();
    }
    if (attack_target == nullptr) {
      continue;
    }

    attack_target->target_id = target_id;
    attack_target->should_chase = should_chase;

    if (!should_chase) {
      continue;
    }

    auto *target_ent = world.get_entity(target_id);
    if (target_ent == nullptr) {
      continue;
    }

    auto *t_trans =
        target_ent->get_component<Engine::Core::TransformComponent>();
    auto *att_trans = e->get_component<Engine::Core::TransformComponent>();
    if ((t_trans == nullptr) || (att_trans == nullptr)) {
      continue;
    }

    QVector3D const target_pos(t_trans->position.x, 0.0F, t_trans->position.z);
    QVector3D const attacker_pos(att_trans->position.x, 0.0F,
                                 att_trans->position.z);

    QVector3D desired_pos = target_pos;

    float range = 2.0F;
    bool is_ranged_unit = false;
    if (auto *atk = e->get_component<Engine::Core::AttackComponent>()) {
      range = std::max(0.1F, atk->range);
      if (atk->can_ranged && atk->range > atk->melee_range * 1.5F) {
        is_ranged_unit = true;
      }
    }

    QVector3D direction = target_pos - attacker_pos;
    float const distance = direction.length();
    if (distance > 0.001F) {
      direction /= distance;
      if (target_ent->has_component<Engine::Core::BuildingComponent>()) {
        float const scale_x = t_trans->scale.x;
        float const scale_z = t_trans->scale.z;
        float const target_radius = std::max(scale_x, scale_z) * 0.5F;
        float const desired_distance =
            target_radius + std::max(range - 0.2F, 0.2F);
        if (distance > desired_distance + 0.15F) {
          desired_pos = target_pos - direction * desired_distance;
        }
      } else {
        float desired_distance = std::max(range - 0.2F, 0.2F);
        if (is_ranged_unit) {
          desired_distance = range * 0.85F;
        }
        if (distance > desired_distance + 0.15F) {
          desired_pos = target_pos - direction * desired_distance;
        }
      }
    }

    CommandService::MoveOptions opts;
    opts.clear_attack_intent = false;
    opts.allow_direct_fallback = true;
    CommandService::move_unit(world, unit_id, desired_pos, opts);

    auto *mv = e->get_component<Engine::Core::MovementComponent>();
    if (mv == nullptr) {
      mv = e->add_component<Engine::Core::MovementComponent>();
    }
    if (mv != nullptr) {

      mv->target_x = desired_pos.x();
      mv->target_y = desired_pos.z();
      mv->goal_x = desired_pos.x();
      mv->goal_y = desired_pos.z();
      mv->has_target = true;
      mv->clear_path();
    }
  }
}

} // namespace Game::Systems
