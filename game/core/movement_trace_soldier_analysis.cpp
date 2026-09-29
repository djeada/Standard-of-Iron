#include <algorithm>
#include <cmath>
#include <map>

#include "movement_trace_analysis_detail.h"

namespace Engine::Core::movement_analysis {

namespace {

struct SoldierKey {
  std::uint64_t session;
  EntityID troop;
  std::uint32_t slot;
  auto operator<(const SoldierKey& other) const -> bool {
    if (session != other.session) {
      return session < other.session;
    }
    return troop != other.troop ? troop < other.troop : slot < other.slot;
  }
};

using SoldierSamples = std::vector<const MovementSoldierSample*>;

auto planar_offset(float ax, float az, float bx, float bz) -> float {
  return std::max(std::fabs(ax - bx), std::fabs(az - bz));
}

void report_duplicate_slots(
    const std::map<SoldierKey, std::vector<std::uint32_t>>& per_frame,
    FindingSink& sink) {
  for (auto const& [frame_key, frame_slots] : per_frame) {
    std::vector<std::uint32_t> sorted = frame_slots;
    std::sort(sorted.begin(), sorted.end());
    auto const duplicate = std::adjacent_find(sorted.begin(), sorted.end());
    if (duplicate != sorted.end()) {
      sink.add(MovementFindingKind::SlotIdentityChanged,
               frame_key.troop,
               *duplicate,
               frame_key.slot,
               0.0F,
               text("slot %u submitted twice in one frame", *duplicate));
    }
  }
}

void check_anchor_agreement(const MovementSoldierSample& sample,
                            const SoldierKey& key,
                            const MovementGateThresholds& thresholds,
                            MovementSoldierSummary& summary,
                            FindingSink& sink) {
  float const ring_error = planar_offset(
      sample.ring_root_x, sample.ring_root_z, sample.body_root_x, sample.body_root_z);
  summary.max_marker_error = std::max(summary.max_marker_error, ring_error);
  if (ring_error > thresholds.marker_anchor_tolerance) {
    sink.add(MovementFindingKind::MarkerAnchorMismatch,
             key.troop,
             key.slot,
             sample.frame,
             ring_error,
             text("ring at (%.5f, %.5f) vs body (%.5f, %.5f)",
                  static_cast<double>(sample.ring_root_x),
                  static_cast<double>(sample.ring_root_z),
                  static_cast<double>(sample.body_root_x),
                  static_cast<double>(sample.body_root_z)));
  }

  float const shadow_error = planar_offset(sample.shadow_root_x,
                                           sample.shadow_root_z,
                                           sample.body_root_x,
                                           sample.body_root_z);
  if (shadow_error > thresholds.marker_anchor_tolerance) {
    sink.add(MovementFindingKind::ShadowAnchorMismatch,
             key.troop,
             key.slot,
             sample.frame,
             shadow_error,
             text("contact shadow off the body anchor by %.5f",
                  static_cast<double>(shadow_error)));
  }

  float const picking_error = planar_offset(sample.picking_root_x,
                                            sample.picking_root_z,
                                            sample.body_root_x,
                                            sample.body_root_z);
  if (picking_error > thresholds.marker_anchor_tolerance) {
    sink.add(MovementFindingKind::PickingAnchorMismatch,
             key.troop,
             key.slot,
             sample.frame,
             picking_error,
             text("picking proxy off the body anchor by %.5f",
                  static_cast<double>(picking_error)));
  }
}

void check_anchor_jump(const MovementSoldierSample& sample,
                       const MovementSoldierSample* previous,
                       const SoldierKey& key,
                       const MovementGateThresholds& thresholds,
                       MovementSoldierSummary& summary,
                       FindingSink& sink) {
  if (previous == nullptr || !previous->alive || previous->culled ||
      sample.frame != previous->frame + 1U) {
    return;
  }
  float const jump = std::hypot(sample.body_root_x - previous->body_root_x,
                                sample.body_root_z - previous->body_root_z);
  summary.max_anchor_jump = std::max(summary.max_anchor_jump, jump);
  if (jump > thresholds.soldier_anchor_jump_metres) {
    sink.add(MovementFindingKind::SoldierAnchorJump,
             key.troop,
             key.slot,
             sample.frame,
             jump,
             text("soldier root moved %.3fm in one frame", static_cast<double>(jump)));
  }
}

void walk_soldier(const SoldierKey& key,
                  const SoldierSamples& samples,
                  const MovementGateThresholds& thresholds,
                  MovementAnalysis& analysis,
                  FindingSink& sink) {
  MovementSoldierSummary summary;
  summary.troop_id = key.troop;
  summary.slot = key.slot;
  std::size_t const findings_before = sink.finding_count();

  const MovementSoldierSample* previous = nullptr;
  for (auto const* sample_ptr : samples) {
    auto const& sample = *sample_ptr;
    ++summary.frames;

    if (sample.alive && !sample.culled) {
      if (!sample.has_final_anchor) {
        sink.add(MovementFindingKind::MissingFinalAnchor,
                 key.troop,
                 key.slot,
                 sample.frame,
                 0.0F,
                 "living soldier submitted without a final presented anchor");
      }
      check_anchor_agreement(sample, key, thresholds, summary, sink);
      check_anchor_jump(sample, previous, key, thresholds, summary, sink);
    }
    previous = sample_ptr;
  }

  summary.findings = static_cast<std::uint32_t>(sink.finding_count() - findings_before);
  analysis.soldiers.push_back(summary);
}

} // namespace

void analyze_soldiers(const std::vector<MovementSoldierSample>& soldiers,
                      const MovementGateThresholds& thresholds,
                      MovementAnalysis& analysis,
                      FindingSink& sink) {
  std::map<SoldierKey, SoldierSamples> by_slot;
  std::map<SoldierKey, std::vector<std::uint32_t>> per_frame;
  for (auto const& sample : soldiers) {
    by_slot[SoldierKey{sample.session_id, sample.troop_id, sample.stable_slot}]
        .push_back(&sample);
    per_frame[SoldierKey{sample.session_id,
                         sample.troop_id,
                         static_cast<std::uint32_t>(sample.frame)}]
        .push_back(sample.stable_slot);
  }

  report_duplicate_slots(per_frame, sink);

  for (auto& [key, samples] : by_slot) {
    std::stable_sort(
        samples.begin(),
        samples.end(),
        [](const MovementSoldierSample* lhs, const MovementSoldierSample* rhs) {
          return lhs->frame < rhs->frame;
        });
    walk_soldier(key, samples, thresholds, analysis, sink);
  }
}

} // namespace Engine::Core::movement_analysis
