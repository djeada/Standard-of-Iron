#pragma once

#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Core {
using EntityID = unsigned int;
}

namespace Game::Systems {
struct Nation;
}

namespace Game::Systems::AI {

enum class AIState {
  Idle,
  Gathering,
  Attacking,
  Defending,
  Retreating,
  Expanding
};

enum class AICommandType { MoveUnits, AttackTarget, StartProduction };

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

struct EntitySnapshot {
  Engine::Core::EntityID id = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
  int owner_id = 0;
  int health = 0;
  int max_health = 0;
  bool is_building = false;

  float posX = 0.0F;
  float posY = 0.0F;
  float posZ = 0.0F;

  MovementSnapshot movement;
  ProductionSnapshot production;
};

struct ContactSnapshot {
  Engine::Core::EntityID id = 0;
  bool is_building = false;
  int owner_id = 0;

  float posX = 0.0F;
  float posY = 0.0F;
  float posZ = 0.0F;

  int health = 0;
  int max_health = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
};

struct AISnapshot {
  int player_id = 0;
  std::vector<EntitySnapshot> friendly_units;
  std::vector<ContactSnapshot> visible_enemies;

  float game_time = 0.0F;
};

struct AIContext {
  int player_id = 0;
  AIState state = AIState::Idle;
  float state_timer = 0.0F;
  float decision_timer = 0.0F;

  const Game::Systems::Nation *nation = nullptr;

  std::vector<Engine::Core::EntityID> military_units;
  std::vector<Engine::Core::EntityID> buildings;
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

  struct UnitAssignment {
    BehaviorPriority owner_priority = BehaviorPriority::Normal;
    float assignment_time = 0.0F;
    std::string assigned_task;
  };
  std::unordered_map<Engine::Core::EntityID, UnitAssignment> assigned_units;

  int melee_count = 0;
  int ranged_count = 0;
  int damaged_units_count = 0;

  int visible_enemy_count = 0;
  int enemy_buildings_count = 0;
  int neutral_barracks_count = 0;
  float average_enemy_distance = 0.0F;

  int max_troops_per_player = 500;

  std::unordered_map<Engine::Core::EntityID, float> buildings_under_attack;

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
