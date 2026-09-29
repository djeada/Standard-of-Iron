#include "formation_soldier_gait.h"

#include <algorithm>
#include <cmath>

namespace Game::Systems::Combat {
namespace {

using Soldier = Engine::Core::FormationSoldierPresentation;

constexpr float k_gait_walk_enter_speed = 0.10F;
constexpr float k_gait_walk_exit_speed = 0.05F;
constexpr float k_gait_turn_enter_degrees_per_second = 12.0F;
constexpr float k_gait_turn_exit_degrees_per_second = 6.0F;
constexpr float k_gait_min_run_speed = 2.6F;
constexpr float k_gait_run_speed_ratio = 1.45F;
constexpr float k_gait_run_exit_ratio = 0.85F;
constexpr float k_gait_run_entry_seconds = 0.15F;
constexpr float k_gait_min_dwell_seconds = 0.30F;

} // namespace

auto gait_run_speed(float march_speed) -> float {
  return std::max(k_gait_min_run_speed, march_speed * k_gait_run_speed_ratio);
}

void settle_soldier_gait(const Soldier* previous,
                         Soldier& soldier,
                         float dt,
                         float run_speed) {
  using Gait = Engine::Core::FormationSoldierGait;
  if (previous == nullptr || !previous->alive) {
    soldier.gait = Gait::Idle;
    soldier.gait_held_seconds = 0.0F;
    soldier.gait_run_pending_seconds = 0.0F;
    return;
  }
  soldier.gait = previous->gait;
  soldier.gait_held_seconds = previous->gait_held_seconds + dt;
  soldier.gait_run_pending_seconds = previous->gait_run_pending_seconds;

  float const speed = std::hypot(soldier.world_velocity_x, soldier.world_velocity_z);
  bool const stepping =
      soldier.gait == Gait::Idle
          ? speed > k_gait_walk_enter_speed ||
                soldier.angular_speed > k_gait_turn_enter_degrees_per_second
          : speed > k_gait_walk_exit_speed ||
                soldier.angular_speed > k_gait_turn_exit_degrees_per_second;

  Gait desired = Gait::Idle;
  if (stepping) {
    desired = Gait::Walk;
    if (soldier.gait == Gait::Run) {
      if (speed > run_speed * k_gait_run_exit_ratio) {
        desired = Gait::Run;
      }
    } else if (speed > run_speed) {
      soldier.gait_run_pending_seconds += dt;
      if (soldier.gait_run_pending_seconds >= k_gait_run_entry_seconds) {
        desired = Gait::Run;
      }
    }
  }
  if (desired != Gait::Run && soldier.gait != Gait::Run && speed <= run_speed) {
    soldier.gait_run_pending_seconds = 0.0F;
  }
  if (desired != soldier.gait &&
      soldier.gait_held_seconds >= k_gait_min_dwell_seconds) {
    soldier.gait = desired;
    soldier.gait_held_seconds = 0.0F;
    soldier.gait_run_pending_seconds = 0.0F;
  }
}

} // namespace Game::Systems::Combat
