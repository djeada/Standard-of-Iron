#include "rts_action_model.h"

#include <QString>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"

namespace {

enum class ActionId {
  Attack,
  Guard,
  Hold,
  Patrol,
  Heal,
  Stop,
  Deliver,
  Collect,
  Build,
  Formation,
  Run,
  Rally,
  Unknown
};

struct ActionStatus {
  int eligible_count = 0;
  int active_count = 0;
  bool enabled = false;
  bool active = false;
  bool mixed = false;
  bool placing = false;
  bool passive = false;
};

constexpr ActionId k_all_actions[] = {ActionId::Attack,
                                      ActionId::Guard,
                                      ActionId::Hold,
                                      ActionId::Patrol,
                                      ActionId::Heal,
                                      ActionId::Stop,
                                      ActionId::Deliver,
                                      ActionId::Collect,
                                      ActionId::Build,
                                      ActionId::Formation,
                                      ActionId::Run,
                                      ActionId::Rally};

auto action_to_string(ActionId action) -> QString {
  switch (action) {
  case ActionId::Attack:
    return QStringLiteral("attack");
  case ActionId::Guard:
    return QStringLiteral("guard");
  case ActionId::Hold:
    return QStringLiteral("hold");
  case ActionId::Patrol:
    return QStringLiteral("patrol");
  case ActionId::Heal:
    return QStringLiteral("heal");
  case ActionId::Stop:
    return QStringLiteral("stop");
  case ActionId::Deliver:
    return QStringLiteral("deliver");
  case ActionId::Collect:
    return QStringLiteral("collect");
  case ActionId::Build:
    return QStringLiteral("build");
  case ActionId::Formation:
    return QStringLiteral("formation");
  case ActionId::Run:
    return QStringLiteral("run");
  case ActionId::Rally:
    return QStringLiteral("rally");
  case ActionId::Unknown:
    break;
  }
  return {};
}

auto action_from_string(const QString& action_id) -> ActionId {
  if (action_id == QStringLiteral("attack")) {
    return ActionId::Attack;
  }
  if (action_id == QStringLiteral("guard")) {
    return ActionId::Guard;
  }
  if (action_id == QStringLiteral("hold")) {
    return ActionId::Hold;
  }
  if (action_id == QStringLiteral("patrol")) {
    return ActionId::Patrol;
  }
  if (action_id == QStringLiteral("heal")) {
    return ActionId::Heal;
  }
  if (action_id == QStringLiteral("stop")) {
    return ActionId::Stop;
  }
  if (action_id == QStringLiteral("deliver")) {
    return ActionId::Deliver;
  }
  if (action_id == QStringLiteral("collect")) {
    return ActionId::Collect;
  }
  if (action_id == QStringLiteral("build")) {
    return ActionId::Build;
  }
  if (action_id == QStringLiteral("formation")) {
    return ActionId::Formation;
  }
  if (action_id == QStringLiteral("run")) {
    return ActionId::Run;
  }
  if (action_id == QStringLiteral("rally")) {
    return ActionId::Rally;
  }
  return ActionId::Unknown;
}

auto selected_units(Engine::Core::World* world)
    -> const std::vector<Engine::Core::EntityID>* {
  if (world == nullptr) {
    return nullptr;
  }
  auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return nullptr;
  }
  return &selection_system->get_selected_units();
}

auto unit_component(const Engine::Core::Entity* entity)
    -> const Engine::Core::UnitComponent* {
  return entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
}

auto unit_is_eligible_for_action(const Engine::Core::Entity& entity,
                                 ActionId action) -> bool {
  const auto* unit = unit_component(&entity);
  switch (action) {
  case ActionId::Attack:
    return (unit != nullptr) && Game::Units::can_use_attack_mode(unit->spawn_type);
  case ActionId::Guard:
    return (unit != nullptr) && Game::Units::can_use_guard_mode(unit->spawn_type);
  case ActionId::Hold:
    return (unit != nullptr) && Game::Units::can_use_hold_mode(unit->spawn_type);
  case ActionId::Patrol:
    return (unit != nullptr) && Game::Units::can_use_patrol_mode(unit->spawn_type);
  case ActionId::Heal:
    return (unit != nullptr) && (unit->spawn_type == Game::Units::SpawnType::Healer);
  case ActionId::Stop:
    return (unit != nullptr) && (unit->spawn_type != Game::Units::SpawnType::Barracks);
  case ActionId::Deliver:
    return (unit != nullptr) && (unit->spawn_type == Game::Units::SpawnType::Civilian);
  case ActionId::Collect:
  case ActionId::Build:
    return (unit != nullptr) && (unit->spawn_type == Game::Units::SpawnType::Builder);
  case ActionId::Formation:
    return (unit != nullptr) && Game::Units::is_troop_spawn(unit->spawn_type);
  case ActionId::Run:
    return (unit != nullptr) && Game::Units::can_use_run_mode(unit->spawn_type);
  case ActionId::Rally:
    return entity.get_component<Engine::Core::CommanderComponent>() != nullptr;
  case ActionId::Unknown:
    break;
  }
  return false;
}

auto unit_is_active_for_action(const Engine::Core::Entity& entity,
                               ActionId action) -> bool {
  switch (action) {
  case ActionId::Attack:
    return entity.get_component<Engine::Core::AttackTargetComponent>() != nullptr;
  case ActionId::Guard: {
    const auto* guard = entity.get_component<Engine::Core::GuardModeComponent>();
    return (guard != nullptr) && guard->active;
  }
  case ActionId::Hold: {
    const auto* hold = entity.get_component<Engine::Core::HoldModeComponent>();
    return (hold != nullptr) && hold->active;
  }
  case ActionId::Patrol: {
    const auto* patrol = entity.get_component<Engine::Core::PatrolComponent>();
    return (patrol != nullptr) && patrol->patrolling;
  }
  case ActionId::Formation: {
    const auto* formation =
        entity.get_component<Engine::Core::FormationModeComponent>();
    return (formation != nullptr) && formation->active;
  }
  case ActionId::Run: {
    const auto* stamina = entity.get_component<Engine::Core::StaminaComponent>();
    return (stamina != nullptr) && stamina->run_requested;
  }
  case ActionId::Heal:
  case ActionId::Stop:
  case ActionId::Deliver:
  case ActionId::Collect:
  case ActionId::Build:
  case ActionId::Rally:
  case ActionId::Unknown:
    break;
  }
  return false;
}

auto get_status(const App::Core::ActionContext& context,
                ActionId action) -> ActionStatus {
  ActionStatus status;
  const auto* selected = selected_units(context.world);
  if (selected == nullptr) {
    return status;
  }

  for (const auto entity_id : *selected) {
    auto* entity = context.world->get_entity(entity_id);
    if ((entity == nullptr) || !unit_is_eligible_for_action(*entity, action)) {
      continue;
    }

    ++status.eligible_count;
    status.active_count += unit_is_active_for_action(*entity, action) ? 1 : 0;
  }

  switch (action) {
  case ActionId::Stop:
    status.enabled = status.eligible_count > 0;
    break;
  case ActionId::Formation:
    status.enabled = status.eligible_count > 1;
    break;
  case ActionId::Heal:
    status.passive = status.eligible_count > 0;
    status.enabled = false;
    break;
  default:
    status.enabled = status.eligible_count > 0;
    break;
  }

  status.active =
      (status.eligible_count > 0) && (status.active_count == status.eligible_count);
  status.mixed =
      (status.active_count > 0) && (status.active_count < status.eligible_count);

  if (action == ActionId::Attack) {
    status.placing = context.cursor_mode == CursorMode::Attack;
  } else if (action == ActionId::Guard) {
    status.placing = context.cursor_mode == CursorMode::Guard;
  } else if (action == ActionId::Patrol) {
    status.placing = (context.cursor_mode == CursorMode::Patrol) ||
                     context.has_patrol_first_waypoint;
  } else if (action == ActionId::Deliver) {
    status.placing = context.cursor_mode == CursorMode::Deliver;
  } else if (action == ActionId::Build) {
    status.placing =
        (context.cursor_mode == CursorMode::Build) ||
        (context.placing_construction &&
         !context.pending_builder_construction_type.isEmpty() &&
         (context.pending_builder_construction_type != QStringLiteral("collect")));
  } else if (action == ActionId::Collect) {
    status.placing =
        context.cursor_mode == CursorMode::Collect ||
        (context.placing_construction &&
         (context.pending_builder_construction_type == QStringLiteral("collect")));
  } else if (action == ActionId::Formation) {
    status.placing = context.placing_formation;
  } else if (action == ActionId::Rally) {
    status.placing = context.cursor_mode == CursorMode::PlaceCommanderRally;
  }

  return status;
}

auto to_variant_map(const ActionStatus& status) -> QVariantMap {
  QVariantMap result;
  result[QStringLiteral("enabled")] = status.enabled;
  result[QStringLiteral("active")] = status.active;
  result[QStringLiteral("mixed")] = status.mixed;
  result[QStringLiteral("placing")] = status.placing;
  result[QStringLiteral("passive")] = status.passive;
  result[QStringLiteral("eligibleCount")] = status.eligible_count;
  return result;
}

auto classify_toggle_state(const ActionStatus& status) -> QString {
  if (status.eligible_count <= 0 || status.active_count <= 0) {
    return QStringLiteral("none");
  }
  if (status.active_count >= status.eligible_count) {
    return QStringLiteral("all");
  }
  return QStringLiteral("mixed");
}

} // namespace

namespace App::Core {

auto get_action_states(const ActionContext& context) -> QVariantMap {
  QVariantMap result;
  for (const auto action : k_all_actions) {
    result[action_to_string(action)] = to_variant_map(get_status(context, action));
  }
  return result;
}

auto get_current_action_mode(const ActionContext& context) -> QString {
  const auto* selected = selected_units(context.world);
  if ((selected == nullptr) || selected->empty()) {
    return QStringLiteral("normal");
  }

  if (get_status(context, ActionId::Collect).placing) {
    return QStringLiteral("collect");
  }
  if (get_status(context, ActionId::Build).placing) {
    return QStringLiteral("build");
  }
  if (get_status(context, ActionId::Formation).placing) {
    return QStringLiteral("formation");
  }
  if (get_status(context, ActionId::Rally).placing) {
    return QStringLiteral("rally");
  }
  if (get_status(context, ActionId::Attack).placing) {
    return QStringLiteral("attack");
  }
  if (get_status(context, ActionId::Guard).placing) {
    return QStringLiteral("guard");
  }
  if (get_status(context, ActionId::Patrol).placing) {
    return QStringLiteral("patrol");
  }
  if (get_status(context, ActionId::Deliver).placing) {
    return QStringLiteral("deliver");
  }

  if (get_status(context, ActionId::Patrol).active) {
    return QStringLiteral("patrol");
  }
  if (get_status(context, ActionId::Attack).active) {
    return QStringLiteral("attack");
  }
  if (get_status(context, ActionId::Guard).active) {
    return QStringLiteral("guard");
  }

  return QStringLiteral("normal");
}

auto get_toggle_state(Engine::Core::World* world, const QString& action_id) -> QString {
  ActionContext context;
  context.world = world;
  return classify_toggle_state(get_status(context, action_from_string(action_id)));
}

auto get_mode_availability(Engine::Core::World* world) -> QVariantMap {
  ActionContext context;
  context.world = world;

  QVariantMap result;
  result[QStringLiteral("canAttack")] = get_status(context, ActionId::Attack).enabled;
  result[QStringLiteral("canGuard")] = get_status(context, ActionId::Guard).enabled;
  result[QStringLiteral("canHold")] = get_status(context, ActionId::Hold).enabled;
  result[QStringLiteral("canPatrol")] = get_status(context, ActionId::Patrol).enabled;
  result[QStringLiteral("canHeal")] =
      get_status(context, ActionId::Heal).eligible_count > 0;
  result[QStringLiteral("canBuild")] = get_status(context, ActionId::Build).enabled;
  result[QStringLiteral("canCollect")] = get_status(context, ActionId::Collect).enabled;
  result[QStringLiteral("canDeliver")] = get_status(context, ActionId::Deliver).enabled;
  result[QStringLiteral("canRally")] = get_status(context, ActionId::Rally).enabled;
  return result;
}

auto get_selection_command_mode(Engine::Core::World* world) -> QString {
  ActionContext context;
  context.world = world;
  return get_current_action_mode(context);
}

auto filter_selected_units_for_action(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selected,
    const QString& action_id) -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> filtered;
  auto const action = action_from_string(action_id);
  if ((world == nullptr) || (action == ActionId::Unknown)) {
    return filtered;
  }

  filtered.reserve(selected.size());
  for (const auto entity_id : selected) {
    auto* entity = world->get_entity(entity_id);
    if ((entity != nullptr) && unit_is_eligible_for_action(*entity, action)) {
      filtered.push_back(entity_id);
    }
  }
  return filtered;
}

auto action_id_for_cursor_mode(CursorMode mode) -> QString {
  switch (mode) {
  case CursorMode::Attack:
    return QStringLiteral("attack");
  case CursorMode::Guard:
    return QStringLiteral("guard");
  case CursorMode::Patrol:
    return QStringLiteral("patrol");
  case CursorMode::Build:
  case CursorMode::PlaceBuilding:
    return QStringLiteral("build");
  case CursorMode::Deliver:
    return QStringLiteral("deliver");
  case CursorMode::Heal:
    return QStringLiteral("heal");
  case CursorMode::PlaceCommanderRally:
    return QStringLiteral("rally");
  case CursorMode::PlaceBarracksRally:
    break;
  case CursorMode::Collect:
    return QStringLiteral("collect");
  case CursorMode::Normal:
    break;
  }
  return {};
}

} // namespace App::Core
