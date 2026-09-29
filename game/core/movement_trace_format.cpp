#include <algorithm>
#include <cmath>
#include <sstream>

#include "movement_trace_analysis.h"
#include "movement_trace_analysis_detail.h"

namespace Engine::Core {

using movement_analysis::text;

auto format_movement_summary(const MovementAnalysis& analysis) -> std::string {
  std::ostringstream out;
  out << "entity     ticks   travel   stall  regress  repath  blocked  flips  "
         "layout  findings  final\n";
  for (auto const& entity : analysis.entities) {
    out << text("%-10llu %5u %8.2f %7.2f %8.2f %7u %8u %6u %7u %9u  %s\n",
                static_cast<unsigned long long>(entity.entity_id),
                entity.ticks,
                static_cast<double>(entity.travelled),
                static_cast<double>(entity.max_stall_seconds),
                static_cast<double>(entity.max_route_regression),
                entity.repaths,
                entity.blocked_steps,
                entity.heading_flips,
                entity.layout_transitions,
                entity.findings,
                movement_state_name(entity.final_state));
  }
  if (!analysis.soldiers.empty()) {
    out << "\ntroop      slot  frames   maxjump   maxmarker  findings\n";
    for (auto const& soldier : analysis.soldiers) {
      if (soldier.findings == 0U && soldier.max_marker_error <= 0.0F) {
        continue;
      }
      out << text("%-10llu %5u %7u %9.4f %11.6f %9u\n",
                  static_cast<unsigned long long>(soldier.troop_id),
                  soldier.slot,
                  soldier.frames,
                  static_cast<double>(soldier.max_anchor_jump),
                  static_cast<double>(soldier.max_marker_error),
                  soldier.findings);
    }
  }
  return out.str();
}

auto format_movement_findings(const MovementAnalysis& analysis) -> std::string {
  std::ostringstream out;
  for (auto const& finding : analysis.findings) {
    out << text("[%s] entity %llu slot %u ticks %llu..%llu magnitude %.4f: ",
                movement_finding_name(finding.kind),
                static_cast<unsigned long long>(finding.entity_id),
                finding.slot,
                static_cast<unsigned long long>(finding.first_tick),
                static_cast<unsigned long long>(finding.last_tick),
                static_cast<double>(finding.magnitude))
        << finding.detail << '\n';
  }
  return out.str();
}

auto format_movement_timeline(const std::vector<MovementTroopSample>& troops,
                              EntityID entity_id,
                              std::uint64_t centre_tick,
                              std::uint32_t ticks_before,
                              std::uint32_t ticks_after) -> std::string {
  std::uint64_t const low =
      centre_tick > ticks_before ? centre_tick - ticks_before : 0U;
  std::uint64_t const high = centre_tick + ticks_after;

  std::vector<const MovementTroopSample*> window;
  for (auto const& sample : troops) {
    if (sample.entity_id != entity_id || sample.tick < low || sample.tick > high) {
      continue;
    }
    window.push_back(&sample);
  }
  std::stable_sort(window.begin(),
                   window.end(),
                   [](const MovementTroopSample* lhs, const MovementTroopSample* rhs) {
                     return lhs->tick < rhs->tick;
                   });

  std::ostringstream out;
  out << "tick    state           root(x,z)         yaw     accepted(v)   "
         "advance  remaining  wp     mode        files  corridor/needed  "
         "spacing  gait\n";
  for (auto const* sample : window) {
    out << text("%-7llu %-15s (%7.2f,%7.2f) %7.1f (%6.2f,%6.2f) %8.3f %10.2f "
                "%2u/%-2u %-11s %2u/%-2u %6.2f/%-6.2f %6.2f  %u\n",
                static_cast<unsigned long long>(sample->tick),
                movement_state_name(sample->state),
                static_cast<double>(sample->root_x),
                static_cast<double>(sample->root_z),
                static_cast<double>(sample->root_yaw),
                static_cast<double>(sample->accepted_vx),
                static_cast<double>(sample->accepted_vz),
                static_cast<double>(sample->route_advance),
                static_cast<double>(sample->remaining_arclength),
                sample->waypoint_index,
                sample->waypoint_count,
                traversal_layout_mode_name(sample->traversal_mode),
                sample->current_files,
                sample->normal_files,
                static_cast<double>(sample->corridor_half_width),
                static_cast<double>(sample->formation_half_width),
                static_cast<double>(sample->file_spacing),
                static_cast<unsigned>(sample->presentation_state));
  }
  return out.str();
}

auto format_soldier_timeline(const std::vector<MovementSoldierSample>& soldiers,
                             EntityID troop_id,
                             std::uint32_t slot,
                             std::uint64_t centre_frame,
                             std::uint32_t frames_before,
                             std::uint32_t frames_after) -> std::string {
  std::uint64_t const low =
      centre_frame > frames_before ? centre_frame - frames_before : 0U;
  std::uint64_t const high = centre_frame + frames_after;

  std::vector<const MovementSoldierSample*> window;
  for (auto const& sample : soldiers) {
    if (sample.troop_id != troop_id || sample.stable_slot != slot ||
        sample.frame < low || sample.frame > high) {
      continue;
    }
    window.push_back(&sample);
  }
  std::stable_sort(
      window.begin(),
      window.end(),
      [](const MovementSoldierSample* lhs, const MovementSoldierSample* rhs) {
        return lhs->frame < rhs->frame;
      });

  std::ostringstream out;
  out << "frame   alpha   body(x,z)             ring(x,z)             error     "
         "mode         gait\n";
  for (auto const* sample : window) {
    float const error = std::max(std::fabs(sample->ring_root_x - sample->body_root_x),
                                 std::fabs(sample->ring_root_z - sample->body_root_z));
    out << text("%-7llu %6.3f (%9.4f,%9.4f) (%9.4f,%9.4f) %9.6f %-12s %5.2f\n",
                static_cast<unsigned long long>(sample->frame),
                static_cast<double>(sample->interpolation_alpha),
                static_cast<double>(sample->body_root_x),
                static_cast<double>(sample->body_root_z),
                static_cast<double>(sample->ring_root_x),
                static_cast<double>(sample->ring_root_z),
                static_cast<double>(error),
                traversal_layout_mode_name(sample->traversal_mode),
                static_cast<double>(sample->gait_speed));
  }
  return out.str();
}

} // namespace Engine::Core
