#include "command_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "pathfinding.h"
#include "units/spawn_type.h"
#include <QDebug>
#include <QVector3D>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <qvectornd.h>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Game::Systems {

namespace {
constexpr float same_target_threshold_sq = 0.01F;

constexpr float pathfinding_request_cooldown = 1.0F;

constexpr float target_movement_threshold_sq = 4.0F;

constexpr float k_unit_radius_threshold = 0.5F;

constexpr float k_jitter_distance = 1.5F;

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
      Game::Units::TroopConfig::instance().getSelectionRingSize(
          unit_comp->spawn_type);

  return selection_ring_size * 0.5F;
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

  for (size_t i = 0; i < units.size(); ++i) {
    auto *e = world.get_entity(units[i]);
    if (e == nullptr) {
      continue;
    }

    auto *hold_mode = e->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {

      hold_mode->active = false;
      hold_mode->exit_cooldown = hold_mode->stand_up_duration;
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

      continue;
    }

    auto *transform = e->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    auto *mv = e->get_component<Engine::Core::MovementComponent>();
    if (mv == nullptr) {
      mv = e->add_component<Engine::Core::MovementComponent>();
    }
    if (mv == nullptr) {
      continue;
    }

    if (options.clear_attack_intent) {
      e->remove_component<Engine::Core::AttackTargetComponent>();
    }

    const float target_x = targets[i].x();
    const float target_z = targets[i].z();

    bool matched_pending = false;
    if (mv->path_pending) {
      std::lock_guard<std::mutex> const lock(s_pending_mutex);
      auto request_it = s_entity_to_request.find(units[i]);
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
      continue;
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
          continue;
        }
      }
    }

    if (!mv->path_pending) {
      bool const current_target_matches =
          mv->has_target && !mv->has_waypoints();
      if (current_target_matches) {
        float const dx = mv->target_x - target_x;
        float const dz = mv->target_y - target_z;
        if (dx * dx + dz * dz <= same_target_threshold_sq) {
          continue;
        }
      }

      if (!mv->path.empty()) {
        const auto &last_waypoint = mv->path.back();
        float const dx = last_waypoint.first - target_x;
        float const dz = last_waypoint.second - target_z;
        if (dx * dx + dz * dz <= same_target_threshold_sq) {
          continue;
        }
      }
    }

    if (s_pathfinder) {
      Point const start =
          world_to_grid(transform->position.x, transform->position.z);
      Point const end = world_to_grid(targets[i].x(), targets[i].z());

      if (start == end) {
        mv->target_x = target_x;
        mv->target_y = target_z;
        mv->has_target = true;
        mv->clear_path();
        mv->path_pending = false;
        mv->pending_request_id = 0;
        mv->vx = 0.0F;
        mv->vz = 0.0F;
        clear_pending_request(units[i]);
        continue;
      }

      int const dx = std::abs(end.x - start.x);
      int const dz = std::abs(end.y - start.y);
      bool use_direct_path = (dx + dz) <= CommandService::DIRECT_PATH_THRESHOLD;
      use_direct_path = false;

      if (use_direct_path) {
        mv->target_x = target_x;
        mv->target_y = target_z;
        mv->has_target = true;
        mv->clear_path();
        mv->path_pending = false;
        mv->pending_request_id = 0;
        mv->vx = 0.0F;
        mv->vz = 0.0F;
        clear_pending_request(units[i]);

        mv->time_since_last_path_request = 0.0F;
        mv->last_goal_x = target_x;
        mv->last_goal_y = target_z;
      } else {

        bool skip_new_request = false;
        {
          std::lock_guard<std::mutex> const lock(s_pending_mutex);
          auto existing_it = s_entity_to_request.find(units[i]);
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
          continue;
        }

        mv->clear_path();
        mv->has_target = false;
        mv->vx = 0.0F;
        mv->vz = 0.0F;
        mv->path_pending = true;

        std::uint64_t const request_id =
            s_next_request_id.fetch_add(1, std::memory_order_relaxed);
        mv->pending_request_id = request_id;

        float const unit_radius = 1.0F;

        {
          std::lock_guard<std::mutex> const lock(s_pending_mutex);
          PendingPathRequest pending;
          pending.entity_id = units[i];
          pending.target = targets[i];
          pending.options = options;
          pending.unit_radius = unit_radius;
          s_pending_requests[request_id] = std::move(pending);
          s_entity_to_request[units[i]] = request_id;
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
      clear_pending_request(units[i]);
    }
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
      hold_mode->active = false;
      hold_mode->exit_cooldown = hold_mode->stand_up_duration;
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

    members.push_back({units[i], entity, transform, movement, targets[i],
                       engaged, member_speed, spawn_type, 0.0F});
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

      if (!s_pathfinder->is_walkable(target_grid.x, target_grid.y)) {
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
  bool use_direct_path = (dx + dz) <= CommandService::DIRECT_PATH_THRESHOLD;
  use_direct_path = false;

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

  float const unit_radius = 1.0F;

  PendingPathRequest pending;
  pending.entity_id = leader.id;
  pending.target = leader_target;
  pending.options = options;
  pending.unit_radius = unit_radius;
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

  s_pathfinder->submit_path_request(request_id, start, end, unit_radius);
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

    const auto &path_points = result.path;

    const float skip_threshold_sq = CommandService::WAYPOINT_SKIP_THRESHOLD_SQ;
    const bool has_path = path_points.size() > 1;

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

      if (!movement_component->path_pending ||
          movement_component->pending_request_id != result.request_id) {
        movement_component->path_pending = false;
        movement_component->pending_request_id = 0;
        return;
      }

      movement_component->path_pending = false;
      movement_component->pending_request_id = 0;
      movement_component->clear_path();
      movement_component->goal_x = target.x();
      movement_component->goal_y = target.z();
      movement_component->vx = 0.0F;
      movement_component->vz = 0.0F;

      if (has_path) {
        movement_component->path.reserve(path_points.size() - 1);
        for (size_t idx = 1; idx < path_points.size(); ++idx) {
          QVector3D const world_pos = grid_to_world(path_points[idx]);
          movement_component->path.emplace_back(world_pos.x() + offset.x(),
                                                world_pos.z() + offset.z());
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
            request_info.unit_radius <= k_unit_radius_threshold
                ? !s_pathfinder->is_walkable(current_grid.x, current_grid.y)
                : !s_pathfinder->is_walkable_with_radius(
                      current_grid.x, current_grid.y, request_info.unit_radius);

        if (current_cell_invalid) {

          constexpr int k_nearest_point_search_radius = 5;
          Point const nearest = Pathfinding::find_nearest_walkable_point(
              current_grid, k_nearest_point_search_radius, *s_pathfinder,
              request_info.unit_radius);

          if (!(nearest == current_grid)) {

            QVector3D safe_pos = grid_to_world(nearest);

            thread_local std::random_device rd;
            thread_local std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(-k_jitter_distance,
                                                       k_jitter_distance);

            float const jitter_x = dist(gen);
            float const jitter_z = dist(gen);

            member_transform->position.x = safe_pos.x() + jitter_x;
            member_transform->position.z = safe_pos.z() + jitter_z;
          } else {

            thread_local std::random_device rd;
            thread_local std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(-k_jitter_distance,
                                                       k_jitter_distance);

            member_transform->position.x += dist(gen);
            member_transform->position.z += dist(gen);
          }

          movement_component->has_target = false;
          movement_component->vx = 0.0F;
          movement_component->vz = 0.0F;

          return;
        }

        if (are_all_surrounding_cells_invalid(current_grid, *s_pathfinder,
                                              request_info.unit_radius)) {

          thread_local std::random_device rd;
          thread_local std::mt19937 gen(rd());
          std::uniform_real_distribution<float> dist(-k_jitter_distance,
                                                     k_jitter_distance);

          float const jitter_x = dist(gen);
          float const jitter_z = dist(gen);

          member_transform->position.x += jitter_x;
          member_transform->position.z += jitter_z;

          movement_component->has_target = false;
          movement_component->vx = 0.0F;
          movement_component->vz = 0.0F;

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

      hold_mode->active = false;
      hold_mode->exit_cooldown = hold_mode->stand_up_duration;
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
    std::vector<Engine::Core::EntityID> const unit_ids = {unit_id};
    std::vector<QVector3D> const move_targets = {desired_pos};
    CommandService::move_units(world, unit_ids, move_targets, opts);

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
