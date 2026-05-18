#pragma once

#include <QHash>
#include <QSet>
#include <QString>

#include <functional>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {
struct VictoryConfig;
}

namespace Game::Systems {

class GlobalStatsRegistry;
class OwnerRegistry;

struct StructureRequirement {
  std::vector<QString> structure_types;
  int required_count = 1;
};

struct EliminationVictoryRule {
  std::vector<QString> structure_types;
};

struct SurviveTimeVictoryRule {
  float duration = 0.0F;
};

struct ControlStructuresVictoryRule {
  StructureRequirement target;
};

struct CaptureStructuresVictoryRule {
  StructureRequirement target;
};

using VictoryRule = std::variant<EliminationVictoryRule,
                                 SurviveTimeVictoryRule,
                                 ControlStructuresVictoryRule,
                                 CaptureStructuresVictoryRule>;

struct NoUnitsDefeatRule {};

struct NoKeyStructuresDefeatRule {
  std::vector<QString> structure_types;
};

struct NoCommanderDefeatRule {};

struct OnlyCommanderRemainingDefeatRule {
  std::vector<QString> structure_types;
};

using DefeatRule = std::variant<NoUnitsDefeatRule,
                                NoKeyStructuresDefeatRule,
                                NoCommanderDefeatRule,
                                OnlyCommanderRemainingDefeatRule>;

struct VictoryRuleSet {
  std::vector<VictoryRule> victory_rules;
  std::vector<DefeatRule> defeat_rules;
};

class VictoryService {
public:
  VictoryService();
  ~VictoryService();

  void configure(const Game::Map::VictoryConfig& config, int local_owner_id);
  void configure(const VictoryRuleSet& rules, int local_owner_id);

  void reset();

  void update(Engine::Core::World& world, float delta_time);

  [[nodiscard]] auto get_victory_state() const -> QString { return m_victory_state; }

  [[nodiscard]] auto is_game_over() const -> bool { return !m_victory_state.isEmpty(); }

  using VictoryCallback = std::function<void(const QString& state)>;
  void set_victory_callback(VictoryCallback callback) {
    m_victory_callback = std::move(callback);
  }

private:
  struct WorldSummary {
    bool local_has_units = false;
    int local_commander_count = 0;
    int local_non_commander_troop_count = 0;
    QHash<QString, int> enemy_structure_counts;
    QHash<QString, int> local_owned_structure_counts;
    QHash<QString, int> local_captured_structure_counts;
  };

  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event);
  void on_unit_died(const Engine::Core::UnitDiedEvent& event);
  void on_barrack_captured(const Engine::Core::BarrackCapturedEvent& event);
  void mark_world_dirty();
  void reevaluate_world_state();
  void refresh_rule_metadata();
  void update_only_commander_defeat_arming(const WorldSummary& summary);
  void evaluate_time_based_victory();
  void evaluate_world_state(Engine::Core::World& world);
  void evaluate_world_summary(const WorldSummary& summary);
  void finalize_game(const QString& state);

  [[nodiscard]] auto can_evaluate() const -> bool;
  [[nodiscard]] auto summarize_world(Engine::Core::World& world) const -> WorldSummary;
  [[nodiscard]] auto check_victory_rule(const VictoryRule& rule,
                                        const WorldSummary& summary) const -> bool;
  [[nodiscard]] auto check_defeat_rule(const DefeatRule& rule,
                                       const WorldSummary& summary) const -> bool;

  VictoryRuleSet m_rule_set;
  QSet<QString> m_tracked_enemy_structure_types;
  QSet<QString> m_tracked_local_structure_types;
  float m_elapsed_time = 0.0F;
  float m_startup_delay = 0.0F;
  bool m_has_time_based_victory = false;
  bool m_has_world_based_rules = false;
  bool m_requires_captured_structure_tracking = false;
  bool m_has_only_commander_defeat_rule = false;
  bool m_only_commander_defeat_armed = false;
  bool m_world_state_dirty = false;
  std::vector<QString> m_only_commander_structure_types;

  int m_local_owner_id = 1;
  QString m_victory_state;

  VictoryCallback m_victory_callback;

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_barrack_captured_subscription;

  Engine::Core::World* m_world_ptr = nullptr;

  Game::Systems::GlobalStatsRegistry& m_stats_registry;
  Game::Systems::OwnerRegistry& m_owner_registry;
};

} // namespace Game::Systems
