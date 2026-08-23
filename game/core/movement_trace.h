#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "entity_id.h"

namespace Engine::Core {

// One declared movement state. The order state is the contract todo2.md asks
// for: an accepted order is always in exactly one of these, and the terminal
// ones are the only legal ways for an order to stop being active.
enum class MovementOrderState : std::uint8_t {
  Idle = 0,
  Following,
  Turning,
  LocallyBlocked,
  Yielding,
  Repathing,
  Recovering,
  Arrived,
  Unreachable,
  Cancelled,
  Superseded
};

[[nodiscard]] auto
is_terminal_movement_state(MovementOrderState state) noexcept -> bool;
[[nodiscard]] auto movement_state_name(MovementOrderState state) noexcept -> const
    char*;

// Where the presented facing came from this tick. Recorded so a gait/heading
// defect can be attributed instead of guessed at.
enum class MovementDirectionSource : std::uint8_t {
  None = 0,
  AcceptedVelocity,
  RouteTangent,
  BodyForward,
  DesiredVelocity,
  LayoutRelocation
};

[[nodiscard]] auto
movement_direction_source_name(MovementDirectionSource source) noexcept -> const char*;

// The width ladder from Milestone 5. Recorded here so the trace can prove a
// single enter/exit per physical passage.
enum class TraversalLayoutMode : std::uint8_t {
  Normal = 0,
  NarrowRanks,
  MarchingOrder,
  SingleFile
};

[[nodiscard]] auto
traversal_layout_mode_name(TraversalLayoutMode mode) noexcept -> const char*;

enum class MovementRepathReason : std::uint8_t {
  None = 0,
  GoalChanged,
  TopologyChanged,
  RouteInvalid,
  Blocked,
  ClearanceChanged,
  ObstructionReleased,
  RecoveryEscalation
};

[[nodiscard]] auto
movement_repath_reason_name(MovementRepathReason reason) noexcept -> const char*;

// Per-simulation-tick record for one troop entity. Plain data; the recorder
// never reads back into the world.
struct MovementTroopSample {
  std::uint64_t tick{0};
  EntityID entity_id{0};
  int owner_id{0};
  std::uint8_t troop_type{0};
  std::uint64_t command_sequence{0};
  std::uint8_t order_kind{0};
  MovementOrderState state{MovementOrderState::Idle};

  float root_x{0.0F};
  float root_z{0.0F};
  float root_yaw{0.0F};
  float previous_root_x{0.0F};
  float previous_root_z{0.0F};
  float previous_root_yaw{0.0F};

  float requested_goal_x{0.0F};
  float requested_goal_z{0.0F};
  float resolved_goal_x{0.0F};
  float resolved_goal_z{0.0F};

  std::uint64_t route_id{0};
  std::uint64_t route_revision{0};
  std::uint64_t topology_revision{0};
  std::uint32_t waypoint_index{0};
  std::uint32_t waypoint_count{0};
  float waypoint_x{0.0F};
  float waypoint_z{0.0F};
  float lookahead_x{0.0F};
  float lookahead_z{0.0F};
  float tangent_x{0.0F};
  float tangent_z{0.0F};
  float remaining_arclength{0.0F};

  float desired_vx{0.0F};
  float desired_vz{0.0F};
  float avoidance_dx{0.0F};
  float avoidance_dz{0.0F};
  float steered_vx{0.0F};
  float steered_vz{0.0F};
  bool has_contact{false};
  float contact_nx{0.0F};
  float contact_nz{0.0F};
  float accepted_dx{0.0F};
  float accepted_dz{0.0F};
  float accepted_vx{0.0F};
  float accepted_vz{0.0F};
  float rejected_dx{0.0F};
  float rejected_dz{0.0F};
  float penetration_depth{0.0F};

  float route_advance{0.0F};
  float lateral_route_error{0.0F};
  float no_progress_seconds{0.0F};
  std::uint32_t blocked_steps{0};
  std::uint32_t repath_count{0};
  MovementRepathReason repath_reason{MovementRepathReason::None};
  EntityID queue_owner{0};

  std::uint32_t neighbor_count{0};
  float nearest_time_to_collision{-1.0F};
  std::int8_t passing_side{0};
  std::uint8_t solver_result{0};

  float envelope_radius{0.0F};
  float soldier_body_radius{0.0F};
  float corridor_half_width{0.0F};
  std::uint32_t portal_id{0};
  TraversalLayoutMode traversal_mode{TraversalLayoutMode::Normal};
  std::uint32_t current_files{0};
  std::uint32_t target_files{0};
  float transition_progress{0.0F};
  float mode_dwell_seconds{0.0F};

  std::uint8_t presentation_state{0};
  float presentation_speed{0.0F};
  float presentation_dir_x{0.0F};
  float presentation_dir_z{0.0F};
  MovementDirectionSource direction_source{MovementDirectionSource::None};
  std::uint8_t gait_state{0};
  float locomotion_phase{0.0F};
};

// Per-rendered-frame record for one soldier inside a troop entity.
struct MovementSoldierSample {
  std::uint64_t frame{0};
  std::uint64_t previous_tick{0};
  std::uint64_t current_tick{0};
  float interpolation_alpha{0.0F};
  EntityID troop_id{0};
  std::uint32_t stable_slot{0};
  bool alive{true};
  bool culled{false};
  bool has_final_anchor{true};
  std::uint8_t lod{0};
  TraversalLayoutMode traversal_mode{TraversalLayoutMode::Normal};
  float transition_progress{0.0F};

  float previous_anchor_x{0.0F};
  float previous_anchor_z{0.0F};
  float current_anchor_x{0.0F};
  float current_anchor_z{0.0F};
  float interpolated_anchor_x{0.0F};
  float interpolated_anchor_z{0.0F};
  float facing_yaw{0.0F};
  float combat_offset_x{0.0F};
  float combat_offset_z{0.0F};

  float body_root_x{0.0F};
  float body_root_z{0.0F};
  float shadow_root_x{0.0F};
  float shadow_root_z{0.0F};
  float ring_root_x{0.0F};
  float ring_root_z{0.0F};
  float picking_root_x{0.0F};
  float picking_root_z{0.0F};

  float gait_speed{0.0F};
  float gait_dir_x{0.0F};
  float gait_dir_z{0.0F};
  float locomotion_phase{0.0F};
  std::uint8_t clip_id{0};
  float clip_blend{0.0F};
  bool relocation_drives_locomotion{false};
};

// Everything that must be recorded alongside an artifact so a run can be
// reproduced: seed, commands, topology, step, caps, build, preset, composition.
struct MovementTraceManifest {
  std::uint64_t seed{0};
  std::string command_stream;
  std::string map_id;
  std::uint64_t topology_revision{0};
  float fixed_step_seconds{0.0F};
  int presentation_cap_hz{0};
  std::string build_type;
  std::string commit;
  std::string graphics_preset;
  std::string unit_composition;
  std::string scenario;
  std::vector<std::pair<std::string, std::string>> extra;
};

// Opt-in trace sink. Disabled it costs one predictable bool load per call site;
// there is no string formatting on the disabled path and no allocation.
class MovementTrace {
public:
  static auto instance() -> MovementTrace&;

  [[nodiscard]] auto enabled() const noexcept -> bool { return m_enabled; }

  // Streams JSONL into `directory` (created if needed) and writes manifest.json.
  // Returns false and stays disabled if the directory cannot be opened.
  auto begin_file_session(const std::string& directory,
                          const MovementTraceManifest& manifest) -> bool;

  // Records into memory only. Used by headless gates that assert on the
  // analysis instead of parsing an artifact back.
  void begin_memory_session(const MovementTraceManifest& manifest);

  void end_session();

  void record(const MovementTroopSample& sample);
  void record(const MovementSoldierSample& sample);

  [[nodiscard]] auto manifest() const -> const MovementTraceManifest&;
  [[nodiscard]] auto troop_samples() const -> const std::vector<MovementTroopSample>&;
  [[nodiscard]] auto
  soldier_samples() const -> const std::vector<MovementSoldierSample>&;

  [[nodiscard]] auto troop_sample_count() const -> std::size_t;
  [[nodiscard]] auto soldier_sample_count() const -> std::size_t;

  // Bounds the in-memory buffers so a soak run cannot grow without limit.
  void set_memory_sample_limit(std::size_t troop_limit, std::size_t soldier_limit);

private:
  MovementTrace() = default;

  struct Session;

  bool m_enabled{false};
  mutable std::mutex m_mutex;
  std::unique_ptr<Session> m_session;
};

// Scoped memory-session helper for tests.
class ScopedMovementTrace {
public:
  explicit ScopedMovementTrace(const MovementTraceManifest& manifest);
  ~ScopedMovementTrace();

  ScopedMovementTrace(const ScopedMovementTrace&) = delete;
  auto operator=(const ScopedMovementTrace&) -> ScopedMovementTrace& = delete;
  ScopedMovementTrace(ScopedMovementTrace&&) = delete;
  auto operator=(ScopedMovementTrace&&) -> ScopedMovementTrace& = delete;
};

[[nodiscard]] auto to_json(const MovementTroopSample& sample) -> std::string;
[[nodiscard]] auto to_json(const MovementSoldierSample& sample) -> std::string;
[[nodiscard]] auto to_json(const MovementTraceManifest& manifest) -> std::string;

[[nodiscard]] auto parse_troop_sample(const std::string& line,
                                      MovementTroopSample& out) -> bool;
[[nodiscard]] auto parse_soldier_sample(const std::string& line,
                                        MovementSoldierSample& out) -> bool;

} // namespace Engine::Core
