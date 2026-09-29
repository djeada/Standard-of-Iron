#include "app/orders/mode_toggle_commands.h"

#include <QPointF>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "app/orders/local_command.h"
#include "app/orders/order_cues.h"
#include "app/orders/rts_action_model.h"
#include "game/audio/audio_cues.h"
#include "game/command/command.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "scene/camera.h"

namespace App::Controllers {

namespace {

struct ModeTally {
  int eligible = 0;
  int active = 0;

  [[nodiscard]] auto should_enable() const -> bool { return active < eligible; }
};

template <class Mode, class CanUse>
auto tally_mode(Engine::Core::World& world,
                const std::vector<Engine::Core::EntityID>& selected,
                CanUse can_use) -> ModeTally {
  ModeTally tally;
  for (auto id : selected) {
    auto* entity = world.get_entity(id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit == nullptr || !can_use(unit->spawn_type)) {
      continue;
    }
    tally.eligible++;
    auto* mode = entity->get_component<Mode>();
    if (mode != nullptr && mode->active) {
      tally.active++;
    }
  }
  return tally;
}

template <class Mode>
auto any_active(Engine::Core::World& world,
                const std::vector<Engine::Core::EntityID>& selected) -> bool {
  for (Engine::Core::EntityID const id : selected) {
    Engine::Core::Entity* entity = world.get_entity(id);
    auto* mode = entity != nullptr ? entity->get_component<Mode>() : nullptr;
    if (mode != nullptr && mode->active) {
      return true;
    }
  }
  return false;
}

auto gate_mode_name(Engine::Core::GateComponent::ManualMode mode) -> QString {
  switch (mode) {
  case Engine::Core::GateComponent::ManualMode::ForcedOpen:
    return QStringLiteral("open");
  case Engine::Core::GateComponent::ManualMode::ForcedClosed:
    return QStringLiteral("closed");
  case Engine::Core::GateComponent::ManualMode::Automatic:
    break;
  }
  return QStringLiteral("auto");
}

auto next_gate_mode(Engine::Core::World& world, Engine::Core::EntityID first_gate)
    -> Engine::Core::GateComponent::ManualMode {
  using Mode = Engine::Core::GateComponent::ManualMode;
  auto* first = world.get_entity(first_gate);
  const auto* gate =
      first != nullptr ? first->get_component<Engine::Core::GateComponent>() : nullptr;
  if (gate == nullptr) {
    return Mode::ForcedOpen;
  }
  switch (gate->manual_mode) {
  case Mode::Automatic:
    return Mode::ForcedOpen;
  case Mode::ForcedOpen:
    return Mode::ForcedClosed;
  case Mode::ForcedClosed:
    return Mode::Automatic;
  }
  return Mode::ForcedOpen;
}

struct RunCandidate {
  Engine::Core::Entity* entity;
  Engine::Core::StaminaComponent* stamina;
  Game::Systems::NationID nation_id;
  Game::Units::SpawnType spawn_type;
};

auto run_candidates(Engine::Core::World& world,
                    const std::vector<Engine::Core::EntityID>& selected)
    -> std::vector<RunCandidate> {
  std::vector<RunCandidate> candidates;
  candidates.reserve(selected.size());
  for (const auto id : selected) {
    auto* entity = world.get_entity(id);
    const auto* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit == nullptr || !Game::Units::can_use_run_mode(unit->spawn_type)) {
      continue;
    }
    candidates.push_back({entity,
                          entity->get_component<Engine::Core::StaminaComponent>(),
                          unit->nation_id,
                          unit->spawn_type});
  }
  return candidates;
}

void set_run_requested(RunCandidate& candidate, bool enable) {
  if (!enable) {
    if (candidate.stamina != nullptr) {
      candidate.stamina->run_requested = false;
      candidate.stamina->is_running = false;
    }
    return;
  }
  if (candidate.stamina == nullptr) {
    candidate.stamina =
        candidate.entity->add_component<Engine::Core::StaminaComponent>();
    const auto troop_type = Game::Units::spawn_typeToTroopType(candidate.spawn_type);
    if (troop_type.has_value()) {
      const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          candidate.nation_id, *troop_type);
      candidate.stamina->initialize_from_stats(profile.combat.max_stamina,
                                               profile.combat.stamina_regen_rate,
                                               profile.combat.stamina_depletion_rate);
    }
  }
  candidate.stamina->run_requested = true;
}

auto finished(CommandResult result) -> CommandResult {
  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

} // namespace

ModeToggleCommands::ModeToggleCommands(Engine::Core::World* world,
                                       Game::Session::SelectionService* selection,
                                       App::Orders::OrderIssuer& orders,
                                       QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_selection(selection)
    , m_orders(orders) {
}

auto ModeToggleCommands::ready() const -> bool {
  return m_selection != nullptr && m_world != nullptr;
}

auto ModeToggleCommands::on_stop_command() -> CommandResult {
  CommandResult result;
  if (!ready()) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Stop, App::Core::no_selection_reason());
    return result;
  }

  bool had_hold_mode = false;
  bool had_active_formation = false;
  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    had_hold_mode = had_hold_mode ||
                    entity->get_component<Engine::Core::HoldModeComponent>() != nullptr;
    const auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    had_active_formation =
        had_active_formation || (formation_mode != nullptr && formation_mode->active);
  }

  result.order =
      m_orders.issue(App::Core::OrderKind::Stop,
                     Game::Command::Stop{.units = {selected.begin(), selected.end()}});
  if (!result.order.accepted()) {
    return result;
  }

  if (had_hold_mode) {
    emit hold_mode_changed(false);
  }
  if (had_active_formation) {
    emit formation_mode_changed(false);
  }
  return finished(std::move(result));
}

auto ModeToggleCommands::on_hold_command() -> CommandResult {
  CommandResult result;
  if (!ready()) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Hold, App::Core::no_selection_reason());
    return result;
  }

  const ModeTally tally = tally_mode<Engine::Core::HoldModeComponent>(
      *m_world, selected, Game::Units::can_use_hold_mode);
  if (tally.eligible == 0) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Hold,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Hold));
    return result;
  }

  const bool enable = tally.should_enable();
  result.order =
      m_orders.issue(App::Core::OrderKind::Hold,
                     Game::Command::SetHold{.units = {selected.begin(), selected.end()},
                                            .active = enable});
  if (!result.order.accepted()) {
    return result;
  }
  emit hold_mode_changed(enable);
  return finished(std::move(result));
}

auto ModeToggleCommands::on_guard_command() -> CommandResult {
  CommandResult result;
  if (!ready()) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Guard, App::Core::no_selection_reason());
    return result;
  }

  const ModeTally tally = tally_mode<Engine::Core::GuardModeComponent>(
      *m_world, selected, Game::Units::can_use_guard_mode);
  if (tally.eligible == 0) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Guard,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Guard));
    return result;
  }

  const bool enable = tally.should_enable();
  result.order = m_orders.issue(
      App::Core::OrderKind::Guard,
      Game::Command::SetGuard{.units = {selected.begin(), selected.end()},
                              .active = enable});
  if (!result.order.accepted()) {
    return result;
  }
  emit guard_mode_changed(enable);
  return finished(std::move(result));
}

auto ModeToggleCommands::on_guard_click(const PointerTarget& target) -> CommandResult {
  CommandResult result;
  result.reset_cursor_to_normal = true;
  if (!ready() || target.camera == nullptr) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    result.order =
        m_orders.reject(App::Core::OrderKind::Guard, App::Core::no_selection_reason());
    return result;
  }

  auto const guard_units = App::Core::filter_selected_units_for_action(
      m_world, selected, QStringLiteral("guard"));
  if (guard_units.empty()) {
    result.order = m_orders.reject(
        App::Core::OrderKind::Guard,
        App::Core::no_eligible_units_reason(App::Core::OrderKind::Guard));
    return result;
  }

  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(QPointF(target.sx, target.sy),
                                                       *target.camera,
                                                       target.viewport_width,
                                                       target.viewport_height,
                                                       hit)) {
    result.order = m_orders.reject(App::Core::OrderKind::Guard,
                                   App::Core::no_ground_under_cursor_reason());
    return result;
  }

  result.order = m_orders.issue(
      App::Core::OrderKind::Guard,
      Game::Command::SetGuard{
          .units = guard_units, .active = true, .anchor = hit, .has_anchor = true},
      0,
      &hit);
  if (!result.order.accepted()) {
    return result;
  }

  emit guard_mode_changed(true);
  result.input_consumed = true;
  return result;
}

auto ModeToggleCommands::on_gate_command() -> CommandResult {
  CommandResult result;
  if (!ready()) {
    return result;
  }

  auto const gates = App::Core::filter_selected_units_for_action(
      m_world, m_selection->get_selected_units(), QStringLiteral("gate"));
  if (gates.empty()) {
    return result;
  }

  const auto next_mode = next_gate_mode(*m_world, gates.front());
  App::Orders::submit_local_command(
      m_world, Game::Command::SetGateMode{.units = gates, .mode = next_mode});
  emit gate_mode_changed(gate_mode_name(next_mode));
  return finished(std::move(result));
}

auto ModeToggleCommands::on_run_command() -> CommandResult {
  CommandResult result;
  if (!ready()) {
    return result;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  auto candidates = run_candidates(*m_world, selected);
  if (candidates.empty()) {
    return result;
  }

  const auto already_running = std::count_if(
      candidates.begin(), candidates.end(), [](const RunCandidate& candidate) {
        return candidate.stamina != nullptr && candidate.stamina->run_requested;
      });
  const bool enable = already_running < static_cast<std::ptrdiff_t>(candidates.size());

  for (auto& candidate : candidates) {
    set_run_requested(candidate, enable);
  }

  if (enable) {
    Game::Audio::play_cue(
        App::Orders::charge_cue(App::Orders::selection_mounts(*m_world, selected)));
  }
  emit run_mode_changed(enable);
  return finished(std::move(result));
}

void ModeToggleCommands::enable_run_mode_for_selected() {
  if (!ready()) {
    return;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    return;
  }

  App::Orders::submit_local_command(
      m_world,
      Game::Command::SetRunMode{.units = {selected.begin(), selected.end()},
                                .active = true});

  Game::Audio::play_cue(
      App::Orders::charge_cue(App::Orders::selection_mounts(*m_world, selected)));
  emit run_mode_changed(true);
}

void ModeToggleCommands::disable_run_mode_for_selected() {
  if (!ready()) {
    return;
  }

  const auto& selected = m_selection->get_selected_units();
  if (selected.empty()) {
    return;
  }

  App::Orders::submit_local_command(
      m_world,
      Game::Command::SetRunMode{.units = {selected.begin(), selected.end()},
                                .active = false});
  emit run_mode_changed(false);
}

auto ModeToggleCommands::any_selected_in_hold_mode() const -> bool {
  return ready() && any_active<Engine::Core::HoldModeComponent>(
                        *m_world, m_selection->get_selected_units());
}

auto ModeToggleCommands::any_selected_in_guard_mode() const -> bool {
  return ready() && any_active<Engine::Core::GuardModeComponent>(
                        *m_world, m_selection->get_selected_units());
}

auto ModeToggleCommands::any_selected_in_run_mode() const -> bool {
  if (!ready()) {
    return false;
  }
  for (const auto id : m_selection->get_selected_units()) {
    const auto* entity = m_world->get_entity(id);
    const auto* stamina = entity != nullptr
                              ? entity->get_component<Engine::Core::StaminaComponent>()
                              : nullptr;
    if (stamina != nullptr && stamina->run_requested) {
      return true;
    }
  }
  return false;
}

} // namespace App::Controllers
