#include "victory_service.h"

#include <QDebug>
#include <qglobal.h>

#include <algorithm>
#include <variant>

#include "core/event_manager.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems {

namespace {

constexpr float k_startup_delay_seconds = 0.35F;

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

auto build_rule_set_from_config(const Game::Map::VictoryConfig& config)
    -> VictoryRuleSet {
  VictoryRuleSet rules;

  QString const victory_type = config.victory_type.trimmed().toLower();
  if (victory_type == "elimination") {
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

  std::vector<QString> const default_defeat_structures =
      normalize_structure_types(config.key_structures);
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
    rules.defeat_rules.emplace_back(NoCommanderDefeatRule{});
    rules.defeat_rules.emplace_back(
        OnlyCommanderRemainingDefeatRule{{QStringLiteral("barracks")}});
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

VictoryService::VictoryService()
    : m_unit_spawned_subscription(
          [this](const Engine::Core::UnitSpawnedEvent& e) { on_unit_spawned(e); })
    , m_unit_died_subscription(
          [this](const Engine::Core::UnitDiedEvent& e) { on_unit_died(e); })
    , m_barrack_captured_subscription(
          [this](const Engine::Core::BarrackCapturedEvent& e) {
            on_barrack_captured(e);
          })
    , m_stats_registry(Game::Systems::GlobalStatsRegistry::instance())
    , m_owner_registry(Game::Systems::OwnerRegistry::instance()) {
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
  m_has_world_based_rules = false;
  m_requires_captured_structure_tracking = false;
  m_has_only_commander_defeat_rule = false;
  m_only_commander_defeat_armed = false;
  m_world_state_dirty = false;
  m_local_owner_id = 1;
  m_victory_state.clear();
  m_world_ptr = nullptr;
  m_victory_callback = nullptr;
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

  if (m_has_only_commander_defeat_rule && !m_only_commander_defeat_armed) {
    update_only_commander_defeat_arming(summarize_world(world));
  }

  if (m_startup_delay > 0.0F) {
    m_startup_delay = std::max(0.0F, m_startup_delay - delta_time);
    if (m_startup_delay > 0.0F) {
      return;
    }
    mark_world_dirty();
  }

  if (m_has_time_based_victory) {
    m_elapsed_time += delta_time;
    evaluate_time_based_victory();
    if (!m_victory_state.isEmpty()) {
      return;
    }
  }

  if (!m_has_world_based_rules || !m_world_state_dirty) {
    return;
  }

  evaluate_world_state(world);
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
  m_has_world_based_rules = false;
  m_requires_captured_structure_tracking = false;
  m_has_only_commander_defeat_rule = false;
  m_only_commander_structure_types.clear();

  for (const auto& rule : m_rule_set.victory_rules) {
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
            [this](const SurviveTimeVictoryRule&) {
              m_has_time_based_victory = true;
            }},
        rule);
  }

  for (const auto& rule : m_rule_set.defeat_rules) {
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
            }},
        rule);
  }
}

void VictoryService::update_only_commander_defeat_arming(const WorldSummary& summary) {
  if (m_only_commander_defeat_armed || !m_has_only_commander_defeat_rule) {
    return;
  }

  if (summary.local_non_commander_troop_count > 0 ||
      count_matching_structures(summary.local_owned_structure_counts,
                                m_only_commander_structure_types) > 0) {
    m_only_commander_defeat_armed = true;
  }
}

void VictoryService::evaluate_time_based_victory() {
  for (const auto& rule : m_rule_set.victory_rules) {
    const auto* survive_rule = std::get_if<SurviveTimeVictoryRule>(&rule);
    if (survive_rule == nullptr) {
      continue;
    }
    if (m_elapsed_time >= survive_rule->duration) {
      finalize_game(QStringLiteral("victory"));
      return;
    }
  }
}

void VictoryService::evaluate_world_state(Engine::Core::World& world) {
  WorldSummary const summary = summarize_world(world);
  update_only_commander_defeat_arming(summary);
  m_world_state_dirty = false;
  evaluate_world_summary(summary);
}

void VictoryService::evaluate_world_summary(const WorldSummary& summary) {
  for (const auto& rule : m_rule_set.victory_rules) {
    if (std::holds_alternative<SurviveTimeVictoryRule>(rule)) {
      continue;
    }
    if (check_victory_rule(rule, summary)) {
      finalize_game(QStringLiteral("victory"));
      return;
    }
  }

  for (const auto& rule : m_rule_set.defeat_rules) {
    if (check_defeat_rule(rule, summary)) {
      finalize_game(QStringLiteral("defeat"));
      return;
    }
  }
}

void VictoryService::finalize_game(const QString& state) {
  m_victory_state = state;
  qInfo() << (state == "victory" ? "VICTORY! Conditions met."
                                 : "DEFEAT! Condition met.");

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

  auto& nation_registry = Game::Systems::NationRegistry::instance();
  const auto* local_nation = nation_registry.get_nation_for_player(m_local_owner_id);
  Game::Systems::NationID const local_nation_id =
      (local_nation != nullptr) ? local_nation->id
                                : nation_registry.default_nation_id();

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : entities) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    bool const is_local_unit = (unit->owner_id == m_local_owner_id);
    if (is_local_unit) {
      summary.local_has_units = true;
      if (entity->get_component<Engine::Core::CommanderComponent>() != nullptr) {
        summary.local_commander_count += 1;
      } else if (Game::Units::is_troop_spawn(unit->spawn_type)) {
        summary.local_non_commander_troop_count += 1;
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
        auto* building = entity->get_component<Engine::Core::BuildingComponent>();
        if (building != nullptr && building->original_nation_id != local_nation_id) {
          summary.local_captured_structure_counts[unit_type] += 1;
        }
      }
      continue;
    }

    if (track_enemy_structures && m_tracked_enemy_structure_types.contains(unit_type) &&
        m_owner_registry.are_enemies(m_local_owner_id, unit->owner_id)) {
      summary.enemy_structure_counts[unit_type] += 1;
    }
  }

  return summary;
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
          }},
      rule);
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
          }},
      rule);
}

} // namespace Game::Systems
