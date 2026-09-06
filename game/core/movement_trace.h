#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "entity_id.h"
#include "movement_facts.h"

namespace Engine::Core {

struct MovementTroopSample {
  std::uint64_t session_id{0};
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
  float lane_offset{0.0F};
  float lane_scale{1.0F};
  float cohesion_pace{0.0F};
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
  float order_seconds{0.0F};
  std::uint32_t blocked_steps{0};
  std::uint32_t repath_count{0};
  MovementRepathReason repath_reason{MovementRepathReason::None};

  std::uint32_t neighbor_count{0};
  float body_overlap{0.0F};
  std::uint8_t solver_result{0};

  float envelope_radius{0.0F};
  float soldier_body_radius{0.0F};
  float corridor_half_width{0.0F};
  float formation_half_width{0.0F};
  float file_spacing{0.0F};
  float lateral_scale{1.0F};
  std::uint32_t portal_id{0};
  TraversalLayoutMode traversal_mode{TraversalLayoutMode::Normal};
  std::uint32_t normal_files{0};
  std::uint32_t current_files{0};
  std::uint32_t target_files{0};
  float transition_progress{0.0F};
  float mode_dwell_seconds{0.0F};

  bool presentation_valid{false};
  std::uint8_t presentation_state{0};
  float presentation_speed{0.0F};
  float presentation_dir_x{0.0F};
  float presentation_dir_z{0.0F};
  MovementDirectionSource direction_source{MovementDirectionSource::None};
  std::uint8_t gait_state{0};
  float locomotion_phase{0.0F};
};

struct MovementSoldierSample {
  std::uint64_t session_id{0};
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

class MovementTrace {
public:
  static auto instance() -> MovementTrace&;

  [[nodiscard]] auto enabled() const noexcept -> bool { return m_enabled; }

  void configure_from_environment();

  auto begin_file_session(const std::string& directory,
                          const MovementTraceManifest& manifest) -> bool;

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

  void set_memory_sample_limit(std::size_t troop_limit, std::size_t soldier_limit);

  void set_file_sample_budget(std::uint64_t tick_stride, std::uint64_t max_bytes);
  [[nodiscard]] auto file_bytes_written() const -> std::uint64_t;

private:
  MovementTrace() = default;

  struct Session;

  [[nodiscard]] static auto should_write(Session& session, std::uint64_t tick) -> bool;

  bool m_enabled{false};
  bool m_environment_checked{false};
  mutable std::mutex m_mutex;
  std::unique_ptr<Session> m_session;
};

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
