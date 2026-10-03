#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

void ArenaScenarioRunner::Impl::check_formation_order(
    const ArenaExpectation& expectation) {
  auto const* definition = group_definition(expectation.group);
  auto const& group_ids = ids(expectation.group);
  if (definition == nullptr || group_ids.size() < 2) {
    return;
  }
  QVector3D axis = definition->spacing;
  axis.setY(0.0F);
  if (axis.lengthSquared() < 0.0001F) {
    return;
  }
  axis.normalize();
  float const tolerance = expectation.threshold > 0.0F ? expectation.threshold : 0.75F;
  float previous_projection = -std::numeric_limits<float>::max();
  for (auto entity_id : group_ids) {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform == nullptr || !entity_alive(entity_id)) {
      continue;
    }
    float const projection =
        QVector3D::dotProduct(vector_from_transform(*transform), axis);
    if (projection + tolerance < previous_projection) {
      add_issue(QStringLiteral("formation_order_inversion"),
                QStringLiteral("%1 formation members crossed their stable order")
                    .arg(expectation.group),
                entity_id);
      return;
    }
    previous_projection = projection;
  }
}

void ArenaScenarioRunner::Impl::publish_movement_diagnostics() {
  report.movement.clear();
  for (auto const& group : scenario.groups) {
    auto const found = stall_observations.constFind(group.name);
    if (found == stall_observations.constEnd()) {
      continue;
    }
    ArenaGroupMovementDiagnostics row;
    row.group = group.name;
    row.worst_stalled_seconds = found->worst_stalled_seconds;
    row.worst_stalled_at = found->worst_stalled_at;
    row.recovery_attempts = found->recovery_attempts;
    row.repaths = found->repaths;
    row.abandons = found->abandons;
    row.worst_state = found->worst_state;
    row.has_objective = found->has_objective;
    row.objective_x = found->objective_x;
    row.objective_z = found->objective_z;
    for (auto entity_id : ids(group.name)) {
      auto const* movement = world.try_get<Engine::Core::MovementComponent>(entity_id);
      auto const* movement_facts =
          world.try_get<Engine::Core::MovementFactsComponent>(entity_id);
      if (movement == nullptr || movement_facts == nullptr ||
          !entity_alive(entity_id)) {
        continue;
      }
      if (movement->get_has_target() && movement_facts->progress.stall.rung !=
                                            Engine::Core::MovementRecoveryRung::None) {
        ++row.units_holding_a_stalled_objective;
      }
    }
    if (row.worst_stalled_seconds <= 0.0F && row.recovery_attempts == 0U &&
        row.repaths == 0U && row.abandons == 0U &&
        row.units_holding_a_stalled_objective == 0) {
      continue;
    }
    report.movement.push_back(std::move(row));
  }
}

void ArenaScenarioRunner::Impl::publish_narrow_layout_outcome() {
  report.narrow_layout.clear();
  for (auto const& group : scenario.groups) {
    if (!narrow_layout.contains(group.name)) {
      continue;
    }
    auto const& state = narrow_layout.value(group.name);
    ArenaNarrowLayoutOutcome outcome;
    outcome.group = group.name;
    outcome.engaged = state.engaged;
    outcome.formation_half_width = state.formation_half_width;
    outcome.narrowest_corridor_half_width = state.narrowest_corridor;
    outcome.normal_files = state.normal_files;
    outcome.narrowest_files = state.narrowest_files;
    outcome.tightest_file_spacing = state.tightest_file_spacing;
    outcome.narrowest_frontage = state.narrowest_frontage;
    outcome.deepest_column = state.deepest_column;
    outcome.narrowest_mode = QString::fromLatin1(
        Engine::Core::traversal_layout_mode_name(state.narrowest_mode));
    outcome.mode_changes = state.mode_changes;
    outcome.worst_reform_error = state.worst_reform_error;
    report.narrow_layout.push_back(std::move(outcome));
  }
}

void ArenaScenarioRunner::Impl::check_end_expectations() {
  if (end_expectations_checked) {
    return;
  }
  end_expectations_checked = true;
  observe_battle();
  publish_battle_outcome();
  publish_movement_diagnostics();
  publish_narrow_layout_outcome();
  for (auto const& expectation : scenario.expectations) {
    check_battle_expectation(expectation);
    check_combat_expectation(expectation);
    check_commander_expectation(expectation);
    check_movement_expectation(expectation);
    check_frame_expectation(expectation);
    check_rpg_expectation(expectation);
    check_world_expectation(expectation);
  }
}

} // namespace Arena
