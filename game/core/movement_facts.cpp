#include "movement_facts.h"

namespace Engine::Core {

auto is_terminal_movement_state(MovementOrderState state) noexcept -> bool {
  switch (state) {
  case MovementOrderState::Arrived:
  case MovementOrderState::Unreachable:
  case MovementOrderState::Cancelled:
  case MovementOrderState::Superseded:
    return true;
  default:
    return false;
  }
}

auto is_active_movement_state(MovementOrderState state) noexcept -> bool {
  switch (state) {
  case MovementOrderState::Following:
  case MovementOrderState::Turning:
  case MovementOrderState::LocallyBlocked:
  case MovementOrderState::Yielding:
  case MovementOrderState::Repathing:
  case MovementOrderState::Recovering:
    return true;
  default:
    return false;
  }
}

auto movement_state_name(MovementOrderState state) noexcept -> const char* {
  switch (state) {
  case MovementOrderState::Idle:
    return "Idle";
  case MovementOrderState::Following:
    return "Following";
  case MovementOrderState::Turning:
    return "Turning";
  case MovementOrderState::LocallyBlocked:
    return "LocallyBlocked";
  case MovementOrderState::Yielding:
    return "Yielding";
  case MovementOrderState::Repathing:
    return "Repathing";
  case MovementOrderState::Recovering:
    return "Recovering";
  case MovementOrderState::Arrived:
    return "Arrived";
  case MovementOrderState::Unreachable:
    return "Unreachable";
  case MovementOrderState::Cancelled:
    return "Cancelled";
  case MovementOrderState::Superseded:
    return "Superseded";
  }
  return "Unknown";
}

auto movement_direction_source_name(MovementDirectionSource source) noexcept -> const
    char* {
  switch (source) {
  case MovementDirectionSource::None:
    return "None";
  case MovementDirectionSource::AcceptedVelocity:
    return "AcceptedVelocity";
  case MovementDirectionSource::RouteTangent:
    return "RouteTangent";
  case MovementDirectionSource::BodyForward:
    return "BodyForward";
  case MovementDirectionSource::DesiredVelocity:
    return "DesiredVelocity";
  case MovementDirectionSource::LayoutRelocation:
    return "LayoutRelocation";
  }
  return "Unknown";
}

auto traversal_layout_mode_name(TraversalLayoutMode mode) noexcept -> const char* {
  switch (mode) {
  case TraversalLayoutMode::Normal:
    return "Normal";
  case TraversalLayoutMode::NarrowRanks:
    return "NarrowRanks";
  case TraversalLayoutMode::MarchingOrder:
    return "MarchingOrder";
  case TraversalLayoutMode::SingleFile:
    return "SingleFile";
  }
  return "Unknown";
}

auto movement_repath_reason_name(MovementRepathReason reason) noexcept -> const char* {
  switch (reason) {
  case MovementRepathReason::None:
    return "None";
  case MovementRepathReason::GoalChanged:
    return "GoalChanged";
  case MovementRepathReason::TopologyChanged:
    return "TopologyChanged";
  case MovementRepathReason::RouteInvalid:
    return "RouteInvalid";
  case MovementRepathReason::Blocked:
    return "Blocked";
  case MovementRepathReason::ClearanceChanged:
    return "ClearanceChanged";
  case MovementRepathReason::ObstructionReleased:
    return "ObstructionReleased";
  case MovementRepathReason::RecoveryEscalation:
    return "RecoveryEscalation";
  }
  return "Unknown";
}

} // namespace Engine::Core
