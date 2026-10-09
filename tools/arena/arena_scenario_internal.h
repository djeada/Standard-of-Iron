#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <utility>

#include "arena_scenario.h"
#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/core/component.h"
#include "game/core/death_sequence.h"
#include "game/core/world.h"
#include "game/formation/army_formation_service.h"
#include "game/map/terrain_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/attack_range.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_system/damage_application.h"
#include "game/systems/combat_system/engagement_trace.h"
#include "game/systems/combat_system/mounted_charge_processor.h"
#include "game/systems/combat_system/structure_combat.h"
#include "game/systems/combat_system/structure_fire.h"
#include "game/systems/defensive_unit_layout_service.h"
#include "game/systems/economy/construction_cost_catalog.h"
#include "game/systems/economy/food_targets.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/movement/command_service.h"
#include "game/systems/movement/order_service.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/projectile_kind.h"
#include "game/systems/projectile_system.h"
#include "game/systems/raft_system.h"
#include "game/systems/rockfall_system.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/unit.h"
#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_species.h"
#include "render/graphics_settings.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/frame_profile.h"
#include "render/profiling/performance_report.h"

namespace Arena {

namespace scenario_internal {

auto shortest_degrees(float to_degrees, float from_degrees) -> float;

auto vector_from_transform(const Engine::Core::TransformComponent& transform)
    -> QVector3D;

auto horizontal_distance(const QVector3D& lhs, const QVector3D& rhs) -> float;

} // namespace scenario_internal

struct ArenaScenarioRunner::Impl {
  struct StepRuntime {
    bool executed{false};
    float executed_at{0.0F};
  };
  struct CommandResponse {
    float issued_at{0.0F};
    float deadline{0.0F};
    QVector3D initial_position;
    float initial_yaw{0.0F};
    QString command;
    bool observed{false};
    bool reported{false};
  };
  struct EntityState {
    QVector3D position;
    float observed_at{0.0F};
    int health{0};
    float yaw{0.0F};
    Engine::Core::EntityID melee_lock_target_id{0};
    bool melee_lock{false};
    bool initialized{false};
  };
  struct MotionQualityState {
    QVector3D position;
    QVector3D last_step;
    float yaw{0.0F};
    float last_yaw_step{0.0F};
    float observed_at{0.0F};
    int fast_rotation_frames{0};
    std::vector<float> facing_reversals;
    std::vector<float> position_reversals;
    bool initialized{false};
  };
  struct SoldierState {
    QVector3D root_position;
    QVector3D hand_l_world;
    QVector3D hand_r_world;
    QVector3D foot_l_world;
    QVector3D foot_r_world;
    float pelvis_yaw_degrees{0.0F};
    float locomotion_presence{0.0F};
    float cycle_phase{0.0F};
    float attack_phase{0.0F};
    float attack_exit_phase{0.0F};
    float attack_exit_at{-1.0F};
    bool attack_exit_is_melee{false};
    bool attack_is_melee{false};
    bool hit_since_attack_exit{false};
    bool walked_since_attack_exit{false};
    std::array<QVector3D, 4> local_joints{};
    std::array<float, 4> local_joint_steps{};
    bool joints_valid{false};
    bool dying{false};
    float attack_pelvis_yaw_min{0.0F};
    float attack_pelvis_yaw_max{0.0F};
    bool attack_yaw_tracked{false};
    float observed_at{0.0F};
    float fight_idle_since{-1.0F};
    float terminal_pose_since{-1.0F};
    bool initialized{false};
    bool culled{false};
    bool attacking{false};
    bool completed_attack_phase{false};
    bool ever_visible{false};
    bool alive{false};
  };
  struct TraceUnit {
    Engine::Core::EntityID entity_id{0};
    QString group;
    QVector3D position;
    int health{0};
    Engine::Core::EntityID target_id{0};
    QString motion;
    QString combat_mode;
    int mounted_charge_state{-1};
    int mounted_charge_cancel_reason{-1};
    int combat_action_id{0};
    bool melee_lock{false};
    Engine::Core::EntityID melee_lock_target_id{0};
    bool combat_indicator_submitted{false};
    float yaw{0.0F};
    bool movement_target{false};
    float movement_vx{0.0F};
    float movement_vz{0.0F};
    float movement_goal_x{0.0F};
    float movement_goal_z{0.0F};
    bool formation_contact{false};
    float formation_surface_gap{0.0F};
    std::vector<std::uint16_t> engaged_soldiers;
    std::vector<Engine::Core::FormationEngagementPair> engagement_pairs;
    QString construction_type;
    bool construction_site{false};
    bool construction_in_progress{false};
    float construction_time_remaining{0.0F};
    bool commander_aura_active{false};
    bool commander_aura_buffed{false};
    int rpg_health{-1};
    bool rpg_guard_active{false};
    bool rpg_dodge_grace{false};
    Engine::Core::EntityID rpg_aim_target_id{0};
    int rpg_aim_soldier_slot{-1};
    int rpg_action_phase{0};
    float rpg_action_normalized_time{0.0F};
    Engine::Core::EntityID engagement_candidate_id{0};
    Engine::Core::EntityID engagement_target_id{0};
    float engagement_range{0.0F};
    QString engagement_reason;
    QString command_source;
  };
  struct TraceSoldier {
    Engine::Core::EntityID entity_id{0};
    int soldier_index{0};
    QVector3D root_position;
    float root_yaw_degrees{0.0F};
    float root_up_y{1.0F};
    float submitted_body_up_y{1.0F};
    float submitted_max_arm_reach{0.0F};
    bool submitted_body_pose_valid{false};
    QVector3D foot_l_world;
    QVector3D foot_r_world;
    QVector3D hand_l_world;
    QVector3D hand_r_world;
    float locomotion_blend{0.0F};
    float locomotion_presence{0.0F};
    float cycle_phase{0.0F};
    float travel_alignment{1.0F};
    float travel_lateral_share{0.0F};
    float action_link_weight{0.0F};
    bool persistent_valid{false};
    float sample_time{0.0F};
    float persistent_last_sample_time{0.0F};
    QString declared_action;
    int declared_target_slot{-1};
    float declared_surface_gap{0.0F};
    QString animation;
    QString visual;
    bool swing_recoil{false};
    float hit_reaction_tilt_degrees{0.0F};
    float attack_phase{0.0F};
    std::uint32_t transitions{0};
    bool culled{false};
    QString cull_reason;
    int lod{0};
    float pelvis_yaw_degrees{0.0F};
    float torso_yaw_degrees{0.0F};
    bool attack_is_melee{false};
  };
  struct TraceAnimal {
    Engine::Core::EntityID entity_id{0};
    QVector3D position;
    int health{0};
    QString species;
    QString behavior;
    Engine::Core::EntityID focus_id{0};
    float yaw{0.0F};
    float desired_yaw{0.0F};
    bool has_desired_yaw{false};
    float vx{0.0F};
    float vz{0.0F};
    bool biting{false};
    float bite_phase{-1.0F};
    float flinch_phase{-1.0F};
    Engine::Core::EntityID bite_target_id{0};
    bool impact_pending{false};
    bool dying{false};
    bool melee_lock{false};
    bool has_move_target{false};
    float goal_x{0.0F};
    float goal_z{0.0F};
    float state_timer{0.0F};
    float stall_timer{0.0F};
    bool staggered{false};
  };
  struct TraceFrame {
    float time_seconds{0.0F};
    float animation_time{0.0F};
    double frame_time_ms{0.0};
    ArenaRenderedFrameTimings timings;
    std::vector<TraceUnit> units;
    std::vector<TraceSoldier> soldiers;
    std::vector<TraceAnimal> animals;
    App::Core::CommanderPresentationTrace commander;
  };
  struct TravelObservation {
    bool has_start{false};
    bool has_end{false};
    QVector3D start;
    QVector3D end;
  };
  struct BridgeAlignmentObservation {
    bool sampled{false};
    float midpoint_distance{std::numeric_limits<float>::infinity()};
    float lateral_offset{std::numeric_limits<float>::infinity()};
  };
  struct BattleSideState {
    int owner_id{0};
    QString label;
    QVector3D home;
    QVector3D enemy_home;
    bool has_axis{false};
    float separation{0.0F};
    int living_units{0};
    int living_soldiers{0};
    int living_buildings{0};
    int peak_units{0};
    int peak_soldiers{0};
    int initial_units{0};
    int initial_soldiers{0};
    float peak_advance{0.0F};
    float final_advance{0.0F};
    bool has_advance{false};
    std::vector<std::pair<float, float>> advance_samples;
    QString strategy;
    QString posture;
    float seconds_attacking{0.0F};
    float seconds_observed{0.0F};
    float home_radius{16.0F};
    QSet<Engine::Core::EntityID> initial_buildings;
    QSet<Engine::Core::EntityID> seen_buildings;
    QHash<QString, int> building_census;
    int peak_buildings{0};
    int peak_home_units{0};
    int peak_forward_units{0};
    double home_share_sum{0.0};
    int home_share_samples{0};
    float eliminated_at{-1.0F};
    bool had_presence{false};
    QSet<Engine::Core::EntityID> seen_units;
    QString ai_state;
    bool wave_committed{false};
    int wave_size{0};
  };
  struct UndeadZoneObservation {
    int spawned_total{0};
    int peak_alive{0};
    int alive{0};
    float first_spawn_at{-1.0F};
    bool shrine_seen{false};
    bool shrine_standing{false};
    bool shrine_destroyed{false};
  };
  Engine::Core::World& world;
  ArenaScenarioHost host;
  ArenaScenarioDefinition scenario;
  QVector3D world_origin;
  QHash<QString, std::vector<Engine::Core::EntityID>> groups;
  QHash<QString, QVector3D> formed_destinations;
  QHash<Engine::Core::EntityID, QString> entity_groups;
  std::vector<StepRuntime> steps;
  QHash<Engine::Core::EntityID, CommandResponse> responses;
  QHash<Engine::Core::EntityID, EntityState> entity_states;
  QHash<Engine::Core::EntityID, MotionQualityState> motion_states;
  QHash<std::uint64_t, SoldierState> soldier_states;
  QHash<Engine::Core::EntityID, QSet<int>> sampled_soldiers_by_entity;
  QSet<Engine::Core::EntityID> entities_with_render_samples;
  QHash<Engine::Core::EntityID, float> idle_since;
  QHash<QString, bool> visible_attacks;
  QHash<QString, bool> visible_movement;
  QHash<QString, QString> building_overlap_report;
  QHash<QString, bool> visible_attack_recoveries;
  QHash<QString, bool> visible_hit_reactions;
  QHash<QString, bool> visible_deaths;
  QHash<QString, bool> launched_casualties;
  QHash<QString, bool> charge_impacts;
  QHash<QString, bool> melee_locks_after_charge;
  QSet<Engine::Core::EntityID> auto_engaged_entities;
  QSet<Engine::Core::EntityID> reported_engagement_windows;
  QHash<QString, bool> paired_visible_attacks;
  QHash<QString, bool> projectile_flights;
  QHash<QString, bool> flaming_projectile_flights;
  QHash<QString, bool> plain_projectile_flights;
  QHash<QString, bool> structure_fires;
  QHash<QString, bool> structure_collapses;
  QHash<QString, bool> structure_repairs;
  QHash<QString, bool> structure_dismantles;
  QHash<QString, bool> projectile_contacts;
  QHash<QString, bool> projectile_impacts;
  QSet<std::uint64_t> observed_projectile_impacts;
  QHash<QString, QSet<std::uint64_t>> living_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> engaged_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> attacking_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> guarding_soldiers_by_group;
  QHash<std::uint64_t, int> attack_entries_by_soldier;
  QHash<QString, bool> staggered_attack_phases;
  QHash<QString, bool> damage_seen;
  QHash<QString, int> initial_health_by_group;
  QHash<QString, bool> structure_damage_cues;
  QHash<QString, bool> structure_facade_contacts;
  QHash<QString, float> minimum_formation_surface_gap;
  QHash<QString, bool> bridge_traversal_seen;
  QHash<QString, bool> gate_seen;
  QHash<QString, bool> gate_opened_seen;
  QHash<QString, bool> tower_docked_seen;
  bool wall_walker_seen{false};
  QHash<QString, QSet<std::uint64_t>> raft_riders_by_group;
  int most_raft_riders{0};
  QHash<QString, QSet<std::uint64_t>> waders_by_group;
  QHash<QString, BridgeAlignmentObservation> bridge_alignment;
  QHash<QString, float> initial_elevation;
  QHash<QString, float> maximum_elevation;
  struct ElevationLegState {
    bool seeded{false};
    float extreme{0.0F};
    float worst_reversal{0.0F};
    float worst_at{0.0F};
    bool suspended{false};
  };
  QHash<QString, ElevationLegState> elevation_climb_legs;
  QHash<QString, ElevationLegState> elevation_descent_legs;
  struct ElevationFloorState {
    bool seeded{false};
    float lowest{0.0F};
    float lowest_at{0.0F};
    QVector3D lowest_where;
  };
  QHash<QString, ElevationFloorState> elevation_floors;
  struct OffGroundState {
    int samples{0};
    QVector3D worst;
    float worst_at{0.0F};
  };
  QHash<QString, OffGroundState> off_walkable_ground;
  QHash<QString, OffGroundState> off_walkable_soldiers;
  struct StallObservation {
    float worst_stalled_seconds{0.0F};
    float worst_stalled_at{0.0F};
    std::uint32_t recovery_attempts{0};
    std::uint32_t repaths{0};
    std::uint32_t abandons{0};
    bool recovery_seen{false};
    QString worst_state;
    bool has_objective{false};
    float objective_x{0.0F};
    float objective_z{0.0F};
  };
  QHash<QString, StallObservation> stall_observations;
  struct NarrowLayoutState {
    bool seeded{false};
    bool engaged{false};
    float formation_half_width{0.0F};
    float narrowest_corridor{0.0F};
    std::uint32_t normal_files{0};
    std::uint32_t narrowest_files{0};
    float tightest_file_spacing{0.0F};
    float narrowest_frontage{0.0F};
    float deepest_column{0.0F};
    float engaged_from{-1.0F};
    float engaged_until{-1.0F};
    Engine::Core::TraversalLayoutMode narrowest_mode{
        Engine::Core::TraversalLayoutMode::Normal};
    Engine::Core::TraversalLayoutMode previous_mode{
        Engine::Core::TraversalLayoutMode::Normal};
    std::uint32_t mode_changes{0};
    float worst_reform_error{0.0F};
    bool active_at_end{false};
    std::uint32_t files_at_end{0};
  };
  QHash<QString, NarrowLayoutState> narrow_layout;
  QSet<QString> narrow_layout_groups;
  bool narrow_layout_groups_resolved{false};
  QHash<QString, bool> defensive_layout_locked;
  QHash<QString, bool> useful_bot_action;
  std::vector<BattleSideState> battle_sides;
  bool battle_decided{false};
  float battle_decided_at{-1.0F};
  QHash<QString, UndeadZoneObservation> undead_zone_states;
  QHash<QString, QSet<Engine::Core::EntityID>> undead_zone_entities;
  QHash<QString, float> range_ring_max_radius;
  QHash<QString, float> range_ring_min_radius;
  std::size_t max_range_ring_count{0};
  QHash<QString, bool> commander_aura_active_seen;
  QHash<QString, bool> commander_aura_expired_seen;
  QHash<QString, bool> commander_aura_buff_seen;
  QHash<QString, bool> exact_rpg_target_seen;
  QHash<QString, bool> rpg_damage_contact_seen;
  QHash<QString, bool> rpg_block_contact_seen;
  QHash<QString, bool> rpg_dodge_contact_seen;
  QHash<QString, bool> rpg_dodge_window_seen;
  QHash<QString, int> initial_rpg_health_by_group;
  QHash<QString, int> minimum_rpg_health_by_group;
  QHash<QString, bool> rpg_walk_seen;
  QHash<QString, bool> rpg_run_seen;
  QHash<QString, QString> rpg_locomotion_mismatch;
  QHash<QString, QString> rpg_strike_mismatch;
  QHash<QString, float> rpg_strike_mismatch_since;
  QHash<QString, int> rpg_action_phase_previous;
  QHash<QString, float> rpg_action_time_previous;
  QHash<QString, std::vector<float>> rpg_swing_starts;
  QHash<QString, std::vector<float>> rpg_swing_carry;
  QHash<QString, float> rpg_swing_carry_pending;
  QHash<QString, QVector3D> rpg_swing_carry_origin;
  QHash<QString, bool> rpg_swing_carry_open;
  QHash<QString, TravelObservation> rpg_travel_observations;
  QHash<QString, float> minimum_group_pair_distance;
  QHash<QString, float> minimum_group_pair_standoff;
  QHash<QString, float> minimum_group_pair_standoff_at;
  QHash<int, int> completed_construction_by_owner;
  QHash<int, int> completed_harvest_by_owner;
  QSet<Engine::Core::EntityID> latched_builder_completions;
  QSet<Engine::Core::EntityID> initial_building_ids;
  QSet<Engine::Core::EntityID> observed_constructed_building_ids;
  QHash<QString, std::uint64_t> rendered_by_group;
  QHash<QString, std::vector<float>> initial_formation_projection;
  QSet<QString> issue_keys;
  std::vector<TraceFrame> trace;
  App::Core::CommanderPresentationTrace commander_trace;
  float animation_time{0.0F};
  ArenaScenarioReport report;
  ArenaEnvironmentSnapshot environment_snapshot;
  float elapsed{0.0F};
  float duration_limit{0.0F};
  bool started{false};
  bool complete{false};
  bool end_expectations_checked{false};
  QString rpg_aim_shooter_group;
  QString rpg_aim_target_group;
  Impl(Engine::Core::World& world_value,
       ArenaScenarioHost host_value,
       const ArenaScenarioDefinition& definition,
       QVector3D origin);
  [[nodiscard]] auto ordered_destination(const QString& group) const -> QVector3D;
  [[nodiscard]] auto
  group_definition(const QString& name) const -> const ArenaScenarioGroup*;
  [[nodiscard]] auto
  ids(const QString& group) const -> const std::vector<Engine::Core::EntityID>&;
  [[nodiscard]] auto step_entities(const ArenaScenarioStep& step) const
      -> std::vector<Engine::Core::EntityID>;
  [[nodiscard]] auto entity_alive(Engine::Core::EntityID entity_id) const -> bool;
  [[nodiscard]] auto group_health(const QString& group) const -> int;
  [[nodiscard]] auto commander_frames() const -> std::vector<const TraceFrame*>;
  [[nodiscard]] auto group_destroyed(const QString& group) const -> bool;
  [[nodiscard]] auto centroid(const QString& group) const -> std::optional<QVector3D>;
  void track_rpg_aim();
  [[nodiscard]] auto groups_distance(const QString& lhs,
                                     const QString& rhs) const -> float;
  void add_issue(QString code,
                 QString message,
                 Engine::Core::EntityID entity_id = 0,
                 int soldier_index = -1);
  void spawn_group(const ArenaScenarioGroup& group);
  [[nodiscard]] auto trigger_ready(std::size_t index,
                                   const ArenaScenarioStep& step) const -> bool;
  void arm_response(const QString& group, const QString& command);
  void stop_group(const QString& group, bool clear_attack);
  void attack_group(const QString& group, const QString& target_group, bool chase);
  void form_army(const ArenaScenarioStep& step);
  void execute_step(std::size_t index, const ArenaScenarioStep& step);
  struct WildlifeObservation {
    bool grazing_seen{false};
    bool flee_seen{false};
    bool hunt_seen{false};
    int min_population{-1};
    int peak_population{0};
    std::uint64_t bird_scatter_events{0U};
    std::uint64_t bird_flyovers{0U};
  };
  WildlifeObservation wildlife_observation;
  static void record_animal(Engine::Core::Entity& entity,
                            const Engine::Core::UnitComponent& unit,
                            const Engine::Core::WildlifeComponent& wildlife,
                            TraceFrame& frame);
  void record_animals(TraceFrame& frame);
  void observe_wildlife();
  void initialize_battle_sides();
  void observe_battle();
  [[nodiscard]] auto battle_decision_ends_scenario() const -> bool;
  [[nodiscard]] static auto
  side_result(const BattleSideState& side) -> ArenaBattleSideResult;
  [[nodiscard]] auto live_sides() const -> std::vector<ArenaBattleSideResult>;
  void publish_battle_outcome();
  [[nodiscard]] static auto
  windowed_peak_advance(const BattleSideState& side,
                        const ArenaExpectation& expectation) -> std::optional<float>;
  [[nodiscard]] auto battle_side(const QString& label) const -> const BattleSideState*;
  void observe_undead_zones();
  void observe_zone_shrine(const Game::Map::UndeadZone& zone,
                           UndeadZoneObservation& state);
  [[nodiscard]] auto
  undead_zone_state(const QString& zone_id) const -> UndeadZoneObservation;
  void observe_range_rings();
  void observe_commander_aura_state();
  [[nodiscard]] auto all_entities() const -> std::vector<Engine::Core::EntityID>;
  void track_elevation_leg(ElevationLegState& leg,
                           float elevation,
                           bool climbing,
                           float ceiling);
  [[nodiscard]] auto
  expectation_active(const ArenaExpectation& expectation) const -> bool;
  [[nodiscard]] auto rpg_health_protection_active(const QString& group) const -> bool;
  [[nodiscard]] auto
  projectile_pair_key(Engine::Core::EntityID attacker_id,
                      Engine::Core::EntityID target_id) const -> QString;
  [[nodiscard]] static auto projectile_pair_key(const QString& attacker_group,
                                                const QString& target_group) -> QString;
  void observe_projectiles();
  [[nodiscard]] auto applies_to(const ArenaExpectation& expectation,
                                const QString& group) const -> bool;
  void observe_building_clearance(Engine::Core::EntityID entity_id,
                                  const QString& group);
  [[nodiscard]] auto tracks_narrow_layout(const QString& group) -> bool;
  void observe_narrow_layout(const QString& group, Engine::Core::EntityID entity_id);
  void observe_entity(Engine::Core::EntityID entity_id,
                      const QString& group,
                      TraceFrame& frame);
  void observe_bridge_centerline_alignment(const QString& group);
  void observe_rpg_locomotion_presentation(const TraceFrame& frame);
  void observe_rpg_swing_cadence(const TraceFrame& frame);
  static auto travel_key(const ArenaExpectation& expectation) -> QString;
  void observe_rpg_travel(const TraceFrame& frame);
  void observe_group_pair_proximity(const TraceFrame& frame);
  void observe_motion_quality(Engine::Core::EntityID entity_id, const QString& group);
  void observe_soldiers(Engine::Core::EntityID entity_id,
                        const QString& group,
                        TraceFrame& frame);
  void check_lens_gap_readability(Engine::Core::EntityID entity_id,
                                  const QString& group,
                                  int living_samples,
                                  int lens_gap_culled);
  void check_formation_order(const ArenaExpectation& expectation);
  void publish_movement_diagnostics();
  void publish_narrow_layout_outcome();
  void check_end_expectations();
  void check_battle_expectation(const ArenaExpectation& expectation);
  void check_combat_expectation(const ArenaExpectation& expectation);
  void check_commander_expectation(const ArenaExpectation& expectation);
  void check_movement_expectation(const ArenaExpectation& expectation);
  void check_frame_expectation(const ArenaExpectation& expectation);
  void check_rpg_expectation(const ArenaExpectation& expectation);
  void check_world_expectation(const ArenaExpectation& expectation);
};

} // namespace Arena
