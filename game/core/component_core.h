#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../animation/combat_manifest.h"
#include "../systems/nation_id.h"
#include "../systems/projectile_kind.h"
#include "../systems/resource_types.h"
#include "../systems/unit_activity.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "../wildlife/wildlife_species.h"
#include "entity.h"
#include "melee_intent.h"
#include "movement_facts.h"

namespace Game::Systems {
class MovementSystem;
class RouteFollowSystem;
} // namespace Game::Systems

namespace Engine::Core {

namespace Defaults {
inline constexpr int k_unit_default_health = 100;
inline constexpr float k_unit_default_vision_range = 12.0F;
inline constexpr float k_vision_reveal_scale = 1.5F;

inline constexpr float k_attack_default_range = 2.0F;
inline constexpr int k_attack_default_damage = 10;
inline constexpr float k_attack_melee_range = 1.5F;
inline constexpr float k_attack_height_tolerance = 2.0F;

inline constexpr float k_production_default_build_time = 4.0F;
inline constexpr int k_production_max_units = 10000;

inline constexpr float k_capture_required_time = 15.0F;

inline constexpr float k_hold_stand_up_duration = 2.0F;
inline constexpr float k_hold_kneel_duration = 1.5F;

inline constexpr float k_guard_enter_duration = 1.8F;
inline constexpr float k_guard_exit_duration = 1.4F;

inline constexpr float k_guard_default_radius = 10.0F;
inline constexpr float k_guard_return_threshold = 1.0F;
inline constexpr float k_blood_stain_default_radius = 0.46F;
inline constexpr float k_blood_stain_default_aspect_ratio = 1.0F;
inline constexpr float k_blood_stain_default_lifetime = 8.0F;
inline constexpr int k_blood_stain_max_active = 10;
} // namespace Defaults

class TransformComponent {
public:
  TransformComponent(float x = 0.0F,
                     float y = 0.0F,
                     float z = 0.0F,
                     float rot_x = 0.0F,
                     float rot_y = 0.0F,
                     float rot_z = 0.0F,
                     float scale_x = 1.0F,
                     float scale_y = 1.0F,
                     float scale_z = 1.0F)
      : position{x, y, z}
      , rotation{rot_x, rot_y, rot_z}
      , scale{scale_x, scale_y, scale_z} {}

  struct Vec3 {
    float x, y, z;
  };
  Vec3 position;
  Vec3 rotation;
  Vec3 scale;

  float desired_yaw = 0.0F;
  bool has_desired_yaw = false;
};

class RenderableComponent {
public:
  std::string renderer_id;
  bool visible{true};
};

class UnitComponent {
public:
  UnitComponent(int health = Defaults::k_unit_default_health,
                int max_health = Defaults::k_unit_default_health,
                float speed = 1.0F,
                float vision = Defaults::k_unit_default_vision_range)
      : health(health)
      , max_health(max_health)
      , speed(speed)
      , vision_range(vision) {}

  int health;
  int max_health;
  float speed;
  Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
  int owner_id{0};
  float vision_range;
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  bool uses_nation_formation_profile{false};

  int render_individuals_per_unit_override{0};

  int squad_strength{0};
  bool render_rider{true};
  std::uint8_t death_sequence_override{0xFF};
};

[[nodiscard]] inline auto resolve_surviving_individual_count(
    int health, int max_health, int individuals_per_unit) noexcept -> int {
  if (individuals_per_unit <= 0 || health <= 0) {
    return 0;
  }
  int const safe_max_health = std::max(1, max_health);
  float const ratio =
      std::clamp(health / static_cast<float>(safe_max_health), 0.0F, 1.0F);
  return std::max(1,
                  std::min(individuals_per_unit,
                           static_cast<int>(std::ceil(
                               ratio * static_cast<float>(individuals_per_unit)))));
}

enum class MovementState : std::uint8_t {
  Idle = 0,
  FollowingPath = 2,
  FollowingDirect = 3
};

class MovementComponent {
public:
  MovementComponent() = default;

  [[nodiscard]] auto get_has_target() const -> bool { return has_target; }
  [[nodiscard]] auto get_target_x() const -> float { return target_x; }
  [[nodiscard]] auto get_target_y() const -> float { return target_y; }
  [[nodiscard]] auto get_goal_x() const -> float { return goal_x; }
  [[nodiscard]] auto get_goal_y() const -> float { return goal_y; }
  [[nodiscard]] auto get_vx() const -> float { return vx; }
  [[nodiscard]] auto get_vz() const -> float { return vz; }
  [[nodiscard]] auto get_travelled() const -> float { return travelled; }
  [[nodiscard]] auto get_path() const -> const std::vector<std::pair<float, float>>& {
    return path;
  }
  [[nodiscard]] auto get_path_index() const -> std::size_t { return path_index; }
  [[nodiscard]] auto get_state() const -> MovementState {
    if (has_target && has_waypoints()) {
      return MovementState::FollowingPath;
    }
    if (has_target) {
      return MovementState::FollowingDirect;
    }
    return MovementState::Idle;
  }

  void clear_path() {
    path.clear();
    path_index = 0;
  }

  void stop() {
    has_target = false;
    clear_path();
    vx = 0.0F;
    vz = 0.0F;
    precise_arrival = false;
    structure_approach_target_id = 0;
    has_requested_goal = false;
  }

  [[nodiscard]] auto get_has_requested_goal() const -> bool {
    return has_requested_goal;
  }
  [[nodiscard]] auto get_requested_goal_x() const -> float { return requested_goal_x; }
  [[nodiscard]] auto get_requested_goal_z() const -> float { return requested_goal_z; }

  [[nodiscard]] auto has_waypoints() const -> bool { return path_index < path.size(); }

  [[nodiscard]] auto current_waypoint() const -> const std::pair<float, float>& {
    return path[path_index];
  }

  void advance_waypoint() {
    if (path_index < path.size()) {
      ++path_index;
    }
  }

  [[nodiscard]] auto remaining_waypoints() const -> std::size_t {
    return path.size() > path_index ? path.size() - path_index : 0;
  }

  void validate_path_index() {
    if (path_index > path.size()) {
      path_index = path.size();
    }
  }

  void set_rest_position(float x, float z) {
    goal_x = x;
    goal_y = z;
    target_x = x;
    target_y = z;
    has_requested_goal = false;
  }

  void engage_manual_move(float x, float z) {
    has_target = true;
    target_x = x;
    target_y = z;
    goal_x = x;
    goal_y = z;
    has_requested_goal = false;
  }

  void set_manual_velocity(float new_vx, float new_vz) {
    vx = new_vx;
    vz = new_vz;
  }

  void set_structure_approach_target(EntityID target_id) {
    structure_approach_target_id = target_id;
  }

  void clear_structure_approach_target() { structure_approach_target_id = 0; }

  [[nodiscard]] auto get_structure_approach_target() const -> EntityID {
    return structure_approach_target_id;
  }

  [[nodiscard]] auto get_stuck_time() const -> float { return stuck_timer; }

  [[nodiscard]] auto get_precise_arrival() const -> bool { return precise_arrival; }

  [[nodiscard]] auto get_order_sequence() const -> std::uint64_t {
    return order_sequence;
  }
  [[nodiscard]] auto get_route_revision() const -> std::uint64_t {
    return route_revision;
  }
  [[nodiscard]] auto get_topology_revision() const -> std::uint64_t {
    return topology_revision;
  }
  [[nodiscard]] auto get_route_id() const -> std::uint64_t { return route_id; }
  [[nodiscard]] auto get_route_lane_offset() const -> float {
    return route_lane_offset;
  }
  [[nodiscard]] auto get_route_lane_scale() const -> float {
    if (route_lane_min_scale >= 0.99F || path_index < route_opening_waypoint_index ||
        path_index >= route_reform_waypoint_index) {
      return 1.0F;
    }
    return route_lane_min_scale;
  }
  [[nodiscard]] auto get_declared_group_pace() const -> float {
    return declared_group_pace;
  }

  void begin_order() {
    ++order_sequence;
    route_id = 0U;
    route_lane_offset = 0.0F;
    route_lane_min_scale = 1.0F;
    route_opening_waypoint_index = 0U;
    route_reform_waypoint_index = 0U;
    declared_group_pace = 0.0F;
  }
  void begin_route(std::uint64_t topology) {
    ++route_revision;
    topology_revision = topology;
  }

  [[nodiscard]] auto get_can_enter_forest() const -> bool { return can_enter_forest; }
  void set_can_enter_forest(bool allowed) { can_enter_forest = allowed; }

  [[nodiscard]] auto get_navigation_clearance() const -> float {
    return navigation_clearance;
  }
  void set_navigation_clearance(float radius) {
    navigation_clearance = std::max(0.0F, radius);
  }

private:
  friend class Game::Systems::MovementSystem;
  friend class Game::Systems::RouteFollowSystem;
  friend class Serialization;
  friend struct MovementTestAccess;

  bool has_target{false};
  float target_x{0.0F}, target_y{0.0F};
  float goal_x{0.0F}, goal_y{0.0F};
  float vx{0.0F}, vz{0.0F};

  float travelled{0.0F};

  std::vector<std::pair<float, float>> path;
  std::size_t path_index{0};

  bool has_requested_goal{false};
  float requested_goal_x{0.0F}, requested_goal_z{0.0F};

  float navigation_clearance{0.5F};

  bool stuck_ref_valid{false};
  float stuck_ref_x{0.0F}, stuck_ref_z{0.0F};
  float stuck_timer{0.0F};

  bool precise_arrival{false};
  EntityID structure_approach_target_id{0};
  bool can_enter_forest{true};

  std::uint64_t order_sequence{0};
  std::uint64_t route_revision{0};
  std::uint64_t topology_revision{0};
  std::uint64_t route_id{0};
  float route_lane_offset{0.0F};
  float route_lane_min_scale{1.0F};
  std::size_t route_opening_waypoint_index{0U};
  std::size_t route_reform_waypoint_index{0U};
  float declared_group_pace{0.0F};
};

class MovementFactsComponent {
public:
  RootPoseFacts previous_root;
  RouteIntentFacts route;
  DesiredMotionFacts desired;
  SteeringFacts steering;
  MotorFacts motor;
  MovementProgressFacts progress;
  TraversalLayoutFacts traversal;

  MovementDirectionSource direction_source{MovementDirectionSource::None};

  float last_accepted_speed{0.0F};

  void begin_tick() {
    desired = {};
    motor = {};
  }
};

enum class PlayerOrderIntentKind : std::uint8_t {
  None,
  ManualMove
};

class PlayerOrderIntentComponent {
public:
  PlayerOrderIntentKind kind{PlayerOrderIntentKind::None};
  bool suppress_opportunistic_combat{false};
};

enum class MotionPresentationSource : std::uint8_t {
  None,
  Navigation,
  Chase,
  DirectControl,
  BuilderBypass,
  ForcedDisplacement
};

enum class MotionPresentationState : std::uint8_t {
  Idle,
  Turning,
  Walk,
  Run,
  Yielding,
  Recovering,
  ForcedDisplacement
};

class MotionPresentationComponent {
public:
  MotionPresentationComponent() = default;

  bool snapshot_valid{false};
  bool initialized{false};
  float previous_x{0.0F}, previous_y{0.0F}, previous_z{0.0F};
  float previous_rotation_y{0.0F};
  float displacement_x{0.0F}, displacement_z{0.0F};
  float velocity_x{0.0F}, velocity_z{0.0F};
  float speed{0.0F};
  float direction_x{0.0F}, direction_z{1.0F};
  bool has_velocity{false};
  bool has_navigation_intent{false};
  bool has_chase_intent{false};
  bool attack_target_in_range{false};
  bool has_movement_target{false};
  float movement_target_x{0.0F}, movement_target_z{0.0F};
  MotionPresentationSource source{MotionPresentationSource::None};
  MotionPresentationState state{MotionPresentationState::Idle};
  MotionPresentationState previous_state{MotionPresentationState::Idle};
  bool state_changed{false};
  float state_time{0.0F};
  float seconds_since_motion{0.0F};
  float stalled_seconds{0.0F};
  float tick_delta_time{0.0F};

  void set_state(MotionPresentationState next_state) noexcept {
    previous_state = state;
    state = next_state;
    state_changed = state != previous_state;
  }

  [[nodiscard]] auto is_idle_state() const noexcept -> bool {
    return state == MotionPresentationState::Idle;
  }
  [[nodiscard]] auto is_walk_state() const noexcept -> bool {
    return state == MotionPresentationState::Walk ||
           state == MotionPresentationState::ForcedDisplacement;
  }
  [[nodiscard]] auto is_run_state() const noexcept -> bool {
    return state == MotionPresentationState::Run;
  }
  [[nodiscard]] auto has_locomotion() const noexcept -> bool {
    return state == MotionPresentationState::Walk ||
           state == MotionPresentationState::Run ||
           state == MotionPresentationState::ForcedDisplacement;
  }
};

} // namespace Engine::Core
