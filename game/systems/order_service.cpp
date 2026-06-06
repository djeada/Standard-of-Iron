#include "order_service.h"

#include "../core/component.h"
#include "../core/entity.h"

namespace Game::Systems {

namespace {

auto should_record_player_move_intent(MoveOrderKind kind) -> bool {
  return kind == MoveOrderKind::PlayerMove || kind == MoveOrderKind::FormationMove;
}

auto should_clear_attack_target(MoveOrderKind kind) -> bool {
  return kind == MoveOrderKind::PlayerMove || kind == MoveOrderKind::FormationMove;
}

auto should_clear_auxiliary_orders(MoveOrderKind kind) -> bool {
  return kind == MoveOrderKind::PlayerMove || kind == MoveOrderKind::FormationMove;
}

auto should_exit_hold_mode(MoveOrderKind kind) -> bool {
  return kind != MoveOrderKind::RecoveryMove;
}

auto should_disable_guard_mode(MoveOrderKind kind) -> bool {
  return kind == MoveOrderKind::PlayerMove || kind == MoveOrderKind::FormationMove ||
         kind == MoveOrderKind::AttackChase || kind == MoveOrderKind::ScriptedMove;
}

auto should_touch_formation_mode(MoveOrderKind kind) -> bool {
  return kind != MoveOrderKind::RecoveryMove;
}

void set_player_move_intent(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  auto* intent = entity->get_component<Engine::Core::PlayerOrderIntentComponent>();
  if (intent == nullptr) {
    intent = entity->add_component<Engine::Core::PlayerOrderIntentComponent>();
  }
  if (intent == nullptr) {
    return;
  }

  intent->kind = Engine::Core::PlayerOrderIntentKind::ManualMove;
  intent->suppress_opportunistic_combat = true;
}

} // namespace

void OrderService::reset_movement(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    return;
  }
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  movement->stop();
  if (transform != nullptr) {
    movement->set_rest_position(transform->position.x, transform->position.z);
  } else {
    movement->set_rest_position(0.0F, 0.0F);
  }
}

void OrderService::clear_civilian_delivery(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Civilian) {
    return;
  }

  entity->remove_component<Engine::Core::CivilianDeliveryComponent>();
}

void OrderService::clear_patrol(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  auto* patrol = entity->get_component<Engine::Core::PatrolComponent>();
  if (patrol == nullptr) {
    return;
  }

  patrol->patrolling = false;
  patrol->waypoints.clear();
}

void OrderService::clear_attack_target(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }
  entity->remove_component<Engine::Core::AttackTargetComponent>();
}

void OrderService::clear_player_order_intent(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }
  entity->remove_component<Engine::Core::PlayerOrderIntentComponent>();
}

void OrderService::exit_hold_mode(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    hold_mode->begin_exit();
  }
}

void OrderService::set_guard_mode_active(Engine::Core::Entity* entity, bool active) {
  if (entity == nullptr) {
    return;
  }

  auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
  if (guard_mode == nullptr) {
    return;
  }

  guard_mode->active = active;
  if (!active) {
    guard_mode->returning_to_guard_position = false;
    guard_mode->guarded_entity_id = 0;
    guard_mode->has_guard_target = false;
    guard_mode->guard_position_x = 0.0F;
    guard_mode->guard_position_z = 0.0F;
  }
}

void OrderService::set_formation_mode_active(Engine::Core::Entity* entity,
                                             bool active) {
  if (entity == nullptr) {
    return;
  }

  auto* formation_mode = entity->get_component<Engine::Core::FormationModeComponent>();
  if (formation_mode == nullptr) {
    return;
  }
  formation_mode->active = active;
}

void OrderService::prepare_for_move(Engine::Core::Entity* entity,
                                    MoveOrderKind kind,
                                    bool preserve_formation_mode) {
  if (entity == nullptr) {
    return;
  }

  if (should_clear_auxiliary_orders(kind)) {
    clear_civilian_delivery(entity);
    clear_patrol(entity);
  }

  if (should_clear_attack_target(kind)) {
    clear_attack_target(entity);
  }

  if (should_exit_hold_mode(kind)) {
    exit_hold_mode(entity);
  }

  if (should_disable_guard_mode(kind)) {
    auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active &&
        !guard_mode->returning_to_guard_position) {
      set_guard_mode_active(entity, false);
    }
  }

  if (should_touch_formation_mode(kind) && !preserve_formation_mode) {
    set_formation_mode_active(entity, false);
  }

  if (should_record_player_move_intent(kind)) {
    set_player_move_intent(entity);
  } else if (kind != MoveOrderKind::RecoveryMove) {
    clear_player_order_intent(entity);
  }
}

void OrderService::prepare_for_attack(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  clear_player_order_intent(entity);
  clear_civilian_delivery(entity);
  clear_patrol(entity);
  exit_hold_mode(entity);
  set_guard_mode_active(entity, false);
  set_formation_mode_active(entity, false);
}

void OrderService::apply_stop(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }

  reset_movement(entity);
  clear_attack_target(entity);
  clear_player_order_intent(entity);
  clear_patrol(entity);
  exit_hold_mode(entity);
  set_formation_mode_active(entity, false);
}

auto move_order_kind_name(MoveOrderKind kind) -> const char* {
  switch (kind) {
  case MoveOrderKind::PlayerMove:
    return "player";
  case MoveOrderKind::FormationMove:
    return "formation";
  case MoveOrderKind::AttackChase:
    return "attack-chase";
  case MoveOrderKind::GuardReturn:
    return "guard-return";
  case MoveOrderKind::RecoveryMove:
    return "recovery";
  case MoveOrderKind::ScriptedMove:
    return "scripted";
  }
  return "unknown";
}

} // namespace Game::Systems
