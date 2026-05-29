#pragma once

#include <QString>

#include <unordered_map>
#include <vector>

#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"

namespace Engine::Core {
using EntityID = unsigned int;
}

namespace Game::Systems {
struct Nation;
}

namespace Game::Systems::AI {

enum class AIStrategy {
  Balanced,
  Aggressive,
  Defensive,
  Expansionist,
  Economic,
  Harasser,
  Rusher,
  SepulcherDefense
};

enum class AIState {
  Idle,
  Gathering,
  Attacking,
  Defending,
  Retreating,
  Expanding
};

enum class AICommandType {
  MoveUnits,
  AttackTarget,
  StartProduction,
  StartBuilderConstruction,
  TriggerCommanderRally,
  TriggerCommanderAura
};

enum class BehaviorPriority {
  VeryLow = 0,
  Low = 1,
  Normal = 2,
  High = 3,
  Critical = 4
};

struct MovementSnapshot {
  bool has_component = false;
  bool has_target = false;
};

struct ProductionSnapshot {
  bool has_component = false;
  bool in_progress = false;
  float build_time = 0.0F;
  float time_remaining = 0.0F;
  int produced_count = 0;
  int max_units = 0;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;
  bool rally_set = false;
  float rally_x = 0.0F;
  float rally_z = 0.0F;
  int queue_size = 0;
};

struct BuilderProductionSnapshot {
  bool has_component = false;
  bool has_construction_site = false;
  bool in_progress = false;
  bool at_construction_site = false;
  float construction_site_x = 0.0F;
  float construction_site_z = 0.0F;
};

struct EntitySnapshot {
  Engine::Core::EntityID id = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
  int owner_id = 0;
  int health = 0;
  int max_health = 0;
  bool is_building = false;
  bool is_commander = false;

  float pos_x = 0.0F;
  float pos_y = 0.0F;
  float pos_z = 0.0F;

  MovementSnapshot movement;
  ProductionSnapshot production;
  BuilderProductionSnapshot builder_production;
};

struct ContactSnapshot {
  Engine::Core::EntityID id = 0;
  bool is_building = false;
  int owner_id = 0;

  float pos_x = 0.0F;
  float pos_y = 0.0F;
  float pos_z = 0.0F;

  int health = 0;
  int max_health = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
};

struct AISnapshot {
  int player_id = 0;
  std::vector<EntitySnapshot> friendly_units;
  std::vector<ContactSnapshot> visible_enemies;
  std::vector<ContactSnapshot> strategic_objectives;
  std::vector<ContactSnapshot> defense_anchors;

  float game_time = 0.0F;
};

struct AIStrategyConfig {
  struct PersonalityInputs {
    float aggression = 0.5F;
    float defense = 0.5F;
    float harassment = 0.5F;
  };

  struct DifficultyTuning {
    QString level = "normal";
    float update_interval_multiplier = 1.0F;
    float production_rate_multiplier = 1.0F;
    float scouting_distance_multiplier = 1.0F;
  };

  AIStrategy strategy = AIStrategy::Balanced;
  PersonalityInputs personality;
  DifficultyTuning difficulty;

  float aggression_modifier = 1.0F;
  float defense_modifier = 1.0F;
  float expansion_priority = 1.0F;
  float production_rate_modifier = 1.0F;
  float min_attack_force = 1.0F;
  float retreat_threshold = 0.25F;
  float harassment_range = 0.0F;
  int target_builder_count = 3;
  int base_home_target = 2;
  int desired_barracks_count = 1;
  int desired_defense_tower_count = 1;
  int desired_wall_segment_count = 0;
  int desired_catapult_count = 0;
  int desired_assembly_size = 4;
  int reactive_attack_size = 2;
  int proactive_attack_size = 4;
  int reserve_units = 0;
  int harass_units = 0;
  int desired_outpost_barracks_count = 0;
  int outpost_home_target = 0;
  float assembly_radius = 10.0F;
  float gather_spacing = 1.4F;
  float attack_formation_spacing = 2.5F;
  float scouting_distance = 40.0F;
  float reserve_hold_radius = 8.0F;
  float expansion_site_distance = 28.0F;
};

struct AIContext {
  int player_id = 0;
  AIState state = AIState::Idle;
  float state_timer = 0.0F;
  float decision_timer = 0.0F;

  const Game::Systems::Nation* nation = nullptr;

  AIStrategyConfig strategy_config;

  std::vector<Engine::Core::EntityID> military_units;
  std::vector<Engine::Core::EntityID> buildings;
  std::vector<Engine::Core::EntityID> commander_ids;
  Engine::Core::EntityID primary_barracks = 0;

  float rally_x = 0.0F;
  float rally_z = 0.0F;
  int target_priority = 0;

  int total_units = 0;
  int idle_units = 0;
  int combat_units = 0;
  float average_health = 1.0F;
  bool barracks_under_threat = false;
  int nearby_threat_count = 0;
  float closest_threat_distance = 0.0F;

  float base_pos_x = 0.0F;
  float base_pos_y = 0.0F;
  float base_pos_z = 0.0F;
  bool has_base_anchor = false;
  bool anchor_is_structural = false;
  bool has_expansion_site = false;
  float expansion_site_x = 0.0F;
  float expansion_site_z = 0.0F;

  struct UnitAssignment {
    BehaviorPriority owner_priority = BehaviorPriority::Normal;
    float assignment_time = 0.0F;
    const char* assigned_task = "";
  };
  std::unordered_map<Engine::Core::EntityID, UnitAssignment> assigned_units;

  int melee_count = 0;
  int ranged_count = 0;
  int builder_count = 0;
  int damaged_units_count = 0;

  int visible_enemy_count = 0;
  int enemy_buildings_count = 0;
  int neutral_barracks_count = 0;
  float average_enemy_distance = 0.0F;

  int home_count = 0;
  int defense_tower_count = 0;
  int wall_segment_count = 0;
  int barracks_count = 0;
  int marketplace_count = 0;
  int assembled_unit_count = 0;
  int effective_reserve_units = 0;
  int effective_harass_units = 0;
  int outpost_barracks_count = 0;
  int outpost_home_count = 0;
  std::vector<Engine::Core::EntityID> reserve_unit_ids;
  std::vector<Engine::Core::EntityID> harass_unit_ids;

  int max_troops_per_player = 500;
  bool expansion_construction_pending = false;
  float last_expansion_order_time = -1000.0F;

  std::unordered_map<Engine::Core::EntityID, float> buildings_under_attack;
  float last_local_threat_time = 0.0F;

  struct MacroTargets {
    int builder_count = 3;
    int home_count = 2;
    int barracks_count = 1;
    int marketplace_count = 0;
    int defense_tower_count = 1;
    int wall_segment_count = 0;
    int catapult_count = 0;
    int assembly_size = 4;
    float assembly_radius = 10.0F;
    float gather_spacing = 1.4F;
  };
  MacroTargets macro_targets;

  int consecutive_no_progress_cycles = 0;
  float last_meaningful_action_time = 0.0F;
  int last_total_units = 0;
  float max_state_duration = 60.0F;

  struct DebugInfo {
    int total_decisions_made = 0;
    int total_commands_issued = 0;
    int state_transitions = 0;
    int deadlock_recoveries = 0;
    float last_update_time = 0.0F;
    float total_cpu_time = 0.0F;
  };
  DebugInfo debug_info;
};

struct AICommand {
  AICommandType type = AICommandType::MoveUnits;
  std::vector<Engine::Core::EntityID> units;

  std::vector<float> move_target_x;
  std::vector<float> move_target_y;
  std::vector<float> move_target_z;

  Engine::Core::EntityID target_id = 0;
  bool should_chase = false;
  Engine::Core::EntityID building_id = 0;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;

  const char* construction_type = nullptr;
  float construction_site_x = 0.0F;
  float construction_site_z = 0.0F;
};

struct AIResult {
  AIContext context;
  std::vector<AICommand> commands;
};

struct AIJob {
  AISnapshot snapshot;
  AIContext context;
  float delta_time = 0.0F;
};

} // namespace Game::Systems::AI
