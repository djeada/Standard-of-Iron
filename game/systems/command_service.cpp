#include "command_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../formation/army_formation_planner.h"
#include "../formation/army_formation_registry.h"
#include "../formation/army_formation_service.h"
#include "../game_config.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "combat_rules.h"
#include "formation_combat_geometry.h"
#include "movement_system.h"
#include "nav_grid.h"
#include "pathfinding.h"
#include "units/spawn_type.h"
#include "walkability.h"

namespace Game::Systems {

namespace {
constexpr int k_stranded_start_recovery_cells = 8;

auto resolve_walkable_target(const QVector3D& target) -> QVector3D {
  return NavGrid::snap_to_walkable_ground(target, 8);
}

auto slot_is_reachable(Engine::Core::World& world,
                       Engine::Core::EntityID member,
                       const QVector3D& destination) -> bool {
  auto* pathfinder = NavGrid::get_pathfinder();
  auto* entity = world.get_entity(member);
  auto const* transform =
      entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (pathfinder == nullptr || entity == nullptr || transform == nullptr) {
    return pathfinder == nullptr && entity != nullptr && transform != nullptr;
  }

  auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto const passability = movement == nullptr || movement->get_can_enter_forest()
                               ? Pathfinding::Passability::Light
                               : Pathfinding::Passability::Heavy;
  Point const target = NavGrid::world_to_grid(destination.x(), destination.z());
  Point const start = Pathfinding::find_nearest_walkable_point(
      NavGrid::world_to_grid(transform->position.x, transform->position.z),
      k_stranded_start_recovery_cells,
      *pathfinder,
      passability);
  return pathfinder->can_reach(start, target, passability);
}

auto rescued_slot_position(Engine::Core::World& world,
                           Engine::Core::EntityID member,
                           const QVector3D& ideal) -> QVector3D {
  auto* entity = world.get_entity(member);
  auto const* transform =
      entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  QVector3D const home =
      transform != nullptr
          ? QVector3D(transform->position.x, 0.0F, transform->position.z)
          : ideal;

  BodyProfile profile;
  if (entity != nullptr) {
    if (auto const* movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      profile.radius = movement->get_navigation_clearance();
      profile.passability = movement->get_can_enter_forest()
                                ? Pathfinding::Passability::Light
                                : Pathfinding::Passability::Heavy;
    }
  }

  constexpr float k_rescue_search_radius = 12.0F;
  auto const standable =
      Walkability::nearest_standable(ideal, profile, k_rescue_search_radius, home);
  QVector3D const anchor = standable.value_or(ideal);

  if (slot_is_reachable(world, member, anchor)) {
    return anchor;
  }

  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return home;
  }
  pathfinder->update_navigation_grid();
  auto const route =
      pathfinder->find_path(NavGrid::world_to_grid(home.x(), home.z()),
                            NavGrid::world_to_grid(anchor.x(), anchor.z()),
                            profile.passability,
                            profile.radius);
  if (route.empty()) {
    return home;
  }
  QVector3D const closest = NavGrid::grid_to_world(route.back());
  return Walkability::can_stand(closest, profile) ? closest : home;
}

} // namespace

auto CommandService::GroundMovePlan::matches_members(
    const std::vector<Engine::Core::EntityID>& units) const -> bool {
  if (member_slots.size() != units.size()) {
    return false;
  }
  for (std::size_t index = 0; index < units.size(); ++index) {
    if (member_slots[index].member != units[index]) {
      return false;
    }
  }
  return true;
}

auto CommandService::GroundMovePlan::fully_placeable_for(
    const std::vector<Engine::Core::EntityID>& units) const -> bool {
  return matches_members(units) &&
         std::none_of(member_slots.begin(), member_slots.end(), [](auto const& slot) {
           return slot.placement == SlotPlacement::Blocked;
         });
}

auto CommandService::GroundMovePlan::anyone_can_move() const -> bool {
  return std::any_of(member_slots.begin(), member_slots.end(), [](auto const& slot) {
    return slot.placement != SlotPlacement::Blocked;
  });
}

auto CommandService::GroundMovePlan::target_positions() const
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(member_slots.size());
  for (auto const& slot : member_slots) {
    positions.push_back(slot.position);
  }
  return positions;
}

auto CommandService::GroundMovePlan::facing_angles() const -> std::vector<float> {
  std::vector<float> angles;
  angles.reserve(member_slots.size());
  for (auto const& slot : member_slots) {
    angles.push_back(slot.facing_angle);
  }
  return angles;
}

auto CommandService::resolve_group_slots(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& units,
    const QVector3D& center,
    bool preserve_current_shape) -> std::vector<GroupSlot> {
  std::vector<GroupSlot> resolved_slots;
  if (units.empty()) {
    return resolved_slots;
  }
  float const formation_facing =
      Game::Formation::ArmyFormationService::auto_facing(world, units, center);

  if (!preserve_current_shape) {
    Game::Formation::ArmyFormationRequest request;
    request.members = units;
    request.anchor = resolve_walkable_target(center);
    request.facing = formation_facing;
    request.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;

    auto& registry = Game::Formation::ArmyFormationRegistry::instance();
    auto const group_id = registry.group_of(units.front());
    bool one_existing_group = group_id != Game::Formation::k_invalid_group;
    for (auto const member : units) {
      one_existing_group = one_existing_group && registry.group_of(member) == group_id;
    }
    if (one_existing_group) {
      request.group_id = group_id;
      request.preserve_previous_slots = true;
    }

    auto const result = Game::Formation::ArmyFormationService::preview(world, request);
    if (result.positions.size() != units.size() ||
        result.facing_angles.size() != units.size() ||
        result.stable_slot_ids.size() != units.size() ||
        result.slot_status.size() != units.size()) {
      return resolved_slots;
    }

    resolved_slots.reserve(units.size());
    for (std::size_t index = 0; index < units.size(); ++index) {
      GroupSlot slot;
      slot.member = units[index];
      slot.position = result.positions[index];
      slot.stable_slot_id = result.stable_slot_ids[index];
      slot.facing_angle = result.facing_angles[index];
      slot.placement = result.slot_status[index];
      if (slot.placement != SlotPlacement::Blocked &&
          !slot_is_reachable(world, slot.member, slot.position)) {
        slot.placement = SlotPlacement::Blocked;
      }
      if (slot.placement == SlotPlacement::Blocked) {

        slot.position = rescued_slot_position(world, slot.member, slot.position);
        if (slot_is_reachable(world, slot.member, slot.position)) {
          slot.placement = SlotPlacement::Adjusted;
        }
      }
      resolved_slots.push_back(slot);
    }
    return resolved_slots;
  }

  QVector3D current_center(0.0F, 0.0F, 0.0F);
  int positioned_count = 0;
  float max_radius = CommandService::k_unit_radius_threshold;
  float max_core_radius = CommandService::k_unit_radius_threshold;
  std::vector<Engine::Core::TransformComponent*> member_transforms(units.size(),
                                                                   nullptr);
  std::vector<QVector3D> member_positions(units.size());
  std::vector<std::size_t> canonical_order(units.size());
  for (std::size_t index = 0; index < units.size(); ++index) {
    canonical_order[index] = index;
    auto* entity = world.get_entity(units[index]);
    member_transforms[index] =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (member_transforms[index] != nullptr) {
      member_positions[index] = QVector3D(member_transforms[index]->position.x,
                                          0.0F,
                                          member_transforms[index]->position.z);
    }
    UnitRadii const radii = get_unit_radii(world, units[index]);
    max_radius = std::max(max_radius, radii.envelope);
    max_core_radius = std::max(max_core_radius, radii.core);
  }
  std::sort(canonical_order.begin(),
            canonical_order.end(),
            [&](std::size_t lhs, std::size_t rhs) { return units[lhs] < units[rhs]; });
  for (std::size_t const index : canonical_order) {
    if (member_transforms[index] == nullptr) {
      continue;
    }
    current_center += member_positions[index];
    ++positioned_count;
  }
  if (positioned_count > 0) {
    current_center /= static_cast<float>(positioned_count);
  } else {
    current_center = center - QVector3D(0.0F, 0.0F, 1.0F);
  }
  for (std::size_t index = 0; index < units.size(); ++index) {
    if (member_transforms[index] == nullptr) {
      member_positions[index] = current_center;
    }
  }

  float min_separation = max_radius * 2.0F;
  float current_min_separation = std::numeric_limits<float>::infinity();
  for (std::size_t lhs = 0; lhs < units.size(); ++lhs) {
    if (member_transforms[lhs] == nullptr) {
      continue;
    }
    for (std::size_t rhs = lhs + 1U; rhs < units.size(); ++rhs) {
      if (member_transforms[rhs] == nullptr) {
        continue;
      }
      current_min_separation =
          std::min(current_min_separation,
                   std::hypot(member_positions[lhs].x() - member_positions[rhs].x(),
                              member_positions[lhs].z() - member_positions[rhs].z()));
    }
  }
  if (std::isfinite(current_min_separation)) {
    min_separation = std::max(max_core_radius * 2.0F,
                              std::min(min_separation, current_min_separation));
  }

  Game::Formation::ArmyFormationLayout layout;
  layout.valid = true;

  layout.spacing = min_separation / 0.6F;
  layout.slot_list.reserve(units.size());
  for (std::size_t const index : canonical_order) {
    if (member_transforms[index] == nullptr) {
      continue;
    }
    Game::Formation::FormationSlot slot;
    slot.id = static_cast<int>(layout.slot_list.size());
    slot.occupant = units[index];
    slot.local_offset = member_positions[index] - current_center;
    layout.slot_list.push_back(slot);
  }

  Game::Formation::ArmyFormationRequest request;
  request.anchor = center;
  request.facing = 0.0F;
  request.resolve_terrain = true;
  auto const fitted = Game::Formation::ArmyFormationPlanner::place(layout, request);

  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->update_navigation_grid();
  }
  auto member_slot = [&](std::size_t member_index) -> GroupSlot {
    GroupSlot result;
    result.member = units[member_index];
    auto const* transform = member_transforms[member_index];
    QVector3D const member_position = member_positions[member_index];
    auto const* fitted_slot = fitted.slot_for(result.member);

    if (fitted_slot == nullptr || transform == nullptr) {
      result.position = member_position;
      result.placement = SlotPlacement::Blocked;
      return result;
    }

    result.position = fitted_slot->world_position;
    result.stable_slot_id = fitted_slot->id;
    result.facing_angle = formation_facing;
    result.placement = fitted_slot->status;
    if (result.placement == SlotPlacement::Blocked ||
        !slot_is_reachable(world, result.member, result.position)) {

      result.position = rescued_slot_position(world, result.member, result.position);
      result.placement = slot_is_reachable(world, result.member, result.position)
                             ? SlotPlacement::Adjusted
                             : SlotPlacement::Blocked;
      if (result.placement == SlotPlacement::Blocked) {
        result.position = member_position;
      }
      return result;
    }
    return result;
  };

  resolved_slots.reserve(units.size());
  for (std::size_t index = 0; index < units.size(); ++index) {
    resolved_slots.push_back(member_slot(index));
  }
  return resolved_slots;
}

auto CommandService::plan_ground_move(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const QVector3D& target,
                                      bool preserve_current_shape) -> GroundMovePlan {
  GroundMovePlan plan;
  if (units.empty()) {
    return plan;
  }

  plan.resolved_target = resolve_walkable_target(target);
  plan.member_slots =
      resolve_group_slots(world, units, plan.resolved_target, preserve_current_shape);
  plan.preserve_formation_mode = preserve_current_shape;
  if (units.size() == 1 && !plan.member_slots.empty()) {
    plan.resolved_target = plan.member_slots.front().position;
  }
  return plan;
}

void CommandService::issue_ground_move(Engine::Core::World& world,
                                       const std::vector<Engine::Core::EntityID>& units,
                                       const GroundMovePlan& plan) {

  if (units.empty() || !plan.matches_members(units) || !plan.anyone_can_move()) {
    return;
  }

  std::vector<MoveIntent> intents;
  intents.reserve(units.size());
  for (std::size_t index = 0; index < units.size(); ++index) {
    intents.push_back({.unit_id = units[index],
                       .target = plan.member_slots[index].position,
                       .facing_angle = plan.member_slots[index].facing_angle});
  }

  MoveOptions opts;
  opts.kind = MoveOrderKind::FormationMove;
  opts.preserve_formation_mode = plan.preserve_formation_mode;
  move_units(world, intents, opts);
}

auto CommandService::structure_work_position(const QVector3D& worker_position,
                                             const QVector3D& structure_position,
                                             const std::string& structure_key,
                                             float unit_radius) -> QVector3D {
  const auto size = BuildingCollisionRegistry::get_building_size(structure_key);
  float dir_x = worker_position.x() - structure_position.x();
  float dir_z = worker_position.z() - structure_position.z();
  const float len_sq = dir_x * dir_x + dir_z * dir_z;
  if (len_sq < 0.0001F) {
    dir_x = 1.0F;
    dir_z = 0.0F;
  } else {
    const float len = std::sqrt(len_sq);
    dir_x /= len;
    dir_z /= len;
  }

  const float clearance =
      BuildingCollisionRegistry::get_grid_padding() + unit_radius + 0.25F;
  const float half_width = size.width * 0.5F;
  const float half_depth = size.depth * 0.5F;
  const float abs_x = std::fabs(dir_x);
  const float abs_z = std::fabs(dir_z);
  const float sx = abs_x > 0.0001F ? (half_width + clearance) / abs_x
                                   : std::numeric_limits<float>::infinity();
  const float sz = abs_z > 0.0001F ? (half_depth + clearance) / abs_z
                                   : std::numeric_limits<float>::infinity();
  const float scale = std::min(sx, sz);
  const float fallback_scale = std::max(half_width, half_depth) + clearance;
  const float final_scale =
      std::isfinite(scale) && scale > 0.0F ? scale : fallback_scale;

  return NavGrid::snap_to_walkable_ground(
      QVector3D(structure_position.x() + dir_x * final_scale,
                0.0F,
                structure_position.z() + dir_z * final_scale));
}

auto CommandService::world_prop_work_position(Game::Map::TerrainService& terrain,
                                              const QVector3D& worker_position,
                                              std::uint64_t world_prop_id,
                                              float unit_radius) -> QVector3D {
  const auto& props = terrain.world_props();
  const auto found = std::find_if(
      props.begin(), props.end(), [world_prop_id](const Game::Map::WorldProp& prop) {
        return prop.id == world_prop_id;
      });
  if (found == props.end()) {
    return worker_position;
  }

  const QVector3D placed = terrain.world_prop_world_position(*found);
  const QVector3D prop_position(placed.x(), 0.0F, placed.z());

  QVector3D approach = worker_position - prop_position;
  approach.setY(0.0F);
  if (approach.lengthSquared() < 0.01F) {
    approach = QVector3D(1.0F, 0.0F, 0.0F);
  }
  approach.normalize();

  const float standoff =
      Game::Map::world_prop_ground_radius(found->type, found->scale) + unit_radius +
      0.25F;
  return NavGrid::snap_to_walkable_ground(prop_position + approach * standoff);
}

auto CommandService::get_unit_radii(Engine::Core::World& world,
                                    Engine::Core::EntityID entity_id) -> UnitRadii {
  UnitRadii radii;
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return radii;
  }

  auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr) {
    return radii;
  }

  auto const layout = FormationCombat::resolve_layout(*entity);
  radii.core = std::max(layout.body_radius, k_unit_radius_threshold);
  radii.envelope = radii.core;
  for (auto const& slot : layout.live_slots) {
    radii.envelope = std::max(
        radii.envelope, std::hypot(slot.local_x, slot.local_z) + layout.body_radius);
  }
  return radii;
}

auto CommandService::get_unit_radius(Engine::Core::World& world,
                                     Engine::Core::EntityID entity_id) -> float {
  return get_unit_radii(world, entity_id).envelope;
}

void CommandService::move_unit(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D& target) {
  MovementSystem::issue_move(world, unit_id, target);
}

void CommandService::move_unit(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D& target,
                               const MoveOptions& options) {
  MovementSystem::issue_move(world, unit_id, target, options);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const std::vector<QVector3D>& targets) {
  MovementSystem::issue_move_units(world, units, targets);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const std::vector<QVector3D>& targets,
                                const MoveOptions& options) {
  MovementSystem::issue_move_units(world, units, targets, options);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<MoveIntent>& intents) {
  MovementSystem::issue_move_units(world, intents);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<MoveIntent>& intents,
                                const MoveOptions& options) {
  MovementSystem::issue_move_units(world, intents, options);
}

void CommandService::attack_target(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& units,
                                   Engine::Core::EntityID target_id,
                                   bool should_chase) {
  if (target_id == 0) {
    return;
  }
  for (auto unit_id : units) {
    auto* e = world.get_entity(unit_id);
    if (e == nullptr) {
      continue;
    }

    auto* atk = e->get_component<Engine::Core::AttackComponent>();
    if (atk != nullptr && atk->in_melee_lock &&
        atk->melee_lock_target_id != target_id &&
        CombatRules::participates_in_rts_melee_lock(e)) {
      auto* locked_target = world.get_entity(atk->melee_lock_target_id);
      auto const* locked_unit =
          locked_target != nullptr
              ? locked_target->get_component<Engine::Core::UnitComponent>()
              : nullptr;
      bool const locked_opponent_alive =
          locked_unit != nullptr && locked_unit->health > 0 &&
          !locked_target->has_component<Engine::Core::PendingRemovalComponent>();

      bool const leaves_structure =
          locked_target != nullptr &&
          locked_target->has_component<Engine::Core::BuildingComponent>();

      if (locked_opponent_alive && !leaves_structure) {
        continue;
      }
      CombatRules::clear_rts_melee_lock(e);
    }

    OrderService::prepare_for_attack(e);

    auto* attack_target =
        Engine::Core::get_or_add_component<Engine::Core::AttackTargetComponent>(e);
    if (attack_target == nullptr) {
      continue;
    }

    if (auto* action = e->get_component<Engine::Core::RpgCommanderActionComponent>();
        action != nullptr && action->action_running && action->active_target_id != 0 &&
        action->active_target_id != target_id) {
      action->action_running = false;
      action->action_completed = true;
      action->action_active = false;
      action->weapon_trace_active = false;
    }

    attack_target->target_id = target_id;
    attack_target->should_chase = should_chase;
    attack_target->is_player_command = true;
  }
}

} // namespace Game::Systems
