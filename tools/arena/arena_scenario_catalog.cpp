#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <iterator>
#include <utility>

#include "arena_ai_duel_scenarios.h"
#include "arena_allied_nation_scenarios.h"
#include "arena_ambience_scenarios.h"
#include "arena_animation_matrix_scenarios.h"
#include "arena_battle_order_scenarios.h"
#include "arena_battle_scale_scenarios.h"
#include "arena_cinematic_scenarios.h"
#include "arena_city_scenarios.h"
#include "arena_combat_scenarios.h"
#include "arena_commander_duel_scenarios.h"
#include "arena_economy_scenarios.h"
#include "arena_engagement_scenarios.h"
#include "arena_facade_scenarios.h"
#include "arena_formation_scenarios.h"
#include "arena_gate_scenarios.h"
#include "arena_grounding_scenarios.h"
#include "arena_hazard_scenarios.h"
#include "arena_identity_scenarios.h"
#include "arena_lighting_scenarios.h"
#include "arena_maneuver_scenarios.h"
#include "arena_matchup_matrix_scenarios.h"
#include "arena_navigation_scenarios.h"
#include "arena_pathing_scenarios.h"
#include "arena_promo_scenarios.h"
#include "arena_range_indicator_scenarios.h"
#include "arena_rpg_combat_scenarios.h"
#include "arena_rpg_control_scenarios.h"
#include "arena_rpg_friendly_scenarios.h"
#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "arena_sepulcher_scenarios.h"
#include "arena_settlement_life_scenarios.h"
#include "arena_showcase_scenarios.h"
#include "arena_skirmisher_scenarios.h"
#include "arena_spotlight_scenarios.h"
#include "arena_structure_lifecycle_scenarios.h"
#include "arena_stuck_recovery_scenarios.h"
#include "arena_trailer_scenarios.h"
#include "arena_transition_gauntlet_scenarios.h"
#include "arena_traversal_scenarios.h"
#include "arena_weapon_scenarios.h"
#include "arena_wildlife_scenarios.h"
#include "arena_world_review_scenarios.h"

namespace Arena::Scenarios {
namespace {

void append(std::vector<ArenaScenarioDefinition>& values,
            std::vector<ArenaScenarioDefinition> more) {
  values.insert(values.end(),
                std::make_move_iterator(more.begin()),
                std::make_move_iterator(more.end()));
}

auto build_core_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  for (auto const build : {
           &build_rpg_control_definitions,
           &build_rpg_combat_definitions,
           &build_rpg_friendly_definitions,
           &build_identity_definitions,
           &build_commander_duel_definitions,
           &build_weapon_definitions,
           &build_animation_matrix_definitions,
           &build_combat_definitions,
           &build_pathing_definitions,
           &build_gate_definitions,
           &build_battle_scale_definitions,
           &build_world_review_definitions,
           &build_settlement_life_definitions,
           &build_sepulcher_definitions,
           &build_lighting_definitions,
           &build_promo_definitions,
           &build_range_indicator_definitions,
       }) {
    append(result, build());
  }
  return result;
}

} // namespace

auto definitions() -> const std::vector<ArenaScenarioDefinition>& {
  static const std::vector<ArenaScenarioDefinition> catalog = [] {
    auto values = build_core_definitions();
    append(values, build_showcase_definitions());
    append(values, build_facade_definitions());
    append(values, build_formation_definitions());
    append(values, build_navigation_definitions());
    append(values, build_stuck_recovery_definitions());
    append(values, build_maneuver_definitions());
    append(values, build_traversal_definitions());
    append(values, build_hazard_definitions());
    append(values, build_wildlife_definitions());
    append(values, build_trailer_definitions());
    append(values, build_cinematic_definitions());
    append(values, build_ambience_definitions());
    append(values, build_spotlight_definitions());
    append(values, build_city_definitions());
    append(values, build_ai_duel_definitions());
    append(values, build_economy_definitions());
    append(values, build_engagement_definitions());
    append(values, build_structure_lifecycle_definitions());
    append(values, build_grounding_definitions());
    append(values, build_allied_nation_definitions());
    append(values, build_skirmisher_definitions());
    append(values, build_transition_gauntlet_definitions());
    append(values, build_matchup_matrix_definitions());
    append(values, build_battle_order_definitions());

    for (auto& scenario : values) {
      if (scenario.rpg_mode && !scenario.rpg_commander_group.isEmpty()) {
        builders::add_commander_control_metrics(scenario, scenario.rpg_commander_group);
      }
    }
    return values;
  }();
  return catalog;
}

namespace {

auto runtime_definitions() -> std::vector<ArenaScenarioDefinition>& {
  static std::vector<ArenaScenarioDefinition> generated;
  return generated;
}

} // namespace

void register_runtime_definition(ArenaScenarioDefinition definition) {
  auto& generated = runtime_definitions();
  auto const existing =
      std::find_if(generated.begin(), generated.end(), [&](auto const& scenario) {
        return scenario.id == definition.id;
      });
  if (existing == generated.end()) {
    generated.push_back(std::move(definition));
    return;
  }
  *existing = std::move(definition);
}

void clear_runtime_definitions() {
  runtime_definitions().clear();
}

auto find_definition(const QString& scenario_id) -> const ArenaScenarioDefinition* {
  auto const& generated = runtime_definitions();
  auto const runtime =
      std::find_if(generated.begin(), generated.end(), [&](auto const& scenario) {
        return scenario.id == scenario_id;
      });
  if (runtime != generated.end()) {
    return &*runtime;
  }

  auto const found =
      std::find_if(definitions().begin(),
                   definitions().end(),
                   [&](auto const& scenario) { return scenario.id == scenario_id; });
  return found == definitions().end() ? nullptr : &*found;
}

auto select_definition_ids(const QString& selection, QString* error) -> QStringList {
  QStringList selected;
  auto const add = [&selected](const QString& id) {
    if (!selected.contains(id)) {
      selected.push_back(id);
    }
  };
  auto const fail = [error](const QString& message) {
    if (error != nullptr) {
      *error = message;
    }
    return QStringList{};
  };

  for (const QString& raw : selection.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const QString pattern = raw.trimmed();
    if (pattern.isEmpty()) {
      continue;
    }
    const bool wildcard = pattern.contains(QLatin1Char('*')) ||
                          pattern.contains(QLatin1Char('?')) ||
                          pattern.contains(QLatin1Char('['));
    if (!wildcard) {
      if (find_definition(pattern) == nullptr) {
        return fail(QStringLiteral("Unknown Arena scenario '%1'; use --list-scenarios")
                        .arg(pattern));
      }
      add(pattern);
      continue;
    }

    const QRegularExpression matcher = QRegularExpression::fromWildcard(pattern);
    bool matched = false;
    for (auto const& scenario : definitions()) {
      if (matcher.match(scenario.id).hasMatch()) {
        add(scenario.id);
        matched = true;
      }
    }
    if (!matched) {
      return fail(QStringLiteral("Arena scenario pattern '%1' matched nothing; use "
                                 "--list-scenarios")
                      .arg(pattern));
    }
  }

  if (selected.isEmpty()) {
    return fail(QStringLiteral("No Arena scenarios selected"));
  }
  return selected;
}

auto options() -> const std::vector<ScenarioOption>& {
  static const std::vector<ScenarioOption> catalog = [] {
    std::vector<ScenarioOption> values;
    values.reserve(definitions().size());
    for (auto const& scenario : definitions()) {
      values.push_back({scenario.id, scenario.label, scenario.description});
    }
    return values;
  }();
  return catalog;
}

auto find_option(const QString& scenario_id) -> const ScenarioOption* {
  auto const found =
      std::find_if(options().begin(), options().end(), [&](auto const& option) {
        return option.id == scenario_id;
      });
  return found == options().end() ? nullptr : &*found;
}

} // namespace Arena::Scenarios
