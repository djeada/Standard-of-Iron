#include "movement_trace_analysis.h"

#include <algorithm>
#include <cmath>

#include "movement_trace_analysis_detail.h"

namespace Engine::Core {

namespace movement_analysis {

void FindingSink::add(MovementFindingKind kind,
                      EntityID entity,
                      std::uint32_t slot,
                      std::uint64_t tick,
                      float magnitude,
                      std::string detail) {
  MovementFinding finding;
  finding.kind = kind;
  finding.entity_id = entity;
  finding.slot = slot;
  finding.first_tick = tick;
  finding.last_tick = tick;
  finding.magnitude = magnitude;
  finding.detail = std::move(detail);
  m_analysis.findings.push_back(std::move(finding));
}

void FindingSink::extend(OpenFinding& run,
                         MovementFindingKind kind,
                         EntityID entity,
                         std::uint32_t slot,
                         std::uint64_t tick,
                         float magnitude,
                         std::string detail) {
  if (run.open && run.index < m_analysis.findings.size()) {
    auto& existing = m_analysis.findings[run.index];
    existing.last_tick = tick;
    existing.magnitude = std::max(existing.magnitude, magnitude);
    existing.detail = std::move(detail);
    return;
  }
  run.open = true;
  run.index = m_analysis.findings.size();
  add(kind, entity, slot, tick, magnitude, std::move(detail));
}

auto is_declared_hold(MovementOrderState state) -> bool {
  switch (state) {
  case MovementOrderState::Yielding:
  case MovementOrderState::Repathing:
  case MovementOrderState::LocallyBlocked:
  case MovementOrderState::Recovering:
    return true;
  default:
    return false;
  }
}

auto accepted_speed(const MovementTroopSample& sample) -> float {
  return std::hypot(sample.accepted_vx, sample.accepted_vz);
}

} // namespace movement_analysis

auto movement_finding_name(MovementFindingKind kind) noexcept -> const char* {
  switch (kind) {
  case MovementFindingKind::ProgressStall:
    return "ProgressStall";
  case MovementFindingKind::RouteRegression:
    return "RouteRegression";
  case MovementFindingKind::IndefiniteActiveOrder:
    return "IndefiniteActiveOrder";
  case MovementFindingKind::MissingTerminalOutcome:
    return "MissingTerminalOutcome";
  case MovementFindingKind::ObstructionNotEscalated:
    return "ObstructionNotEscalated";
  case MovementFindingKind::BlockedStepStreak:
    return "BlockedStepStreak";
  case MovementFindingKind::RepathChurn:
    return "RepathChurn";
  case MovementFindingKind::WaypointRegression:
    return "WaypointRegression";
  case MovementFindingKind::HeadingOscillation:
    return "HeadingOscillation";
  case MovementFindingKind::DirectionReversal:
    return "DirectionReversal";
  case MovementFindingKind::AngularSpeedExceeded:
    return "AngularSpeedExceeded";
  case MovementFindingKind::AngularAccelerationExceeded:
    return "AngularAccelerationExceeded";
  case MovementFindingKind::ArrivalNotSettled:
    return "ArrivalNotSettled";
  case MovementFindingKind::ArrivalRestart:
    return "ArrivalRestart";
  case MovementFindingKind::GaitWithoutMotion:
    return "GaitWithoutMotion";
  case MovementFindingKind::IdleWhileMoving:
    return "IdleWhileMoving";
  case MovementFindingKind::DirectionSourceNotAccepted:
    return "DirectionSourceNotAccepted";
  case MovementFindingKind::LayoutModeToggle:
    return "LayoutModeToggle";
  case MovementFindingKind::LayoutModeDwellTooShort:
    return "LayoutModeDwellTooShort";
  case MovementFindingKind::LayoutAspectRatio:
    return "LayoutAspectRatio";
  case MovementFindingKind::SlotIdentityChanged:
    return "SlotIdentityChanged";
  case MovementFindingKind::SoldierAnchorJump:
    return "SoldierAnchorJump";
  case MovementFindingKind::MarkerAnchorMismatch:
    return "MarkerAnchorMismatch";
  case MovementFindingKind::ShadowAnchorMismatch:
    return "ShadowAnchorMismatch";
  case MovementFindingKind::PickingAnchorMismatch:
    return "PickingAnchorMismatch";
  case MovementFindingKind::MissingFinalAnchor:
    return "MissingFinalAnchor";
  case MovementFindingKind::CollisionPenetration:
    return "CollisionPenetration";
  case MovementFindingKind::BodyOverlap:
    return "BodyOverlap";
  case MovementFindingKind::Starvation:
    return "Starvation";
  }
  return "Unknown";
}

auto MovementAnalysis::count(MovementFindingKind kind) const -> std::size_t {
  std::size_t total = 0;
  for (auto const& finding : findings) {
    if (finding.kind == kind) {
      ++total;
    }
  }
  return total;
}

auto MovementAnalysis::worst_entity() const -> EntityID {
  EntityID worst = 0;
  std::uint32_t best = 0;
  for (auto const& entity : entities) {
    if (entity.findings > best) {
      best = entity.findings;
      worst = entity.entity_id;
    }
  }
  if (worst == 0 && !entities.empty()) {
    worst = entities.front().entity_id;
  }
  return worst;
}

auto MovementAnalysis::worst_soldier() const -> const MovementSoldierSummary* {
  const MovementSoldierSummary* worst = nullptr;
  float best = -1.0F;
  for (auto const& soldier : soldiers) {
    float const score = soldier.max_marker_error * 1000.0F + soldier.max_anchor_jump;
    if (score > best) {
      best = score;
      worst = &soldier;
    }
  }
  return worst;
}

auto analyze_movement_trace(const std::vector<MovementTroopSample>& troops,
                            const std::vector<MovementSoldierSample>& soldiers,
                            const MovementGateThresholds& thresholds)
    -> MovementAnalysis {
  MovementAnalysis analysis;
  movement_analysis::FindingSink sink(analysis);
  movement_analysis::analyze_troops(troops, thresholds, analysis, sink);
  movement_analysis::analyze_soldiers(soldiers, thresholds, analysis, sink);
  analysis.has_failure = !analysis.findings.empty();
  if (analysis.has_failure) {
    analysis.first_failing_tick = analysis.findings.front().first_tick;
    analysis.first_failing_frame = analysis.findings.front().first_tick;
    for (auto const& finding : analysis.findings) {
      analysis.first_failing_tick =
          std::min(analysis.first_failing_tick, finding.first_tick);
    }
  }
  return analysis;
}

auto analyze_active_movement_trace(const MovementGateThresholds& thresholds)
    -> MovementAnalysis {
  auto& trace = MovementTrace::instance();
  return analyze_movement_trace(
      trace.troop_samples(), trace.soldier_samples(), thresholds);
}

} // namespace Engine::Core
