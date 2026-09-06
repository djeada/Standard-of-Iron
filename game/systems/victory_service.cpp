#include "victory_service.h"

#include <QDebug>
#include <qglobal.h>

#include <algorithm>
#include <set>
#include <variant>

#include "core/event_manager.h"
#include "game/core/component_commander.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems {

namespace {

constexpr float k_startup_delay_seconds = 0.35F;

constexpr float k_spectator_poll_seconds = 0.5F;

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

auto normalize_structure_types(std::vector<QString> structure_types,
                               std::vector<QString> fallback = {
                                   "barracks"}) -> std::vector<QString> {
  if (structure_types.empty()) {
    structure_types = std::move(fallback);
  }

  std::vector<QString> normalized;
  QSet<QString> seen_types;
  for (auto& type : structure_types) {
    QString normalized_type = type.trimmed().toLower();
    if (normalized_type == "village") {
      normalized_type = "barracks";
    }
    if (normalized_type.isEmpty() || seen_types.contains(normalized_type)) {
      continue;
    }
    seen_types.insert(normalized_type);
    normalized.push_back(std::move(normalized_type));
  }

  if (normalized.empty()) {
    normalized.push_back(QStringLiteral("barracks"));
  }

  return normalized;
}

void append_undead_objectives(const Game::Map::VictoryConfig& config,
                              VictoryRuleSet& rules) {
  for (const auto& objective : config.undead_objectives) {
    if (objective.zone_id.isEmpty()) {
      continue;
    }
    const std::size_t before = rules.victory_rules.size();
    if (objective.type == "clear_undead_zone") {
      rules.victory_rules.emplace_back(ClearUndeadZoneVictoryRule{objective.zone_id});
    } else if (objective.type == "purify_shrine") {
      rules.victory_rules.emplace_back(PurifyShrineVictoryRule{objective.zone_id});
    } else if (objective.type == "survive_undead_wave") {
      rules.victory_rules.emplace_back(SurviveUndeadWaveVictoryRule{
          objective.zone_id, std::max(1, objective.wave_count)});
    } else {
      qWarning() << "Unknown undead victory objective" << objective.type
                 << "- ignoring";
      continue;
    }
    for (std::size_t index = before; index < rules.victory_rules.size(); ++index) {
      rules.victory_rules[index].id = objective.zone_id;
    }
  }
}

auto build_rule_set_from_config(const Game::Map::VictoryConfig& config)
    -> VictoryRuleSet {
  VictoryRuleSet rules;

  QString const victory_type = config.victory_type.trimmed().toLower();
  if (victory_type == "undead_zones") {
    append_undead_objectives(config, rules);
    if (rules.victory_rules.empty()) {
      qWarning() << "Victory type undead_zones declares no undead_objectives - "
                    "defaulting to elimination";
      rules.victory_rules.emplace_back(
          EliminationVictoryRule{{QStringLiteral("barracks")}});
    }
  } else if (victory_type == "elimination") {
    rules.victory_rules.emplace_back(
        EliminationVictoryRule{normalize_structure_types(config.key_structures)});
  } else if (victory_type == "control_structures") {
    rules.victory_rules.emplace_back(ControlStructuresVictoryRule{
        StructureRequirement{normalize_structure_types(config.key_structures),
                             std::max(1, config.required_key_structures)}});
  } else if (victory_type == "capture_structures") {
    rules.victory_rules.emplace_back(CaptureStructuresVictoryRule{
        StructureRequirement{normalize_structure_types(config.key_structures),
                             std::max(1, config.required_key_structures)}});
  } else if (victory_type == "survive_time") {
    rules.victory_rules.emplace_back(
        SurviveTimeVictoryRule{std::max(0.0F, config.survive_time_duration)});
  } else {
    qWarning() << "Unknown victory type" << config.victory_type
               << "- defaulting to elimination";
    rules.victory_rules.emplace_back(
        EliminationVictoryRule{{QStringLiteral("barracks")}});
  }

  if (victory_type != "undead_zones") {
    append_undead_objectives(config, rules);
  }

  std::vector<QString> const default_defeat_structures =
      normalize_structure_types(config.key_structures);
  bool has_commander_defeat = false;
  for (const auto& condition : config.defeat_conditions) {
    QString const normalized_condition = condition.trimmed().toLower();
    if (normalized_condition == "no_units") {
      rules.defeat_rules.emplace_back(NoUnitsDefeatRule{});
      continue;
    }
    if (normalized_condition == "no_key_structures") {
      rules.defeat_rules.emplace_back(
          NoKeyStructuresDefeatRule{default_defeat_structures});
      continue;
    }
    if (normalized_condition == "no_commander") {
      rules.defeat_rules.emplace_back(NoCommanderDefeatRule{});
      has_commander_defeat = true;
      continue;
    }
    if (normalized_condition == "only_commander_remaining") {
      rules.defeat_rules.emplace_back(
          OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});
      continue;
    }
    qWarning() << "Unknown defeat condition" << condition << "- ignoring";
  }

  if (rules.defeat_rules.empty()) {
    rules.defeat_rules.emplace_back(
        OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});
  }

  if (!has_commander_defeat) {
    rules.defeat_rules.emplace_back(NoCommanderDefeatRule{});
  }

  bool const already_wins_on_commanders = std::any_of(
      rules.victory_rules.begin(),
      rules.victory_rules.end(),
      [](const VictoryObjective& objective) {
        return std::holds_alternative<EliminateCommandersVictoryRule>(objective.rule);
      });
  if (!already_wins_on_commanders) {
    rules.victory_rules.emplace_back(EliminateCommandersVictoryRule{});
  }

  return rules;
}

auto count_matching_structures(const QHash<QString, int>& structure_counts,
                               const std::vector<QString>& structure_types) -> int {
  int matching_count = 0;
  for (const auto& structure_type : structure_types) {
    matching_count += structure_counts.value(structure_type, 0);
  }
  return matching_count;
}

} // namespace

VictoryService::VictoryService(Services services)
    : m_unit_spawned_subscription(
          [this](const Engine::Core::UnitSpawnedEvent& e) { on_unit_spawned(e); })
    , m_unit_died_subscription(
          [this](const Engine::Core::UnitDiedEvent& e) { on_unit_died(e); })
    , m_barrack_captured_subscription(
          [this](const Engine::Core::BarrackCapturedEvent& e) {
            on_barrack_captured(e);
          })
    , m_stats_registry(services.stats)
    , m_owner_registry(services.owners)
    , m_nations(services.nations)
    , m_economy(services.economy) {
}

VictoryService::~VictoryService() = default;

void VictoryService::reset() {
  m_rule_set = {};
  m_tracked_enemy_structure_types.clear();
  m_tracked_local_structure_types.clear();
  m_only_commander_structure_types.clear();
  m_elapsed_time = 0.0F;
  m_startup_delay = 0.0F;
  m_has_time_based_victory = false;
  m_has_undead_zone_rules = false;
  m_has_world_based_rules = false;
  m_has_resource_victory = false;
  m_has_wave_victory = false;
  m_has_time_limit_defeat = false;
  m_requires_captured_structure_tracking = false;
  m_has_only_commander_defeat_rule = false;
  m_only_commander_defeat_armed = false;
  m_has_eliminate_commanders_rule = false;
  m_eliminate_commanders_armed = false;
  m_world_state_dirty = false;
  m_spectator_mode = false;
  m_spectator_saw_rivals = false;
  m_spectator_poll_timer = 0.0F;
  m_last_world_summary = {};
  m_has_world_summary = false;
  m_objective_complete.clear();
  m_local_owner_id = 1;
  m_victory_state.clear();
  m_defeat_description.clear();
  m_world_ptr = nullptr;
  m_victory_callback = nullptr;
}

void VictoryService::set_spectator_mode(bool enabled) {
  if (m_spectator_mode == enabled) {
    return;
  }
  m_spectator_mode = enabled;
  m_spectator_saw_rivals = false;
  m_spectator_poll_timer = k_spectator_poll_seconds;
  if (enabled) {

    m_has_world_based_rules = true;
    m_world_state_dirty = true;
  }
}

void VictoryService::configure(const Game::Map::VictoryConfig& config,
                               int local_owner_id) {
  configure(build_rule_set_from_config(config), local_owner_id);
}

void VictoryService::configure(const VictoryRuleSet& rules, int local_owner_id) {
  reset();

  m_local_owner_id = local_owner_id;
  m_rule_set = rules;
  if (m_rule_set.victory_rules.empty()) {
    qWarning() << "Configured empty victory rule set - defaulting to elimination";
    m_rule_set.victory_rules.emplace_back(
        EliminationVictoryRule{{QStringLiteral("barracks")}});
  }

  refresh_rule_metadata();
  m_world_state_dirty = true;
  m_startup_delay = k_startup_delay_seconds;
}

void VictoryService::update(Engine::Core::World& world, float delta_time) {
  if (!m_victory_state.isEmpty()) {
    return;
  }

  m_world_ptr = &world;

  if ((m_has_only_commander_defeat_rule && !m_only_commander_defeat_armed) ||
      (m_has_eliminate_commanders_rule && !m_eliminate_commanders_armed)) {
    update_rule_arming(summarize_world(world));
  }

  if (m_startup_delay > 0.0F) {
    m_startup_delay = std::max(0.0F, m_startup_delay - delta_time);
    if (m_startup_delay > 0.0F) {
      return;
    }
    mark_world_dirty();
  }

  m_elapsed_time += delta_time;

  if (m_spectator_mode) {

    m_spectator_poll_timer += delta_time;
    if (m_spectator_poll_timer >= k_spectator_poll_seconds) {
      m_spectator_poll_timer = 0.0F;
      evaluate_spectator_state();
    }
    return;
  }

  if (m_world_state_dirty || !m_has_world_summary) {
    evaluate_world_state(world);
    return;
  }

  if (m_has_time_based_victory || m_has_undead_zone_rules || m_has_resource_victory ||
      m_has_wave_victory || m_has_time_limit_defeat) {
    evaluate_polled_rules();
  }
}

void VictoryService::on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event) {
  Q_UNUSED(event);
  mark_world_dirty();
  reevaluate_world_state();
}

void VictoryService::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  Q_UNUSED(event);
  mark_world_dirty();
  reevaluate_world_state();
}

void VictoryService::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent& event) {
  Q_UNUSED(event);
  mark_world_dirty();
  reevaluate_world_state();
}

void VictoryService::mark_world_dirty() {
  if (m_has_world_based_rules) {
    m_world_state_dirty = true;
  }
}

void VictoryService::reevaluate_world_state() {
  if (can_evaluate()) {
    evaluate_world_state(*m_world_ptr);
  }
}

void VictoryService::refresh_rule_metadata() {
  m_tracked_enemy_structure_types.clear();
  m_tracked_local_structure_types.clear();
  m_has_time_based_victory = false;
  m_has_undead_zone_rules = false;
  m_has_world_based_rules = false;
  m_has_resource_victory = false;
  m_has_wave_victory = false;
  m_has_time_limit_defeat = false;
  m_requires_captured_structure_tracking = false;
  m_has_only_commander_defeat_rule = false;
  m_has_eliminate_commanders_rule = false;
  m_only_commander_structure_types.clear();

  for (const auto& objective : m_rule_set.victory_rules) {
    std::visit(
        Overloaded{
            [this](const EliminationVictoryRule& elimination_rule) {
              m_has_world_based_rules = true;
              for (const auto& structure_type : elimination_rule.structure_types) {
                m_tracked_enemy_structure_types.insert(structure_type);
              }
            },
            [this](const ControlStructuresVictoryRule& control_rule) {
              m_has_world_based_rules = true;
              for (const auto& structure_type : control_rule.target.structure_types) {
                m_tracked_local_structure_types.insert(structure_type);
              }
            },
            [this](const CaptureStructuresVictoryRule& capture_rule) {
              m_has_world_based_rules = true;
              m_requires_captured_structure_tracking = true;
              for (const auto& structure_type : capture_rule.target.structure_types) {
                m_tracked_local_structure_types.insert(structure_type);
              }
            },
            [this](const ClearUndeadZoneVictoryRule&) {
              m_has_undead_zone_rules = true;
            },
            [this](const PurifyShrineVictoryRule&) { m_has_undead_zone_rules = true; },
            [this](const SurviveUndeadWaveVictoryRule&) {
              m_has_undead_zone_rules = true;
            },
            [this](const SurviveTimeVictoryRule&) { m_has_time_based_victory = true; },
            [this](const SurviveWavesVictoryRule&) { m_has_wave_victory = true; },
            [this](const AccumulateResourcesVictoryRule&) {
              m_has_resource_victory = true;
            },
            [this](const EliminateCommandersVictoryRule&) {
              m_has_world_based_rules = true;
              m_has_eliminate_commanders_rule = true;
            }},
        objective.rule);
  }

  for (const auto& condition : m_rule_set.defeat_rules) {
    m_has_world_based_rules = true;
    std::visit(
        Overloaded{
            [](const NoUnitsDefeatRule&) {},
            [](const NoCommanderDefeatRule&) {},
            [this](const NoKeyStructuresDefeatRule& no_structures_rule) {
              for (const auto& structure_type : no_structures_rule.structure_types) {
                m_tracked_local_structure_types.insert(structure_type);
              }
            },
            [this](const OnlyCommanderRemainingDefeatRule& isolated_commander_rule) {
              m_has_only_commander_defeat_rule = true;
              for (const auto& structure_type :
                   isolated_commander_rule.structure_types) {
                if (std::find(m_only_commander_structure_types.begin(),
                              m_only_commander_structure_types.end(),
                              structure_type) ==
                    m_only_commander_structure_types.end()) {
                  m_only_commander_structure_types.push_back(structure_type);
                }
                m_tracked_local_structure_types.insert(structure_type);
              }
            },
            [this](const TimeLimitDefeatRule&) {
              m_has_time_limit_defeat = true;
            }},
        condition.rule);
  }
}

void VictoryService::update_rule_arming(const WorldSummary& summary) {
  if (m_has_only_commander_defeat_rule && !m_only_commander_defeat_armed &&
      (summary.local_non_commander_troop_count > 0 ||
       count_matching_structures(summary.local_owned_structure_counts,
                                 m_only_commander_structure_types) > 0)) {
    m_only_commander_defeat_armed = true;
  }

  if (m_has_eliminate_commanders_rule && !m_eliminate_commanders_armed &&
      summary.enemy_commander_count > 0) {
    m_eliminate_commanders_armed = true;
  }
}

void VictoryService::evaluate_polled_rules() {
  evaluate_rules(m_last_world_summary);
}

void VictoryService::evaluate_world_state(Engine::Core::World& world) {
  m_last_world_summary = summarize_world(world);
  m_has_world_summary = true;
  update_rule_arming(m_last_world_summary);
  m_world_state_dirty = false;
  evaluate_rules(m_last_world_summary);
}

void VictoryService::evaluate_rules(const WorldSummary& summary) {
  if (m_spectator_mode) {

    evaluate_spectator_state();
    return;
  }

  if (!m_rule_set.victory_rules.empty()) {
    bool objectives_changed = false;
    if (m_objective_complete.size() != m_rule_set.victory_rules.size()) {
      m_objective_complete.assign(m_rule_set.victory_rules.size(), false);
      objectives_changed = true;
    }

    bool all_satisfied = true;
    bool any_satisfied = false;
    for (std::size_t index = 0; index < m_rule_set.victory_rules.size(); ++index) {
      const bool satisfied =
          check_victory_rule(m_rule_set.victory_rules[index].rule, summary);
      if (m_objective_complete[index] != satisfied) {
        m_objective_complete[index] = satisfied;
        objectives_changed = true;
      }
      all_satisfied = all_satisfied && satisfied;
      any_satisfied = any_satisfied || satisfied;
    }

    if (objectives_changed && m_objectives_changed_callback) {
      m_objectives_changed_callback();
    }

    if (m_rule_set.require_all_victory_rules ? all_satisfied : any_satisfied) {
      finalize_game(QStringLiteral("victory"));
      return;
    }
  }

  for (const auto& condition : m_rule_set.defeat_rules) {
    if (check_defeat_rule(condition.rule, summary)) {
      m_defeat_description = condition.description;
      finalize_game(QStringLiteral("defeat"));
      return;
    }
  }
}

void VictoryService::evaluate_spectator_state() {
  if (m_world_ptr == nullptr) {
    return;
  }

  std::set<int> live_teams;
  for (auto [entity_id, unit_ref] : m_world_ptr->view<Engine::Core::UnitComponent>()) {
    const auto* unit = &unit_ref;
    if (unit->health <= 0 || Game::Core::is_neutral_owner(unit->owner_id)) {
      continue;
    }
    const auto owner_type = m_owner_registry.get_owner_type(unit->owner_id);
    if (owner_type != OwnerType::Player && owner_type != OwnerType::AI) {
      continue;
    }
    if (unit->nation_id == Game::Systems::NationID::IronSepulcher) {
      continue;
    }
    live_teams.insert(m_owner_registry.get_owner_team(unit->owner_id));
  }

  if (live_teams.size() >= 2U) {
    m_spectator_saw_rivals = true;
    return;
  }

  if (!m_spectator_saw_rivals) {

    return;
  }

  finalize_game(QStringLiteral("spectator"));
}

void VictoryService::finalize_game(const QString& state) {
  m_victory_state = state;
  if (state == QLatin1String("victory")) {
    qInfo() << "VICTORY! Conditions met.";
  } else if (state == QLatin1String("spectator")) {
    qInfo() << "SPECTATOR: one side is left standing.";
  } else {
    qInfo() << "DEFEAT! Condition met.";
  }

  const auto& all_owners = m_owner_registry.get_all_owners();
  for (const auto& owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      m_stats_registry.mark_game_end(owner.owner_id);
    }
  }

  const auto* stats = m_stats_registry.get_stats(m_local_owner_id);
  if (stats != nullptr) {
    qInfo() << "Final Stats - Troops Recruited:" << stats->troops_recruited
            << "Enemies Killed:" << stats->enemies_killed << "Losses:" << stats->losses
            << "Barracks Owned:" << stats->barracks_owned
            << "Play Time:" << stats->play_time_sec << "seconds";
  }

  if (m_victory_callback) {
    m_victory_callback(m_victory_state);
  }
}

auto VictoryService::can_evaluate() const -> bool {
  return m_world_ptr != nullptr && m_startup_delay <= 0.0F && m_victory_state.isEmpty();
}

auto VictoryService::summarize_world(Engine::Core::World& world) const -> WorldSummary {
  WorldSummary summary;
  bool const track_enemy_structures = !m_tracked_enemy_structure_types.isEmpty();
  bool const track_local_structures = !m_tracked_local_structure_types.isEmpty();
  bool const track_structures = track_enemy_structures || track_local_structures;

  auto& nation_registry = m_nations;
  const auto* local_nation = nation_registry.get_nation_for_player(m_local_owner_id);
  Game::Systems::NationID const local_nation_id =
      (local_nation != nullptr) ? local_nation->id
                                : nation_registry.default_nation_id();

  for (auto [entity_id, unit_ref] : world.view<Engine::Core::UnitComponent>()) {
    const auto* unit = &unit_ref;
    if (unit->health <= 0) {
      continue;
    }

    const bool is_commander = world.has<Engine::Core::CommanderComponent>(entity_id);
    bool const is_local_unit = (unit->owner_id == m_local_owner_id);
    if (is_local_unit) {
      summary.local_has_units = true;
      if (is_commander) {
        summary.local_commander_count += 1;
      } else if (Game::Units::is_troop_spawn(unit->spawn_type)) {
        summary.local_non_commander_troop_count += 1;
      }
    } else if (is_commander &&
               m_owner_registry.are_enemies(m_local_owner_id, unit->owner_id)) {
      if (m_rule_set.include_ambient_undead ||
          unit->nation_id != Game::Systems::NationID::IronSepulcher) {
        summary.enemy_commander_count += 1;
      }
    }

    if (!track_structures) {
      continue;
    }

    QString const unit_type =
        QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));

    if (is_local_unit && track_local_structures &&
        m_tracked_local_structure_types.contains(unit_type)) {
      summary.local_owned_structure_counts[unit_type] += 1;
      if (m_requires_captured_structure_tracking) {
        const auto* building =
            world.try_get<Engine::Core::BuildingComponent>(entity_id);
        if (building != nullptr && building->original_nation_id != local_nation_id) {
          summary.local_captured_structure_counts[unit_type] += 1;
        }
      }
      continue;
    }

    if (track_enemy_structures && m_tracked_enemy_structure_types.contains(unit_type) &&
        m_owner_registry.are_enemies(m_local_owner_id, unit->owner_id)) {
      if (!m_rule_set.include_ambient_undead &&
          unit->nation_id == Game::Systems::NationID::IronSepulcher) {
        continue;
      }
      summary.enemy_structure_counts[unit_type] += 1;
    }
  }

  return summary;
}

auto VictoryService::objectives() const -> std::vector<ObjectiveStatus> {
  std::vector<ObjectiveStatus> out;
  out.reserve(m_rule_set.victory_rules.size());
  for (std::size_t index = 0; index < m_rule_set.victory_rules.size(); ++index) {
    const auto& objective = m_rule_set.victory_rules[index];
    const bool complete =
        index < m_objective_complete.size() && m_objective_complete[index];

    ObjectiveStatus status{.id = objective.id,
                           .description = objective.description,
                           .progress = complete ? 1 : 0,
                           .required = 1,
                           .complete = complete};

    std::visit(
        Overloaded{[](const auto&) {},
                   [this, &status](const SurviveUndeadWaveVictoryRule& rule) {
                     status.required = std::max(1, rule.required_wave_count);
                     status.progress =
                         m_undead_zone_query != nullptr
                             ? m_undead_zone_query->completed_wave_count(rule.zone_id)
                             : 0;
                   },
                   [this, &status](const SurviveWavesVictoryRule& rule) {
                     status.required = std::max(1, rule.required_wave_count);
                     status.progress = m_mission_wave_query != nullptr
                                           ? m_mission_wave_query->cleared_wave_count()
                                           : 0;
                   },
                   [this, &status](const ControlStructuresVictoryRule& rule) {
                     status.required = std::max(1, rule.target.required_count);
                     status.progress = count_matching_structures(
                         m_last_world_summary.local_owned_structure_counts,
                         rule.target.structure_types);
                   },
                   [this, &status](const CaptureStructuresVictoryRule& rule) {
                     status.required = std::max(1, rule.target.required_count);
                     status.progress = count_matching_structures(
                         m_last_world_summary.local_captured_structure_counts,
                         rule.target.structure_types);
                   },
                   [this, &status](const AccumulateResourcesVictoryRule& rule) {
                     const auto tally = resource_tally(
                         m_economy.get_harvested_all(m_local_owner_id), rule.required);
                     status.detail = tally.text;
                     status.required = std::max(1, tally.kinds);
                     status.progress = tally.met;
                   }},
        objective.rule);

    status.progress = std::clamp(status.progress, 0, status.required);
    out.push_back(std::move(status));
  }
  return out;
}

auto VictoryService::check_victory_rule(const VictoryRule& rule,
                                        const WorldSummary& summary) const -> bool {
  return std::visit(
      Overloaded{
          [&summary](const EliminationVictoryRule& elimination_rule) {
            return count_matching_structures(summary.enemy_structure_counts,
                                             elimination_rule.structure_types) == 0;
          },
          [this](const SurviveTimeVictoryRule& survive_rule) {
            return m_elapsed_time >= survive_rule.duration;
          },
          [&summary](const ControlStructuresVictoryRule& control_rule) {
            return count_matching_structures(summary.local_owned_structure_counts,
                                             control_rule.target.structure_types) >=
                   std::max(1, control_rule.target.required_count);
          },
          [&summary](const CaptureStructuresVictoryRule& capture_rule) {
            return count_matching_structures(summary.local_captured_structure_counts,
                                             capture_rule.target.structure_types) >=
                   std::max(1, capture_rule.target.required_count);
          },
          [this](const ClearUndeadZoneVictoryRule& zone_rule) {
            return m_undead_zone_query != nullptr &&
                   m_undead_zone_query->is_zone_cleared(zone_rule.zone_id);
          },
          [this](const PurifyShrineVictoryRule& zone_rule) {
            return m_undead_zone_query != nullptr &&
                   m_undead_zone_query->is_shrine_purified(zone_rule.zone_id);
          },
          [this](const SurviveUndeadWaveVictoryRule& zone_rule) {
            return m_undead_zone_query != nullptr &&
                   m_undead_zone_query->completed_wave_count(zone_rule.zone_id) >=
                       std::max(1, zone_rule.required_wave_count);
          },
          [this](const SurviveWavesVictoryRule& wave_rule) {
            return m_mission_wave_query != nullptr &&
                   m_mission_wave_query->cleared_wave_count() >=
                       std::max(1, wave_rule.required_wave_count);
          },
          [this](const AccumulateResourcesVictoryRule& resource_rule) {
            return m_economy.has_harvested_at_least(m_local_owner_id,
                                                    resource_rule.required);
          },
          [this, &summary](const EliminateCommandersVictoryRule&) {
            return m_eliminate_commanders_armed && summary.enemy_commander_count == 0;
          }},
      rule);
}

auto VictoryService::seconds_until_deadline() const -> float {
  float shortest = -1.0F;
  for (const auto& condition : m_rule_set.defeat_rules) {
    if (const auto* rule = std::get_if<TimeLimitDefeatRule>(&condition.rule)) {
      const float remaining = std::max(0.0F, rule->duration - m_elapsed_time);
      if (shortest < 0.0F || remaining < shortest) {
        shortest = remaining;
      }
    }
  }
  return shortest;
}

auto VictoryService::check_defeat_rule(const DefeatRule& rule,
                                       const WorldSummary& summary) const -> bool {
  return std::visit(
      Overloaded{
          [&summary](const NoUnitsDefeatRule&) { return !summary.local_has_units; },
          [&summary](const NoCommanderDefeatRule&) {
            return summary.local_commander_count == 0;
          },
          [&summary](const NoKeyStructuresDefeatRule& no_structures_rule) {
            return count_matching_structures(summary.local_owned_structure_counts,
                                             no_structures_rule.structure_types) == 0;
          },
          [this,
           &summary](const OnlyCommanderRemainingDefeatRule& isolated_commander_rule) {
            if (!m_only_commander_defeat_armed) {
              return false;
            }
            return summary.local_commander_count > 0 &&
                   summary.local_non_commander_troop_count == 0 &&
                   count_matching_structures(summary.local_owned_structure_counts,
                                             isolated_commander_rule.structure_types) ==
                       0;
          },
          [this](const TimeLimitDefeatRule& time_limit_rule) {
            return m_elapsed_time >= time_limit_rule.duration;
          }},
      rule);
}

} // namespace Game::Systems
