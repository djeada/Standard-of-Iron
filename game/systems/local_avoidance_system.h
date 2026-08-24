#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity_id.h"
#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

struct LocalAvoidanceDiagnostics {
  float spatial_hash_build_ms{0.0F};
  std::uint32_t average_neighbors_checked{0};
  std::uint32_t units_processed{0};
  std::uint32_t overlaps_detected{0};
  std::uint32_t units_steered{0};
  std::uint32_t candidates_evaluated{0};
};

// Stage two of the Movement phase: predictive avoidance.
//
// The old solver was overlap-reactive -- it acted only after two circles were
// already intersecting, averaged every correction, and skipped avoidance
// entirely between members of the same formation or the same owner. That is not
// coordination; it permits overlap and moves the repair downstream. Symmetric
// crowds could also alternate between two equally plausible escapes, because
// nothing remembered which side an encounter had already committed to.
//
// This solver predicts. It builds the neighbour set from swept circles over a
// bounded time horizon, evaluates a fixed, deterministically ordered fan of
// candidate velocities, and publishes the winner as a steered velocity. The
// motor remains the authority on what displacement is accepted.
class LocalAvoidanceSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] auto diagnostics() const -> const LocalAvoidanceDiagnostics& {
    return m_diagnostics;
  }

  static constexpr float k_default_cell_size = 4.0F;
  // Bodies keep this much air between them before either is asked to steer.
  static constexpr float k_separation_radius = 0.15F;
  // How far ahead the solver looks. Beyond it, an encounter is not this tick's
  // problem and open ground costs nothing.
  static constexpr float k_time_horizon_seconds = 2.5F;
  // Deterministic candidate fan. Angles are tried in this order and ties break
  // toward the earlier entry, so two symmetric bodies never coin-flip.
  static constexpr int k_candidate_angle_count = 9;
  static constexpr int k_candidate_speed_count = 3;
  static constexpr std::size_t k_max_neighbors = 6;
  // A committed passing side is held for at least this long, so a pair cannot
  // swap sides every time the geometry crosses a tie.
  static constexpr float k_passing_side_hold_seconds = 1.25F;
  // Bodies that start the tick already overlapping get a bounded push. It is
  // separate from steering and is applied by the motor, so it can never move a
  // root through a wall.
  static constexpr float k_overlap_correction_speed = 1.5F;

private:
  struct UnitCircle {
    Engine::Core::EntityID id{0};
    float x{0.0F};
    float z{0.0F};
    float radius{0.5F};
    float core_radius{0.5F};
    float desired_vx{0.0F};
    float desired_vz{0.0F};
    // What this body actually steered to last tick. Predicting a neighbour from
    // its raw intent makes both of a pair believe the other will barrel through,
    // so both keep dodging.
    float predicted_vx{0.0F};
    float predicted_vz{0.0F};
    float max_speed{0.0F};
    std::uint32_t formation_id{0};
    Engine::Core::EntityID engaged_target{0};
    int owner_id{0};
    std::uint8_t priority{0};
    bool is_moving{false};
    bool follows_navigation_path{false};
    bool avoids{false};
  };

  struct Neighbor {
    std::size_t index{0};
    float time_to_collision{-1.0F};
    float response{0.5F};
  };

  static auto cell_key(int cell_x, int cell_z) -> std::int64_t;

  void build_index(Engine::Core::SystemContext& context);
  auto gather_neighbors(std::size_t self, float horizon) -> std::size_t;

  LocalAvoidanceDiagnostics m_diagnostics;
  std::unordered_map<std::int64_t, std::vector<std::size_t>> m_grid;
  std::vector<std::int64_t> m_active_cell_keys;
  std::vector<UnitCircle> m_circles;
  std::vector<Neighbor> m_neighbors;
  std::size_t m_previous_cell_count{0};
};

} // namespace Game::Systems
