#include "formation_line_layout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "formation_frame.h"
#include "formation_row_split.h"
#include "formation_slot_adjust.h"

namespace Game::Formation::planning {

namespace {

constexpr float k_unit_gap = 0.9F;
constexpr float k_min_unit_gap = 0.45F;
constexpr float k_flank_gap_lanes = 2.5F;
constexpr float k_lateral_floor = 1.0F;

using MemberRefs = std::vector<const ArmyFormationMember*>;

struct AssignedLine {
  const DoctrineLineRule* rule{nullptr};
  MemberRefs members;
};

struct RowBudget {
  float base_spacing{1.0F};
  int row_cap{0};
  int rows_allowed{0};
};

struct LineMetrics {
  float slot_spacing{0.0F};
  float depth_unit{0.0F};
  RowBudget budget;
};

struct SlotSink {
  std::vector<FormationSlot>& out;
  int next_slot_id{0};
  float cursor_z{0.0F};
  Bounds all_bounds;
  Bounds body_bounds;
};

struct JitterSalt {
  std::uint64_t lateral_id;
  std::uint64_t lateral_row;
  std::uint64_t lateral_col;
  std::uint64_t depth_id;
  std::uint64_t depth_row;
  std::uint64_t depth_col;
};

constexpr JitterSalt k_centre_salt{
    2654435761ULL, 97ULL, 17ULL, 2246822519ULL, 53ULL, 29ULL};
constexpr JitterSalt k_flank_salt{
    1597334677ULL, 41ULL, 11ULL, 3812015801ULL, 73ULL, 31ULL};

struct RowPlan {
  float lateral_step{0.0F};
  float depth_step{0.0F};
  std::vector<int> rows;
};

auto signed_noise(std::uint64_t seed) -> float {
  auto value = static_cast<std::uint32_t>(seed ^ (seed >> 32U));
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return (static_cast<float>(value & 0xFFFFU) / 32767.5F) - 1.0F;
}

auto member_lateral_step(const ArmyFormationMember& member,
                         float base_spacing) -> float {
  return member.half_width * 2.0F + lane_for(base_spacing);
}

auto member_depth_step(const ArmyFormationMember& member, float base_spacing) -> float {
  return member.half_depth * 2.0F + lane_for(base_spacing);
}

auto max_lateral_step(const MemberRefs& members, float base_spacing) -> float {
  float step = base_spacing;
  for (const auto* member : members) {
    step = std::max(step, member_lateral_step(*member, base_spacing));
  }
  return step;
}

auto max_depth_step(const MemberRefs& members, float base_spacing) -> float {
  float step = base_spacing;
  for (const auto* member : members) {
    step = std::max(step, member_depth_step(*member, base_spacing));
  }
  return step;
}

auto bucket_members_by_rule(const DoctrineIntentTemplate& tmpl,
                            const std::vector<ArmyFormationMember>& members)
    -> std::vector<AssignedLine> {
  std::vector<AssignedLine> lines;
  lines.reserve(tmpl.lines.size());
  for (const auto& rule : tmpl.lines) {
    lines.push_back({&rule, {}});
  }
  for (const auto& member : members) {
    bool matched = false;
    for (auto& line : lines) {
      if (line.rule != nullptr && line.rule->matches(member.roles)) {
        line.members.push_back(&member);
        matched = true;
        break;
      }
    }
    if (!matched && !lines.empty()) {
      lines.back().members.push_back(&member);
    }
  }
  return lines;
}

void order_line_members(AssignedLine& line, const QVector3D& lateral_axis) {
  std::stable_sort(line.members.begin(),
                   line.members.end(),
                   [&](const ArmyFormationMember* a, const ArmyFormationMember* b) {
                     if (a->troop_type != b->troop_type) {
                       return static_cast<int>(a->troop_type) <
                              static_cast<int>(b->troop_type);
                     }
                     float const a_lateral =
                         QVector3D::dotProduct(a->current_position, lateral_axis);
                     float const b_lateral =
                         QVector3D::dotProduct(b->current_position, lateral_axis);
                     if (a_lateral != b_lateral) {
                       return a_lateral < b_lateral;
                     }
                     return a->entity_id < b->entity_id;
                   });
}

auto assign_lines(const DoctrineIntentTemplate& tmpl,
                  const std::vector<ArmyFormationMember>& members,
                  bool preserve_order,
                  float facing) -> std::vector<AssignedLine> {
  auto lines = bucket_members_by_rule(tmpl, members);
  if (preserve_order) {
    return lines;
  }
  float const yaw = facing * k_deg_to_rad;
  QVector3D const lateral_axis(std::cos(yaw), 0.0F, -std::sin(yaw));
  for (auto& line : lines) {
    order_line_members(line, lateral_axis);
  }
  return lines;
}

auto plan_rows(const DoctrineLineRule& rule,
               const MemberRefs& members,
               const RowBudget& budget) -> RowPlan {
  RowPlan plan;
  plan.lateral_step = max_lateral_step(members, budget.base_spacing) *
                      std::max(1.0F, rule.lateral_spacing_scale);
  plan.depth_step = max_depth_step(members, budget.base_spacing) *
                    std::max(1.0F, rule.depth_spacing_scale);
  int const abreast_for_depth =
      budget.rows_allowed > 0
          ? (static_cast<int>(members.size()) + budget.rows_allowed - 1) /
                budget.rows_allowed
          : 1;
  int const max_per_row = std::clamp(
      std::min(std::max(std::min(rule.max_per_row, budget.row_cap), abreast_for_depth),
               budget.row_cap),
      1,
      static_cast<int>(members.size()));
  int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
  plan.rows = balanced_rows(static_cast<int>(members.size()), max_per_row, min_per_row);
  return plan;
}

auto jitter_of(const ArmyFormationMember& member,
               std::size_t row,
               int col,
               const JitterSalt& salt,
               const RowPlan& plan,
               const DoctrineLineRule& rule) -> QVector3D {
  float const lateral_noise =
      signed_noise(member.entity_id * salt.lateral_id + row * salt.lateral_row +
                   static_cast<std::uint64_t>(col) * salt.lateral_col) *
      plan.lateral_step * rule.lateral_jitter_scale;
  float const depth_noise =
      signed_noise(member.entity_id * salt.depth_id + row * salt.depth_row +
                   static_cast<std::uint64_t>(col) * salt.depth_col) *
      plan.depth_step * rule.depth_jitter_scale;
  return {lateral_noise, 0.0F, depth_noise};
}

void emit_centre_block(const DoctrineLineRule& rule,
                       const MemberRefs& members,
                       const RowBudget& budget,
                       SlotSink& sink) {
  if (members.empty()) {
    return;
  }
  auto const plan = plan_rows(rule, members, budget);
  float const front_z = sink.cursor_z + rule.front_offset_scale * plan.depth_step;

  std::size_t index = 0;
  float rear_z = front_z;
  for (std::size_t row = 0; row < plan.rows.size() && index < members.size(); ++row) {
    int const in_row = plan.rows[row];
    float const row_z = front_z - static_cast<float>(row) * plan.depth_step;
    rear_z = std::min(rear_z, row_z);
    float const echelon =
        static_cast<float>(row) * plan.lateral_step * rule.row_echelon_scale;
    float const stagger =
        (row % 2U == 1U) ? plan.lateral_step * rule.row_stagger_scale : 0.0F;

    for (int col = 0; col < in_row && index < members.size(); ++col) {
      const auto* member = members[index];
      float const centred_col =
          static_cast<float>(col) - (static_cast<float>(in_row) - 1.0F) * 0.5F;
      QVector3D const noise = jitter_of(*member, row, col, k_centre_salt, plan, rule);

      FormationSlot slot;
      slot.id = sink.next_slot_id++;
      slot.role = rule.role;
      slot.local_offset =
          QVector3D(centred_col * plan.lateral_step + echelon + stagger + noise.x(),
                    0.0F,
                    row_z + noise.z());
      slot.rank = static_cast<int>(row);
      slot.file = col;
      slot.occupant = member->entity_id;
      sink.out.push_back(slot);
      sink.all_bounds.expand(slot.local_offset);
      sink.body_bounds.expand(slot.local_offset);
      ++index;
    }
  }

  if (rule.consumes_depth) {
    sink.cursor_z = rear_z - plan.depth_step * rule.line_gap_scale;
  }
}

auto flank_right_weight(const DoctrineLineRule& rule,
                        FlankPreference preference) -> float {
  switch (preference) {
  case FlankPreference::StrongLeft:
    return 0.35F;
  case FlankPreference::StrongRight:
    return 0.65F;
  case FlankPreference::Split:
    return 0.5F;
  case FlankPreference::Balanced:
    break;
  }
  return rule.right_side_weight;
}

auto left_flank_count(int total, float right_weight) -> int {
  int right_count =
      static_cast<int>(std::lround(static_cast<float>(total) * right_weight));
  right_count = std::clamp(right_count, 0, total);
  int left_count = total - right_count;
  if (total > 1) {
    left_count = std::max(1, left_count);
    right_count = std::max(1, total - left_count);
    left_count = total - right_count;
  }
  return left_count;
}

auto sorted_left_to_right(const MemberRefs& members,
                          const QVector3D& lateral_axis) -> MemberRefs {
  MemberRefs sorted = members;
  std::stable_sort(
      sorted.begin(),
      sorted.end(),
      [&lateral_axis](const ArmyFormationMember* a, const ArmyFormationMember* b) {
        float const a_lateral =
            QVector3D::dotProduct(a->current_position, lateral_axis);
        float const b_lateral =
            QVector3D::dotProduct(b->current_position, lateral_axis);
        if (a_lateral != b_lateral) {
          return a_lateral < b_lateral;
        }
        return a->entity_id < b->entity_id;
      });
  return sorted;
}

struct FlankSide {
  const MemberRefs& members;
  float sign;
  ArmyRole role;
};

struct FlankFrame {
  float front_anchor_z;
  float body_half_width;
};

void emit_flank_side(const DoctrineLineRule& rule,
                     const FlankSide& side,
                     const RowBudget& budget,
                     const FlankFrame& frame,
                     SlotSink& sink) {
  if (side.members.empty()) {
    return;
  }
  auto const plan = plan_rows(rule, side.members, budget);
  int const widest_flank_row =
      plan.rows.empty() ? 1 : *std::max_element(plan.rows.begin(), plan.rows.end());
  float const flank_half_width =
      (static_cast<float>(std::max(1, widest_flank_row) - 1) * plan.lateral_step *
       0.5F) +
      (plan.lateral_step * 0.5F);
  float const flank_lane =
      lane_for(budget.base_spacing) * k_flank_gap_lanes * rule.flank_gap_scale;
  float const flank_centre =
      side.sign * (frame.body_half_width + flank_lane + flank_half_width);

  std::size_t index = 0;
  for (std::size_t row = 0; row < plan.rows.size() && index < side.members.size();
       ++row) {
    int const in_row = plan.rows[row];
    float const row_z = frame.front_anchor_z +
                        rule.front_offset_scale * plan.depth_step -
                        static_cast<float>(row) * plan.depth_step;
    float const echelon = side.sign * static_cast<float>(row) * plan.lateral_step *
                          rule.row_echelon_scale;

    for (int col = 0; col < in_row && index < side.members.size(); ++col) {
      const auto* member = side.members[index];
      float const centred_col =
          static_cast<float>(col) - (static_cast<float>(in_row) - 1.0F) * 0.5F;
      QVector3D const noise = jitter_of(*member, row, col, k_flank_salt, plan, rule);

      FormationSlot slot;
      slot.id = sink.next_slot_id++;
      slot.role = side.role;
      slot.local_offset = QVector3D(
          flank_centre + centred_col * plan.lateral_step + echelon + noise.x(),
          0.0F,
          row_z + centred_col * plan.depth_step * rule.flank_forward_step_scale +
              noise.z());
      slot.rank = static_cast<int>(row);
      slot.file = col;
      slot.occupant = member->entity_id;
      sink.out.push_back(slot);
      sink.all_bounds.expand(slot.local_offset);
      ++index;
    }
  }
}

void emit_split_flanks(const DoctrineLineRule& rule,
                       const MemberRefs& members,
                       const RowBudget& budget,
                       const QVector3D& lateral_axis,
                       FlankPreference preference,
                       SlotSink& sink) {
  if (members.empty()) {
    return;
  }
  auto const sorted = sorted_left_to_right(members, lateral_axis);
  auto const total = static_cast<int>(sorted.size());
  int const left_count = left_flank_count(total, flank_right_weight(rule, preference));

  MemberRefs const left(sorted.begin(), sorted.begin() + left_count);
  MemberRefs const right(sorted.begin() + left_count, sorted.end());
  FlankFrame const frame{sink.body_bounds.max_z, sink.body_bounds.half_width()};
  emit_flank_side(rule, {left, -1.0F, ArmyRole::LeftFlank}, budget, frame, sink);
  emit_flank_side(rule, {right, 1.0F, ArmyRole::RightFlank}, budget, frame, sink);
}

auto measure_lines(const std::vector<AssignedLine>& lines,
                   const DoctrineIntentTemplate& tmpl,
                   float spacing,
                   float requested_frontage,
                   int row_cap_override) -> LineMetrics {
  LineMetrics metrics;
  metrics.budget.base_spacing = spacing;

  float slot_spacing = 0.0F;
  float widest_lateral = 0.0F;
  float depth_unit = spacing;
  for (const auto& line : lines) {
    if (line.rule == nullptr || line.members.empty()) {
      continue;
    }
    float const line_step = max_lateral_step(line.members, spacing) *
                            std::max(1.0F, line.rule->lateral_spacing_scale);
    slot_spacing = slot_spacing > 0.0F ? std::min(slot_spacing, line_step) : line_step;
    widest_lateral = std::max(widest_lateral, line_step);
    depth_unit = std::max(depth_unit, max_depth_step(line.members, spacing));
  }
  if (slot_spacing <= 0.0F) {
    slot_spacing = spacing;
  }
  metrics.slot_spacing = slot_spacing;
  metrics.depth_unit = depth_unit;

  if (tmpl.max_depth > 0.1F) {
    int stacked = 0;
    for (const auto& line : lines) {
      if (line.rule != nullptr && !line.members.empty() &&
          line.rule->placement != LinePlacement::SplitFlanks) {
        ++stacked;
      }
    }
    int const total_rows =
        std::max(1, static_cast<int>(tmpl.max_depth / std::max(0.5F, depth_unit)));
    metrics.budget.rows_allowed = std::max(1, total_rows / std::max(1, stacked));
  }

  float const widest_step = std::max(slot_spacing, widest_lateral);
  int row_cap = std::numeric_limits<int>::max();
  if (requested_frontage > 0.01F) {
    float const step = std::max(0.2F, slot_spacing);
    row_cap = std::max(1, static_cast<int>(std::floor(requested_frontage / step)) + 1);
  } else if (tmpl.max_frontage > 0.1F) {
    row_cap = std::max(
        1,
        static_cast<int>(std::floor(tmpl.max_frontage / std::max(0.2F, widest_step))));
  }
  metrics.budget.row_cap = row_cap_override > 0 ? row_cap_override : row_cap;
  return metrics;
}

void emit_lines(const std::vector<AssignedLine>& lines,
                const DoctrineIntentTemplate& tmpl,
                const ArmyFormationOptions& options,
                const RowBudget& budget,
                float facing,
                SlotSink& sink) {
  for (const auto& line : lines) {
    if (line.rule == nullptr || line.rule->placement == LinePlacement::SplitFlanks) {
      continue;
    }
    emit_centre_block(*line.rule, line.members, budget, sink);
  }

  FlankPreference preference = options.flank_preference;
  if (preference == FlankPreference::Balanced) {
    preference = tmpl.default_flank;
  }
  float const yaw = facing * k_deg_to_rad;
  QVector3D const lateral_axis(std::cos(yaw), 0.0F, -std::sin(yaw));

  for (const auto& line : lines) {
    if (line.rule == nullptr || line.rule->placement != LinePlacement::SplitFlanks) {
      continue;
    }
    if (!sink.body_bounds.valid) {
      DoctrineLineRule collapsed = *line.rule;
      collapsed.placement = LinePlacement::CentreBlock;
      collapsed.consumes_depth = true;
      collapsed.front_offset_scale = 0.0F;
      emit_centre_block(collapsed, line.members, budget, sink);
      continue;
    }
    emit_split_flanks(*line.rule, line.members, budget, lateral_axis, preference, sink);
  }
}

void settle_footprints(std::vector<FormationSlot>& slot_list,
                       const std::vector<ArmyFormationMember>& members,
                       float spacing) {
  std::unordered_map<EntityID, const ArmyFormationMember*> by_id;
  by_id.reserve(members.size());
  for (const auto& member : members) {
    by_id.emplace(member.entity_id, &member);
  }
  std::vector<float> half_width(slot_list.size(), 0.5F);
  std::vector<float> half_depth(slot_list.size(), 0.5F);
  for (std::size_t i = 0; i < slot_list.size(); ++i) {
    auto const found = by_id.find(slot_list[i].occupant);
    if (found == by_id.end()) {
      continue;
    }
    half_width[i] = found->second->half_width;
    half_depth[i] = found->second->half_depth;
  }
  separate_footprints(slot_list, half_width, half_depth, lane_for(spacing) * 0.5F);
}

void shape_and_settle(std::vector<FormationSlot>& slot_list,
                      const std::vector<ArmyFormationMember>& members,
                      const DoctrineIntentTemplate& tmpl,
                      const ArmyFormationOptions& options,
                      const LineMetrics& metrics,
                      float spacing,
                      float requested_frontage) {
  int const reserve_rows =
      options.reserve_rows >= 0 ? options.reserve_rows : tmpl.reserve_rows;
  apply_reserve_rows(slot_list, reserve_rows, metrics.depth_unit);

  RangedPlacement ranged = options.ranged_placement;
  if (ranged == RangedPlacement::Automatic) {
    ranged = tmpl.default_ranged;
  }
  apply_ranged_placement(slot_list, ranged, metrics.depth_unit);

  scale_to_frontage(slot_list,
                    requested_frontage,
                    tmpl.frontage_scale * options.frontage_scale,
                    tmpl.depth_scale * options.depth_scale,
                    k_lateral_floor,
                    requested_frontage > 0.01F ? 0.0F : tmpl.max_frontage,
                    tmpl.max_depth);

  settle_footprints(slot_list, members, spacing);
  recentre(slot_list);
}

} // namespace

auto lane_for(float base_spacing) -> float {
  return std::max(k_min_unit_gap, base_spacing * k_unit_gap);
}

auto plan_line_layout(const std::vector<ArmyFormationMember>& members,
                      const DoctrineIntentTemplate& tmpl,
                      const ArmyFormationOptions& options,
                      float spacing,
                      float requested_frontage,
                      float facing,
                      int row_cap_override) -> LineLayout {
  LineLayout layout;
  if (members.empty()) {
    return layout;
  }
  layout.slot_list.reserve(members.size());

  auto const lines = assign_lines(tmpl, members, options.preserve_member_order, facing);
  auto const metrics =
      measure_lines(lines, tmpl, spacing, requested_frontage, row_cap_override);
  layout.row_cap_used = metrics.budget.row_cap;

  SlotSink sink{layout.slot_list};
  emit_lines(lines, tmpl, options, metrics.budget, facing, sink);
  shape_and_settle(
      layout.slot_list, members, tmpl, options, metrics, spacing, requested_frontage);

  layout.slot_spacing = metrics.slot_spacing;
  return layout;
}

} // namespace Game::Formation::planning
