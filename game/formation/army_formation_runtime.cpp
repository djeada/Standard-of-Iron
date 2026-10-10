#include <algorithm>
#include <cmath>
#include <numbers>

#include "../core/component_combat.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "army_formation_ambient.h"
#include "army_formation_cohesion.h"
#include "army_formation_manoeuvre.h"
#include "army_formation_march.h"
#include "army_formation_morph.h"
#include "army_formation_planner.h"
#include "army_formation_registry.h"
#include "army_formation_tuning.h"

namespace Game::Formation {

namespace {

auto formation_of(const Engine::Core::Entity& entity) -> const ArmyFormation* {
  const auto* membership =
      entity.get_component<Engine::Core::ArmyFormationMembershipComponent>();
  if (membership == nullptr || !membership->is_valid()) {
    return nullptr;
  }
  const auto* formation = ambient_formation_registry().find(membership->group_id);
  if (formation == nullptr || !formation->has_member(entity.get_id())) {
    return nullptr;
  }
  return formation;
}

void prune_dead_members(Engine::Core::World& world, ArmyFormationRegistry& registry) {
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr) {
      continue;
    }
    std::vector<EntityID> dead;
    for (auto const member : formation->members) {
      auto* entity = world.get_entity(member);
      if (entity == nullptr) {
        dead.push_back(member);
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        dead.push_back(member);
      }
    }
    for (auto const member : dead) {
      registry.remove_member(member);
    }
  }
}

void refresh_cohesion(Engine::Core::World& world, ArmyFormationRegistry& registry) {
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr) {
      continue;
    }
    Manoeuvre::update(world, *formation, Tuning::k_cohesion_interval_seconds);
    Cohesion::refresh_shape_state(world, *formation);
    Cohesion::collect_stragglers(
        world, *formation, Tuning::k_cohesion_interval_seconds);
  }
}

auto make_replan_request(const ArmyFormation& formation,
                         FormationGroupID id,
                         bool advancing_along_corridor) -> ArmyFormationRequest {
  ArmyFormationRequest request;
  request.members = formation.members;
  request.anchor = formation.anchor;
  request.facing = formation.facing;
  request.frontage = formation.requested_frontage;
  request.intent = formation.intent;
  request.doctrine = formation.doctrine;
  request.options = formation.options;
  request.spacing = formation.spacing;
  request.group_id = id;
  request.preserve_previous_slots = true;
  request.allow_anchor_shift = !advancing_along_corridor;
  return request;
}

auto plan_for_request(Engine::Core::World& world,
                      const ArmyFormation& formation,
                      const ArmyFormationRequest& request) -> ArmyFormationPlan {
  ArmyFormationPlan plan;
  bool reused_reference = false;
  if (ArmyFormationRuntime::reference_matches_members(formation)) {
    plan = ArmyFormationPlanner::place(
        ArmyFormationPlanner::layout_from_reference(formation), request);
    reused_reference = plan.keeps_shape();
  }
  if (!reused_reference) {
    plan = ArmyFormationPlanner::plan(world, request);
    if (plan.valid && plan.narrowed &&
        ArmyFormationRuntime::reference_matches_members(formation)) {
      ArmyFormationPlanner::fold_onto_reference(plan, formation.reference_slots);
    }
  }
  return plan;
}

void replan_groups(Engine::Core::World& world,
                   ArmyFormationRegistry& registry,
                   bool moving_only) {
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->needs_replan) {
      continue;
    }
    if (moving_only &&
        (!formation->has_destination || !formation->maintains_formation())) {
      continue;
    }
    static_cast<void>(ArmyFormationRuntime::replan(world, id));
  }
}

} // namespace

void ArmyFormationRuntime::refresh_shape_state(Engine::Core::World& world,
                                               ArmyFormation& formation) {
  Cohesion::refresh_shape_state(world, formation);
}

auto ArmyFormationRuntime::damage_taken_multiplier(const Engine::Core::Entity& entity)
    -> float {
  const auto* formation = formation_of(entity);
  return formation == nullptr ? 1.0F : Cohesion::damage_taken_multiplier(*formation);
}

auto ArmyFormationRuntime::give_ground_velocity(const Engine::Core::Entity& entity)
    -> QVector3D {
  const auto* formation = formation_of(entity);
  return formation == nullptr ? QVector3D()
                              : Manoeuvre::give_ground_velocity(*formation, entity);
}

auto ArmyFormationRuntime::holds_for_manoeuvre(const Engine::Core::Entity& entity)
    -> bool {
  const auto* formation = formation_of(entity);
  return formation != nullptr &&
         Manoeuvre::holds_for_manoeuvre(*formation, entity.get_id());
}

auto ArmyFormationRuntime::move_speed_multiplier(const Engine::Core::Entity& entity)
    -> float {
  const auto* formation = formation_of(entity);
  return formation == nullptr ? 1.0F
                              : Cohesion::move_speed_multiplier(*formation, entity);
}

void ArmyFormationRuntime::begin_move(Engine::Core::World& world,
                                      FormationGroupID id,
                                      const QVector3D& destination,
                                      float facing,
                                      std::optional<float> marching_facing,
                                      bool allow_morph) {
  auto* formation = ambient_formation_registry().find(id);
  if (formation == nullptr) {
    return;
  }

  formation->manoeuvre.clear();
  Manoeuvre::reapply(*formation);
  formation->destination = destination;
  formation->destination_facing = facing;
  formation->has_destination = true;
  formation->facing = facing;
  formation->advance_progress = 0.0F;
  formation->move_plan.clear();
  formation->morph.clear();

  if (formation->maintains_formation() && allow_morph &&
      Morph::start(world, *formation, marching_facing)) {
    formation->needs_replan = false;
    formation->moves_pending = true;
    refresh_shape_state(world, *formation);
    return;
  }

  if (!formation->maintains_formation() || allow_morph) {
    formation->anchor = destination;
    formation->needs_replan = false;
    refresh_shape_state(world, *formation);
    return;
  }

  March::begin(world, *formation, marching_facing);
  formation->needs_replan = true;
  static_cast<void>(replan(world, id));
  refresh_shape_state(world, *formation);
}

void ArmyFormationRuntime::advance_morphs(Engine::Core::World& world,
                                          float delta_time) {
  auto& registry = ArmyFormationRegistry::for_world(world);
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->morph.active) {
      continue;
    }
    Morph::advance(*formation, delta_time);
  }
}

void ArmyFormationRuntime::advance_maintained_groups(Engine::Core::World& world,
                                                     float delta_time) {
  auto& registry = ambient_formation_registry();
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !March::is_advancing(*formation)) {
      continue;
    }
    March::advance_group(world, *formation, delta_time);
  }
}

auto ArmyFormationRuntime::morph_target(const ArmyFormation& formation,
                                        EntityID entity) -> std::optional<QVector3D> {
  return Morph::target(formation, entity);
}

auto ArmyFormationRuntime::morph_pace(const ArmyFormation& formation,
                                      EntityID entity,
                                      const QVector3D& position,
                                      float full_speed) -> float {
  return Morph::pace(formation, entity, position, full_speed);
}

auto ArmyFormationRuntime::reference_matches_members(const ArmyFormation& formation)
    -> bool {
  if (formation.reference_slots.empty()) {
    return false;
  }
  std::vector<EntityID> occupants;
  occupants.reserve(formation.reference_slots.size());
  for (const auto& slot : formation.reference_slots) {
    if (slot.occupant != 0U) {
      occupants.push_back(slot.occupant);
    }
  }
  std::vector<EntityID> members = formation.members;
  std::sort(occupants.begin(), occupants.end());
  std::sort(members.begin(), members.end());
  return occupants == members;
}

void ArmyFormationRuntime::sync_membership_components(Engine::Core::World& world,
                                                      const ArmyFormation& formation) {
  for (const auto& slot : formation.slot_list) {
    if (slot.occupant == 0U) {
      continue;
    }
    auto* entity = world.get_entity(slot.occupant);
    if (entity == nullptr) {
      continue;
    }
    auto* membership = Engine::Core::get_or_add_component<
        Engine::Core::ArmyFormationMembershipComponent>(entity);
    if (membership == nullptr) {
      continue;
    }
    membership->group_id = formation.id;
    membership->slot_id = slot.id;
  }
}

void ArmyFormationRuntime::clear_membership_component(Engine::Core::World& world,
                                                      EntityID entity) {
  auto* membership =
      world.try_get<Engine::Core::ArmyFormationMembershipComponent>(entity);
  if (membership != nullptr) {
    membership->group_id = 0U;
    membership->slot_id = k_invalid_slot;
  }
}

void ArmyFormationRuntime::disband(Engine::Core::World& world, FormationGroupID id) {
  auto& registry = ArmyFormationRegistry::for_world(world);

  for (auto const member : registry.members_of(id)) {
    clear_membership_component(world, member);
  }
  registry.remove_group(id);
}

void ArmyFormationRuntime::detach(Engine::Core::World& world, EntityID entity) {
  ambient_formation_registry().remove_member(entity);
  clear_membership_component(world, entity);
}

auto ArmyFormationRuntime::replan(Engine::Core::World& world,
                                  FormationGroupID id) -> bool {
  auto& registry = ambient_formation_registry();
  auto* formation = registry.find(id);
  if (formation == nullptr || formation->members.empty()) {
    return false;
  }

  bool const advancing_along_corridor = formation->maintains_formation() &&
                                        formation->has_destination &&
                                        formation->move_plan.active;
  QVector3D const advancing_anchor = formation->anchor;
  auto const request = make_replan_request(*formation, id, advancing_along_corridor);
  auto const plan = plan_for_request(world, *formation, request);
  if (!plan.valid) {
    formation->needs_replan = false;
    refresh_shape_state(world, *formation);
    return false;
  }

  registry.apply_plan(id, plan);
  if (advancing_along_corridor) {
    if (auto* advanced = registry.find(id)) {
      advanced->anchor = advancing_anchor;
    }
  }
  auto* updated = registry.find(id);
  if (updated == nullptr) {
    return true;
  }
  sync_membership_components(world, *updated);
  if (updated->maintains_formation() && updated->has_destination) {
    updated->moves_pending = true;
  }
  return true;
}

void ArmyFormationRuntime::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto& registry = ambient_formation_registry();

  advance_morphs(*world, delta_time);
  prune_dead_members(*world, registry);

  m_advance_accumulator += delta_time;
  if (m_advance_accumulator >= Tuning::k_advance_interval_seconds) {
    float const elapsed = m_advance_accumulator;
    m_advance_accumulator = 0.0F;
    advance_maintained_groups(*world, elapsed);
    replan_groups(*world, registry, true);
  }

  m_cohesion_accumulator += delta_time;
  if (m_cohesion_accumulator >= Tuning::k_cohesion_interval_seconds) {
    m_cohesion_accumulator = 0.0F;
    refresh_cohesion(*world, registry);
  }

  m_replan_accumulator += delta_time;
  if (m_replan_accumulator < Tuning::k_replan_interval_seconds) {
    return;
  }
  m_replan_accumulator = 0.0F;
  replan_groups(*world, registry, false);
}

auto ArmyFormationRuntime::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent,
            TransformComponent,
            AttackTargetComponent,
            HoldModeComponent,
            GuardModeComponent>{},
      Writes<ArmyFormationMembershipComponent, MovementComponent, AttackComponent>{});
}

} // namespace Game::Formation
