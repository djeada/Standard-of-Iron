#include "unit_activity.h"

#include <array>
#include <optional>
#include <utility>

#include "builder_product_types.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"

namespace Game::Systems {

namespace {

constexpr float k_blocked_after_seconds = 1.2F;

struct KindName {
  ActivityKind kind;
  std::string_view id;
};

constexpr std::array k_kind_names = std::to_array<KindName>({
    {ActivityKind::Idle, "idle"},
    {ActivityKind::Move, "move"},
    {ActivityKind::Attack, "attack"},
    {ActivityKind::Patrol, "patrol"},
    {ActivityKind::Guard, "guard"},
    {ActivityKind::Hold, "hold"},
    {ActivityKind::Construct, "construct"},
    {ActivityKind::Repair, "repair"},
    {ActivityKind::Dismantle, "dismantle"},
    {ActivityKind::ChopWood, "chop_wood"},
    {ActivityKind::MineStone, "mine_stone"},
    {ActivityKind::MineIron, "mine_iron"},
    {ActivityKind::HarvestGrain, "harvest_grain"},
    {ActivityKind::SlaughterSheep, "slaughter_sheep"},
    {ActivityKind::AutoGather, "auto_gather"},
    {ActivityKind::Deliver, "deliver"},
    {ActivityKind::Heal, "heal"},
    {ActivityKind::Train, "train"},
    {ActivityKind::Blocked, "blocked"},
});

struct StateName {
  ActivityState state;
  std::string_view id;
};

constexpr std::array k_state_names = std::to_array<StateName>({
    {ActivityState::Active, "active"},
    {ActivityState::Queued, "queued"},
    {ActivityState::Unavailable, "unavailable"},
    {ActivityState::Interrupted, "interrupted"},
});

auto state_for_fault(Engine::Core::BuilderTaskFault fault) -> ActivityState {
  switch (fault) {
  case Engine::Core::BuilderTaskFault::Interrupted:
    return ActivityState::Interrupted;
  case Engine::Core::BuilderTaskFault::TargetLost:
  case Engine::Core::BuilderTaskFault::Unreachable:
    return ActivityState::Unavailable;
  case Engine::Core::BuilderTaskFault::None:
    break;
  }
  return ActivityState::Active;
}

auto builder_activity(const Engine::Core::Entity& entity,
                      const Engine::Core::BuilderProductionComponent& builder)
    -> std::optional<UnitActivity> {
  UnitActivity activity;
  activity.kind = activity_for_builder_product(builder.product_type);
  activity.queued_orders =
      static_cast<int>(builder.queued_construction_site_ids.size());

  const bool has_job = builder.has_construction_site || builder.has_task_target ||
                       builder.structure_task_entity_id != 0;

  if (builder.auto_gather && !has_job) {

    const auto* carry = entity.get_component<Engine::Core::ResourceCarryComponent>();
    if (carry != nullptr && !carry->empty()) {
      return std::nullopt;
    }

    activity.kind = ActivityKind::AutoGather;
    activity.state =
        builder.has_active_fault() ? ActivityState::Unavailable : ActivityState::Queued;
    return activity;
  }

  if (builder.has_active_fault()) {

    activity.state = state_for_fault(builder.fault);
    if (!has_job && activity.kind == ActivityKind::Construct &&
        builder.product_type.empty()) {
      activity.kind = ActivityKind::Blocked;
    }
    return activity;
  }

  if (builder.in_progress && builder.at_construction_site) {
    activity.state = ActivityState::Active;
    return activity;
  }

  if (has_job || !builder.queued_construction_site_ids.empty()) {
    const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
    if (movement != nullptr && movement->get_stuck_time() >= k_blocked_after_seconds) {
      activity.state = ActivityState::Unavailable;
      return activity;
    }
    activity.state = ActivityState::Queued;
    return activity;
  }

  return std::nullopt;
}

auto attack_state(const Engine::Core::Entity& entity) -> ActivityState {
  if (const auto* combat = entity.get_component<Engine::Core::CombatStateComponent>()) {
    if (combat->animation_state != Engine::Core::CombatAnimationState::Idle) {
      return ActivityState::Active;
    }
  }
  if (const auto* motion =
          entity.get_component<Engine::Core::MotionPresentationComponent>()) {
    return motion->attack_target_in_range ? ActivityState::Active
                                          : ActivityState::Queued;
  }
  return ActivityState::Active;
}

} // namespace

auto activity_for_builder_product(std::string_view product_type) -> ActivityKind {
  if (product_type == k_builder_product_cut_tree) {
    return ActivityKind::ChopWood;
  }
  if (product_type == k_builder_product_collect_stone) {
    return ActivityKind::MineStone;
  }
  if (product_type == k_builder_product_collect_iron_ore) {
    return ActivityKind::MineIron;
  }
  if (product_type == k_builder_product_harvest_grain) {
    return ActivityKind::HarvestGrain;
  }
  if (product_type == k_builder_product_slaughter_sheep) {
    return ActivityKind::SlaughterSheep;
  }
  if (product_type == k_builder_product_repair) {
    return ActivityKind::Repair;
  }
  if (product_type == k_builder_product_dismantle) {
    return ActivityKind::Dismantle;
  }
  return ActivityKind::Construct;
}

auto classify_unit_activity(const Engine::Core::Entity& entity) -> UnitActivity {
  if (const auto* builder =
          entity.get_component<Engine::Core::BuilderProductionComponent>()) {
    if (auto activity = builder_activity(entity, *builder); activity.has_value()) {
      return *activity;
    }
  }

  if (const auto* carry = entity.get_component<Engine::Core::ResourceCarryComponent>();
      carry != nullptr && !carry->empty()) {
    const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
    bool const walking = movement != nullptr && movement->get_has_target();
    return {ActivityKind::Deliver,
            walking ? ActivityState::Active : ActivityState::Queued,
            0};
  }

  if (const auto* delivery =
          entity.get_component<Engine::Core::CivilianDeliveryComponent>()) {
    if (delivery->target_barracks_id != 0) {
      return {ActivityKind::Deliver, ActivityState::Queued, 0};
    }
  }

  if (const auto* healer = entity.get_component<Engine::Core::HealerComponent>()) {
    if (healer->is_healing_active) {
      return {ActivityKind::Heal, ActivityState::Active, 0};
    }
  }

  if (entity.get_component<Engine::Core::AttackTargetComponent>() != nullptr) {
    return {ActivityKind::Attack, attack_state(entity), 0};
  }

  if (const auto* patrol = entity.get_component<Engine::Core::PatrolComponent>()) {
    if (patrol->patrolling) {
      return {ActivityKind::Patrol, ActivityState::Active, 0};
    }
  }

  if (const auto* production =
          entity.get_component<Engine::Core::ProductionComponent>()) {
    if (production->in_progress) {
      return {ActivityKind::Train,
              ActivityState::Active,
              static_cast<int>(production->production_queue.size())};
    }
  }

  if (const auto* guard = entity.get_component<Engine::Core::GuardModeComponent>()) {
    if (guard->active) {
      return {ActivityKind::Guard,
              guard->returning_to_guard_position ? ActivityState::Queued
                                                 : ActivityState::Active,
              0};
    }
  }

  if (const auto* hold = entity.get_component<Engine::Core::HoldModeComponent>()) {
    if (hold->active) {
      return {ActivityKind::Hold, ActivityState::Active, 0};
    }
  }

  if (const auto* movement = entity.get_component<Engine::Core::MovementComponent>()) {
    if (movement->get_has_target()) {
      if (movement->get_stuck_time() >= k_blocked_after_seconds) {
        return {ActivityKind::Blocked, ActivityState::Unavailable, 0};
      }
      return {ActivityKind::Move, ActivityState::Active, 0};
    }
  }

  return {};
}

auto classify_unit_activity(Engine::Core::World& world,
                            Engine::Core::EntityID entity_id) -> UnitActivity {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return {};
  }
  return classify_unit_activity(*entity);
}

auto activity_kind_id(ActivityKind kind) -> std::string_view {
  for (const auto& entry : k_kind_names) {
    if (entry.kind == kind) {
      return entry.id;
    }
  }
  return k_kind_names.front().id;
}

auto activity_state_id(ActivityState state) -> std::string_view {
  for (const auto& entry : k_state_names) {
    if (entry.state == state) {
      return entry.id;
    }
  }
  return k_state_names.front().id;
}

auto activity_kind_from_id(std::string_view id) -> ActivityKind {
  for (const auto& entry : k_kind_names) {
    if (entry.id == id) {
      return entry.kind;
    }
  }
  return ActivityKind::Idle;
}

auto activity_state_from_id(std::string_view id) -> ActivityState {
  for (const auto& entry : k_state_names) {
    if (entry.id == id) {
      return entry.state;
    }
  }
  return ActivityState::Active;
}

auto activity_is_noteworthy(const UnitActivity& activity) -> bool {
  if (activity.kind == ActivityKind::Idle) {
    return false;
  }

  return !(activity.kind == ActivityKind::Move &&
           activity.state == ActivityState::Active);
}

} // namespace Game::Systems
