#pragma once

#include <QString>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../../map/map_definition.h"
#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"
#include "../resource_types.h"

namespace Engine::Core {
using EntityID = std::uint64_t;
}

namespace Game::Systems {
struct Nation;
}

namespace Game::Systems::AI {

struct AIDoctrine;

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

enum class AIPosture {
  Field,
  Garrison
};

enum class AIState {
  Idle,
  Gathering,
  Attacking,
  Defending,
  Retreating,
  Expanding
};

enum class BaseRole {
  Main,
  Production,
  Defensive,
  Forward
};

enum class AICommandType {
  MoveUnits,
  AttackTarget,
  StartProduction,
  SetRallyPoint,
  StartBuilderConstruction,
  StartBuilderHarvest,
  StartBuilderRepair,
  DivideSquads,
  MergeSquads,
  SetAutoGather,
  TradeResource,
  DeliverCivilians,
  TriggerCommanderRally,
  TriggerCommanderAura
};

struct ResourceNodeSnapshot {
  std::uint64_t id = 0;
  Game::Map::WorldProp::Type type = Game::Map::WorldProp::Type::PineTree;
  float pos_x = 0.0F;
  float pos_z = 0.0F;
  bool reserved = false;
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

  int manpower_available = 0;
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
  bool has_task_target = false;

  Engine::Core::EntityID task_target_id = 0;

  bool carrying_load = false;
  bool auto_gather = false;
  float construction_site_x = 0.0F;
  float construction_site_z = 0.0F;

  bool raising_a_building = false;
  Game::Units::SpawnType building_under_way = Game::Units::SpawnType::Barracks;
};

struct EntitySnapshot {
  Engine::Core::EntityID id = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
  int owner_id = 0;
  int health = 0;
  int max_health = 0;
  bool is_building = false;
  bool is_commander = false;

  int squad_strength = 0;
  int squad_establishment = 1;
  bool is_assault = false;
  bool engagement_resolved = false;
  bool engaged = false;

  float pos_x = 0.0F;
  float pos_y = 0.0F;
  float pos_z = 0.0F;

  bool has_march_target = false;
  float march_target_x = 0.0F;
  float march_target_z = 0.0F;

  bool has_delivery_order = false;

  bool crop_is_ripe = false;

  MovementSnapshot movement;
  ProductionSnapshot production;
  BuilderProductionSnapshot builder_production;
};

struct ContactSnapshot {
  Engine::Core::EntityID id = 0;
  bool is_building = false;
  int owner_id = 0;

  bool holds_ground = false;

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
  std::vector<ResourceNodeSnapshot> resource_nodes;
  ResourceAmounts resources;
  bool has_resource_snapshot = false;

  std::shared_ptr<const Game::Systems::Nation> nation;
  int max_troops_per_player = 0;

  bool has_map_bounds = false;
  float map_min_x = 0.0F;
  float map_max_x = 0.0F;
  float map_min_z = 0.0F;
  float map_max_z = 0.0F;

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
  AIPosture posture = AIPosture::Field;
  PersonalityInputs personality;
  DifficultyTuning difficulty;

  const AIDoctrine* doctrine = nullptr;

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
  int max_local_responders = 0;
  bool full_recall_on_base_threat = false;
  int desired_outpost_barracks_count = 0;
  int outpost_home_target = 0;
  float assembly_radius = 10.0F;
  float gather_spacing = 1.4F;
  float attack_formation_spacing = 2.5F;
  float scouting_distance = 40.0F;
  float reserve_hold_radius = 8.0F;
  float expansion_site_distance = 28.0F;
  float local_response_radius = 0.0F;
};

struct AIPlayerProfile {
  AIStrategy strategy = AIStrategy::Balanced;
  AIPosture posture = AIPosture::Field;
  AIStrategyConfig::PersonalityInputs personality;
  QString difficulty;
  const AIDoctrine* doctrine = nullptr;
};

struct AIBase {
  int id = 0;
  BaseRole role = BaseRole::Main;

  float center_x = 0.0F;
  float center_z = 0.0F;
  float rally_x = 0.0F;
  float rally_z = 0.0F;

  Engine::Core::EntityID primary_barracks = 0;
  std::vector<Engine::Core::EntityID> buildings;
  std::vector<Engine::Core::EntityID> production_buildings;

  int barracks_count = 0;
  int home_count = 0;
  int farm_count = 0;
  int defense_tower_count = 0;
  int queued_production = 0;
  int production_capacity = 0;

  int nearby_threat_count = 0;
  bool under_threat = false;
  float last_threat_time = -1000.0F;
};

struct ForwardBasePlan {
  bool has_site = false;
  float site_x = 0.0F;
  float site_z = 0.0F;
  int failed_attempts = 0;
  int abandoned_count = 0;
  float attempt_deadline = 0.0F;
  bool attempt_in_flight = false;
};

struct AbandonedSite {
  float x = 0.0F;
  float z = 0.0F;
  float time = 0.0F;
};

struct AIContext {
  int player_id = 0;
  AIState state = AIState::Idle;
  float state_timer = 0.0F;
  float decision_timer = 0.0F;

  const Game::Systems::Nation* nation = nullptr;

  AIStrategyConfig strategy_config;

  std::vector<Engine::Core::EntityID> buildings;
  std::vector<Engine::Core::EntityID> commander_ids;
  Engine::Core::EntityID primary_barracks = 0;

  float rally_x = 0.0F;
  float rally_z = 0.0F;

  int total_units = 0;
  int idle_units = 0;
  int combat_units = 0;
  float average_health = 1.0F;
  bool barracks_under_threat = false;
  int nearby_threat_count = 0;

  float base_pos_x = 0.0F;
  float base_pos_y = 0.0F;
  float base_pos_z = 0.0F;
  bool has_base_anchor = false;

  float settlement_facing_x = 0.0F;
  float settlement_facing_z = -1.0F;
  bool settlement_facing_locked = false;
  bool anchor_is_structural = false;
  bool has_expansion_site = false;
  float expansion_site_x = 0.0F;
  float expansion_site_z = 0.0F;

  struct UnitAssignment {
    BehaviorPriority owner_priority = BehaviorPriority::Normal;
    float assignment_time = 0.0F;
    const char* assigned_task = "";
    int base_id = 0;
  };
  std::unordered_map<Engine::Core::EntityID, UnitAssignment> assigned_units;

  std::vector<AIBase> bases;
  std::vector<AbandonedSite> abandoned_expansion_sites;
  ForwardBasePlan forward_plan;
  int main_base_id = 0;
  int forward_base_id = 0;
  int next_base_id = 1;
  int reassigned_units_last_update = 0;

  int melee_count = 0;
  int ranged_count = 0;
  int cavalry_count = 0;
  int siege_count = 0;
  int builder_count = 0;

  int civilian_count = 0;
  int damaged_units_count = 0;

  int visible_enemy_count = 0;
  int neutral_barracks_count = 0;

  int home_count = 0;
  int farm_count = 0;
  int defense_tower_count = 0;
  int wall_segment_count = 0;
  int barracks_count = 0;
  int marketplace_count = 0;
  int assembled_unit_count = 0;

  int recruitment_manpower_available = 0;
  int home_civilians_remaining = 0;
  int effective_reserve_units = 0;
  int effective_harass_units = 0;
  int outpost_barracks_count = 0;
  int outpost_home_count = 0;
  std::vector<Engine::Core::EntityID> reserve_unit_ids;
  std::vector<Engine::Core::EntityID> harass_unit_ids;
  std::vector<Engine::Core::EntityID> assault_unit_ids;
  int assault_unit_count = 0;
  bool any_base_under_threat = false;

  int max_troops_per_player = 500;

  int population_used = 0;
  int population_cap = 0;

  [[nodiscard]] auto population_headroom() const -> int {
    return population_cap > 0 ? std::max(0, population_cap - population_used) : 9999;
  }
  bool expansion_construction_pending = false;
  float last_expansion_order_time = -1000.0F;

  std::unordered_map<Engine::Core::EntityID, float> buildings_under_attack;
  float last_local_threat_time = 0.0F;

  struct MacroTargets {
    int builder_count = 3;
    int home_count = 2;
    int barracks_count = 1;
    int marketplace_count = 0;
    int farm_count = 0;
    int defense_tower_count = 1;
    int wall_segment_count = 0;
    int catapult_count = 0;
    int assembly_size = 4;
    float assembly_radius = 10.0F;
    float gather_spacing = 1.4F;

    bool raise_homes_first = false;
  };
  MacroTargets macro_targets;

  struct AttackWave {
    std::vector<Engine::Core::EntityID> members;
    Engine::Core::EntityID target_id = 0;
    float target_x = 0.0F;
    float target_z = 0.0F;
    bool committed = false;
    int initial_size = 0;
    float committed_at = -1000.0F;
    float ended_at = -1000.0F;
    float last_order_time = -1000.0F;
  };
  AttackWave wave;

  std::vector<Engine::Core::EntityID> garrison_unit_ids;

  int consecutive_no_progress_cycles = 0;
  float last_meaningful_action_time = 0.0F;
  float max_state_duration = 60.0F;
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
  float construction_rotation_y = 0.0F;
  std::uint64_t resource_target_id = 0;

  float rally_x = 0.0F;
  float rally_z = 0.0F;

  Game::Systems::ResourceType trade_resource = Game::Systems::ResourceType::Wood;
  bool trade_is_purchase = true;
  bool auto_gather_active = true;
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
