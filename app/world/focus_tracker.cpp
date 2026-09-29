#include "app/world/focus_tracker.h"

#include <algorithm>
#include <utility>

#include "app/viewmodels/activity_view_model.h"
#include "app/world/unit_queries.h"
#include "app/world/visibility_coordinator.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/alliance_board.h"
#include "game/systems/owner_registry.h"
#include "game/systems/unit_activity.h"

namespace App::World {

auto FocusTracker::describe(const FocusInputs& inputs,
                            Engine::Core::EntityID id) -> App::Core::FocusTargetInfo {
  App::Core::FocusTargetInfo info;
  Engine::Core::World* world = inputs.world;
  if (world == nullptr || id == Engine::Core::NULL_ENTITY) {
    return info;
  }
  const auto* unit = world->try_get<Engine::Core::UnitComponent>(id);
  if (unit == nullptr || unit->health <= 0) {
    return info;
  }

  App::World::UnitDescription described;
  if (!App::World::describe_unit(world, id, described) || !described.alive) {
    return info;
  }
  QString name = described.name;
  if (described.is_building) {
    const QString pretty = App::Core::building_display_name(*unit);
    if (!pretty.isEmpty()) {
      name = pretty;
    }
  }

  const int local = inputs.local_owner_id;
  const auto* session = inputs.session;
  info.valid = true;
  info.id = id;
  info.name = name;
  info.nation = described.nation;
  (void)App::World::unit_type_key(world, id, info.type_key);
  info.owner_id = unit->owner_id;
  info.is_building = described.is_building;
  info.is_own = unit->owner_id == local;
  info.is_enemy =
      !info.is_own &&
      (session != nullptr ? session->owners().are_enemies(local, unit->owner_id)
                          : true);
  info.is_ally = !info.is_own && session != nullptr &&
                 session->owners().are_allies(local, unit->owner_id);
  if (described.is_building && session != nullptr && !inputs.spectator_mode) {
    const auto kind = info.is_enemy ? Game::Systems::AllyCallKind::Attack
                                    : Game::Systems::AllyCallKind::Defend;
    if (Game::Systems::check_ally_call(*world, session->owners(), local, id, kind) ==
        Game::Systems::AllyCallProblem::None) {
      info.ally_call = QLatin1String(Game::Systems::ally_call_kind_key(kind));
    }
  }
  info.health = described.health;
  info.max_health = described.max_health;
  info.soldiers = described.soldiers;
  info.max_soldiers = described.max_soldiers;
  info.health_ratio =
      described.max_health > 0
          ? static_cast<double>(std::clamp(described.health, 0, described.max_health)) /
                static_cast<double>(described.max_health)
          : 0.0;
  const auto activity = App::World::unit_activity(world, id);
  info.activity =
      QString::fromUtf8(Game::Systems::activity_kind_id(activity.kind).data());
  info.activity_state =
      QString::fromUtf8(Game::Systems::activity_state_id(activity.state).data());

  std::vector<Engine::Core::EntityID> selection;
  const auto& selected =
      Game::Session::session_for(*world).selection().get_selected_units();
  selection.assign(selected.begin(), selected.end());
  info.attacked_by_selection =
      App::Core::count_selection_attacking(world, selection, id);
  info.attacked_by_local = App::Core::count_units_attacking(world, id, local);
  info.attackers_incoming = App::Core::count_enemies_attacking(world, id, local);
  return info;
}

void FocusTracker::sync_focus_targets(const FocusInputs& inputs) {
  QVariantMap inspect;
  QVariantMap target;
  Engine::Core::World* world = inputs.world;
  if (world != nullptr) {
    auto* selection_system = &Game::Session::session_for(*world).selection();
    if (selection_system != nullptr) {
      const auto& selection = selection_system->get_selected_units();
      const auto inspected = selection_system->inspected_entity();
      const auto focus = App::Core::resolve_focus_entity(
          world, selection, inspected, inputs.local_owner_id);
      if (inspected != Engine::Core::NULL_ENTITY && focus != inspected) {
        selection_system->clear_inspected_entity();
      }
      const auto inspect_info = describe(inputs, focus);
      if (inspect_info.valid) {
        inspect = App::Core::focus_target_to_variant(inspect_info);
      }
      const auto primary = App::Core::primary_attack_target(world, selection);
      const auto target_info = describe(inputs, primary);
      if (target_info.valid) {
        target = App::Core::focus_target_to_variant(target_info);
      }
    }
  }
  if (inspect == m_inspect_target && target == m_selection_target) {
    return;
  }
  m_inspect_target = std::move(inspect);
  m_selection_target = std::move(target);
  if (inputs.activity != nullptr) {
    inputs.activity->set_focus_targets(m_inspect_target, m_selection_target);
  }
}

void FocusTracker::sync_target_focus_markers(const FocusInputs& inputs) {
  m_markers.clear();
  Engine::Core::World* world = inputs.world;
  if (world == nullptr || inputs.spectator_mode) {
    return;
  }
  auto* selection_system = &Game::Session::session_for(*world).selection();
  if (selection_system == nullptr) {
    return;
  }
  const auto snapshot =
      inputs.visibility != nullptr ? inputs.visibility->current_snapshot() : nullptr;
  Game::Systems::TargetFocusRequest request;
  request.world = world;
  request.local_owner_id = inputs.local_owner_id;
  request.selection = &selection_system->get_selected_units();
  request.inspected = selection_system->inspected_entity();
  request.max_locked_targets = Game::Systems::k_target_focus_max_locked;
  request.max_incoming_attackers = Game::Systems::k_target_focus_max_incoming;
  request.visibility =
      (snapshot != nullptr && snapshot->initialized) ? snapshot.get() : nullptr;
  request.owners = inputs.session != nullptr ? &inputs.session->owners() : nullptr;
  m_markers = Game::Systems::collect_target_focus_markers(request);
}

} // namespace App::World
