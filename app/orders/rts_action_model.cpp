#include "app/orders/rts_action_model.h"

#include <QString>

#include <cmath>
#include <string>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/combat_system/combat_types.h"
#include "game/systems/owner_registry.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"

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
  AutoGather,
  Build,
  Repair,
  Formation,
  Run,
  Rally,
  Gate,
  Aura,
  Unknown
};

struct ActionStatus {
  int eligible_count = 0;
  int active_count = 0;
  int ready_count = 0;
  bool enabled = false;
  bool active = false;
  bool mixed = false;
  bool placing = false;
  bool passive = false;

  QVariantMap detail;
};

constexpr ActionId k_all_actions[] = {ActionId::Attack,
                                      ActionId::Guard,
                                      ActionId::Hold,
                                      ActionId::Patrol,
                                      ActionId::Heal,
                                      ActionId::Stop,
                                      ActionId::Deliver,
                                      ActionId::Collect,
                                      ActionId::AutoGather,
                                      ActionId::Build,
                                      ActionId::Repair,
                                      ActionId::Formation,
                                      ActionId::Run,
                                      ActionId::Rally,
                                      ActionId::Gate,
                                      ActionId::Aura};

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
  case ActionId::AutoGather:
    return QStringLiteral("auto_gather");
  case ActionId::Build:
    return QStringLiteral("build");
  case ActionId::Repair:
    return QStringLiteral("repair");
  case ActionId::Formation:
    return QStringLiteral("formation");
  case ActionId::Run:
    return QStringLiteral("run");
  case ActionId::Rally:
    return QStringLiteral("rally");
  case ActionId::Gate:
    return QStringLiteral("gate");
  case ActionId::Aura:
    return QStringLiteral("aura");
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
  if (action_id == QStringLiteral("auto_gather")) {
    return ActionId::AutoGather;
  }
  if (action_id == QStringLiteral("build")) {
    return ActionId::Build;
  }
  if (action_id == QStringLiteral("repair")) {
    return ActionId::Repair;
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
  if (action_id == QStringLiteral("gate")) {
    return ActionId::Gate;
  }
  if (action_id == QStringLiteral("aura")) {
    return ActionId::Aura;
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
  case ActionId::AutoGather:
  case ActionId::Build:
  case ActionId::Repair:
    return (unit != nullptr) && (unit->spawn_type == Game::Units::SpawnType::Builder);
  case ActionId::Formation:
    return (unit != nullptr) && Game::Units::is_troop_spawn(unit->spawn_type);
  case ActionId::Run:
    return (unit != nullptr) && Game::Units::can_use_run_mode(unit->spawn_type);
  case ActionId::Rally:
  case ActionId::Aura:
    return entity.get_component<Engine::Core::CommanderComponent>() != nullptr;
  case ActionId::Gate:

    return entity.get_component<Engine::Core::GateComponent>() != nullptr &&
           unit != nullptr &&
           unit->owner_id ==
               Game::Systems::OwnerRegistry::instance().get_local_player_id();
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
  case ActionId::Aura: {
    const auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
    return commander != nullptr && commander->aura_ability_active;
  }
  case ActionId::Gate: {
    const auto* gate = entity.get_component<Engine::Core::GateComponent>();
    return gate != nullptr &&
           gate->manual_mode != Engine::Core::GateComponent::ManualMode::Automatic;
  }
  case ActionId::Repair: {
    const auto* builder =
        entity.get_component<Engine::Core::BuilderProductionComponent>();
    return (builder != nullptr) &&
           builder->product_type ==
               std::string(Game::Systems::k_builder_product_repair);
  }
  case ActionId::AutoGather: {
    const auto* builder =
        entity.get_component<Engine::Core::BuilderProductionComponent>();
    return (builder != nullptr) && builder->auto_gather;
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

auto percent_bonus(float multiplier) -> int {
  return static_cast<int>(std::lround((multiplier - 1.0F) * 100.0F));
}

auto guard_detail(const Engine::Core::Entity* sample) -> QVariantMap {
  const auto* guard = sample != nullptr
                          ? sample->get_component<Engine::Core::GuardModeComponent>()
                          : nullptr;
  QVariantMap detail;
  detail[QStringLiteral("radius")] =
      guard != nullptr ? guard->guard_radius
                       : Engine::Core::Defaults::k_guard_default_radius;
  detail[QStringLiteral("returnThreshold")] =
      Engine::Core::Defaults::k_guard_return_threshold;
  return detail;
}

auto hold_detail() -> QVariantMap {
  namespace Constants = Game::Systems::Combat::Constants;
  static_assert(Constants::k_damage_multiplier_archer_hold ==
                    Constants::k_damage_multiplier_spearman_hold,
                "the hold tooltip quotes one damage bonus for both eligible types");

  QVariantMap detail;
  detail[QStringLiteral("archerRangeBonusPercent")] =
      percent_bonus(Constants::k_range_multiplier_hold);
  detail[QStringLiteral("spearmanRangeBonusPercent")] =
      percent_bonus(Constants::k_range_multiplier_spearman_hold);
  detail[QStringLiteral("damageBonusPercent")] =
      percent_bonus(Constants::k_damage_multiplier_archer_hold);
  detail[QStringLiteral("healthBonusPercent")] =
      percent_bonus(Constants::k_health_multiplier_hold);
  return detail;
}

auto patrol_detail(const App::Core::ActionContext& context) -> QVariantMap {
  int stage = 0;
  if (context.has_patrol_first_waypoint) {
    stage = 2;
  } else if (context.cursor_mode == CursorMode::Patrol) {
    stage = 1;
  }

  QVariantMap detail;
  detail[QStringLiteral("waypointStage")] = stage;
  return detail;
}

auto aura_detail(const Engine::Core::Entity* sample) -> QVariantMap {
  const auto* commander =
      sample != nullptr ? sample->get_component<Engine::Core::CommanderComponent>()
                        : nullptr;
  QVariantMap detail;
  if (commander == nullptr) {
    return detail;
  }

  detail[QStringLiteral("radius")] = commander->aura_radius;
  detail[QStringLiteral("duration")] = commander->aura_ability_duration;
  detail[QStringLiteral("remaining")] = commander->aura_ability_remaining;
  detail[QStringLiteral("cooldown")] = commander->aura_ability_cooldown;
  detail[QStringLiteral("cooldownRemaining")] =
      commander->aura_ability_cooldown_remaining;
  detail[QStringLiteral("wounded")] = commander->wounded;
  if (!commander->bonus_summary.empty()) {
    detail[QStringLiteral("summary")] = Game::Util::tr_asset(
        Game::Util::k_commanders_context, commander->bonus_summary);
  }
  return detail;
}

auto action_detail(const App::Core::ActionContext& context,
                   ActionId action,
                   const Engine::Core::Entity* sample) -> QVariantMap {
  switch (action) {
  case ActionId::Guard:
    return guard_detail(sample);
  case ActionId::Hold:
    return hold_detail();
  case ActionId::Patrol:
    return patrol_detail(context);
  case ActionId::Aura:
    return aura_detail(sample);
  default:
    break;
  }
  return {};
}

auto get_status(const App::Core::ActionContext& context,
                ActionId action) -> ActionStatus {
  ActionStatus status;
  const auto* selected = selected_units(context.world);
  if (selected == nullptr) {
    status.detail = action_detail(context, action, nullptr);
    return status;
  }

  const Engine::Core::Entity* first_eligible = nullptr;
  for (const auto entity_id : *selected) {
    auto* entity = context.world->get_entity(entity_id);
    if ((entity == nullptr) || !unit_is_eligible_for_action(*entity, action)) {
      continue;
    }

    if (first_eligible == nullptr) {
      first_eligible = entity;
    }
    ++status.eligible_count;
    status.active_count += unit_is_active_for_action(*entity, action) ? 1 : 0;
    if (action == ActionId::Aura) {
      const auto* commander = entity->get_component<Engine::Core::CommanderComponent>();
      status.ready_count +=
          commander != nullptr && commander->can_activate_aura_ability() ? 1 : 0;
    }
  }

  status.detail = action_detail(context, action, first_eligible);

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
  case ActionId::Aura:
    status.enabled = status.ready_count > 0;
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
  } else if (action == ActionId::Repair) {
    status.placing = context.cursor_mode == CursorMode::Repair;
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

  result[QStringLiteral("activeCount")] = status.active_count;
  result[QStringLiteral("readyCount")] = status.ready_count;
  result[QStringLiteral("detail")] = status.detail;
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
  if (get_status(context, ActionId::Repair).placing) {
    return QStringLiteral("repair");
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
  result[QStringLiteral("canAutoGather")] =
      get_status(context, ActionId::AutoGather).enabled;
  result[QStringLiteral("canRepair")] = get_status(context, ActionId::Repair).enabled;
  result[QStringLiteral("canDeliver")] = get_status(context, ActionId::Deliver).enabled;
  result[QStringLiteral("canRally")] = get_status(context, ActionId::Rally).enabled;
  result[QStringLiteral("canGate")] = get_status(context, ActionId::Gate).enabled;
  result[QStringLiteral("canAura")] = get_status(context, ActionId::Aura).enabled;
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
  case CursorMode::Repair:
    return QStringLiteral("repair");
  case CursorMode::Normal:
    break;
  }
  return {};
}

} // namespace App::Core
