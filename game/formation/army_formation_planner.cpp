#include "army_formation_planner.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <optional>
#include <unordered_map>

#include "../core/component_core.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/formation_combat_geometry.h"
#include "../systems/nation_registry.h"
#include "../systems/nav_grid.h"
#include "../systems/pathfinding.h"
#include "../units/spawn_type.h"
#include "../units/troop_config.h"
#include "army_formation_registry.h"
#include "troop_role_registry.h"
#include "unit_layout_resolver.h"

namespace Game::Formation {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_deg_to_rad = k_pi / 180.0F;
struct Bounds {
  bool valid{false};
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};

  void expand(const QVector3D& point) {
    if (!valid) {
      valid = true;
      min_x = max_x = point.x();
      min_z = max_z = point.z();
      return;
    }
    min_x = std::min(min_x, point.x());
    max_x = std::max(max_x, point.x());
    min_z = std::min(min_z, point.z());
    max_z = std::max(max_z, point.z());
  }

  [[nodiscard]] auto width() const -> float { return valid ? max_x - min_x : 0.0F; }
  [[nodiscard]] auto depth() const -> float { return valid ? max_z - min_z : 0.0F; }
  [[nodiscard]] auto half_width() const -> float {
    return valid ? std::max(std::abs(min_x), std::abs(max_x)) : 0.0F;
  }
};

class Hasher {
public:
  void mix(std::uint64_t value) {
    m_state ^= value + 0x9e3779b97f4a7c15ULL + (m_state << 6U) + (m_state >> 2U);
  }

  void mix_float(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    mix(static_cast<std::uint64_t>(bits));
  }

  void mix(const std::string& value) {
    for (char const character : value) {
      mix(static_cast<std::uint64_t>(static_cast<unsigned char>(character)));
    }
    mix(value.size());
  }

  [[nodiscard]] auto value() const -> std::uint64_t { return m_state; }

private:
  std::uint64_t m_state{0xcbf29ce484222325ULL};
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

auto balanced_rows(int total, int max_per_row, int min_per_row) -> std::vector<int> {
  std::vector<int> rows;
  if (total <= 0) {
    return rows;
  }
  int const capped = std::max(1, max_per_row);
  if (total <= capped) {
    rows.push_back(total);
    return rows;
  }
  int const row_count = (total + capped - 1) / capped;
  int const base = total / row_count;
  int const extra = total % row_count;
  int assigned = 0;
  for (int r = 0; r < row_count; ++r) {
    int size = base + (r < extra ? 1 : 0);
    size = std::clamp(size, std::max(1, std::min(min_per_row, capped)), capped);
    size = std::min(size, total - assigned);
    if (size <= 0) {
      break;
    }
    rows.push_back(size);
    assigned += size;
  }
  while (assigned < total && !rows.empty()) {
    rows.back() += total - assigned;
    assigned = total;
  }
  return rows;
}

constexpr float k_unit_gap = 0.9F;
constexpr float k_min_unit_gap = 0.45F;

constexpr float k_flank_gap_lanes = 2.5F;

auto lane_for(float base_spacing) -> float {
  return std::max(k_min_unit_gap, base_spacing * k_unit_gap);
}

auto member_lateral_step(const ArmyFormationMember& member,
                         float base_spacing) -> float {
  return member.half_width * 2.0F + lane_for(base_spacing);
}

auto member_depth_step(const ArmyFormationMember& member, float base_spacing) -> float {
  return member.half_depth * 2.0F + lane_for(base_spacing);
}

auto max_lateral_step(const std::vector<const ArmyFormationMember*>& members,
                      float base_spacing) -> float {
  float step = base_spacing;
  for (const auto* member : members) {
    step = std::max(step, member_lateral_step(*member, base_spacing));
  }
  return step;
}

auto max_depth_step(const std::vector<const ArmyFormationMember*>& members,
                    float base_spacing) -> float {
  float step = base_spacing;
  for (const auto* member : members) {
    step = std::max(step, member_depth_step(*member, base_spacing));
  }
  return step;
}

struct AssignedLine {
  const DoctrineLineRule* rule{nullptr};
  std::vector<const ArmyFormationMember*> members;
};

auto assign_lines(const DoctrineIntentTemplate& tmpl,
                  const std::vector<ArmyFormationMember>& members,
                  bool preserve_order,
                  float facing) -> std::vector<AssignedLine> {
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

  float const yaw = facing * k_deg_to_rad;
  QVector3D const lateral_axis(std::cos(yaw), 0.0F, -std::sin(yaw));
  for (auto& line : lines) {
    if (preserve_order) {
      continue;
    }
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
  return lines;
}

void emit_centre_block(const DoctrineLineRule& rule,
                       const std::vector<const ArmyFormationMember*>& members,
                       float base_spacing,
                       int row_cap,
                       int rows_allowed,
                       float& cursor_z,
                       int& next_slot_id,
                       std::vector<FormationSlot>& out,
                       Bounds& all_bounds,
                       Bounds& body_bounds) {
  if (members.empty()) {
    return;
  }

  float const lateral_step = max_lateral_step(members, base_spacing) *
                             std::max(1.0F, rule.lateral_spacing_scale);
  float const depth_step =
      max_depth_step(members, base_spacing) * std::max(1.0F, rule.depth_spacing_scale);
  int const abreast_for_depth =
      rows_allowed > 0
          ? (static_cast<int>(members.size()) + rows_allowed - 1) / rows_allowed
          : 1;

  int const max_per_row = std::clamp(
      std::min(std::max(std::min(rule.max_per_row, row_cap), abreast_for_depth),
               row_cap),
      1,
      static_cast<int>(members.size()));
  int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
  auto const rows =
      balanced_rows(static_cast<int>(members.size()), max_per_row, min_per_row);
  float const front_z = cursor_z + rule.front_offset_scale * depth_step;

  std::size_t index = 0;
  float rear_z = front_z;
  for (std::size_t row = 0; row < rows.size() && index < members.size(); ++row) {
    int const in_row = rows[row];
    float const row_z = front_z - static_cast<float>(row) * depth_step;
    rear_z = std::min(rear_z, row_z);
    float const echelon =
        static_cast<float>(row) * lateral_step * rule.row_echelon_scale;
    float const stagger =
        (row % 2U == 1U) ? lateral_step * rule.row_stagger_scale : 0.0F;

    for (int col = 0; col < in_row && index < members.size(); ++col) {
      const auto* member = members[index];
      float const centred_col =
          static_cast<float>(col) - (static_cast<float>(in_row) - 1.0F) * 0.5F;
      float const lateral_noise =
          signed_noise(member->entity_id * 2654435761ULL + row * 97ULL +
                       static_cast<std::uint64_t>(col) * 17ULL) *
          lateral_step * rule.lateral_jitter_scale;
      float const depth_noise =
          signed_noise(member->entity_id * 2246822519ULL + row * 53ULL +
                       static_cast<std::uint64_t>(col) * 29ULL) *
          depth_step * rule.depth_jitter_scale;

      FormationSlot slot;
      slot.id = next_slot_id++;
      slot.role = rule.role;
      slot.local_offset =
          QVector3D(centred_col * lateral_step + echelon + stagger + lateral_noise,
                    0.0F,
                    row_z + depth_noise);
      slot.facing = 0.0F;
      slot.rank = static_cast<int>(row);
      slot.file = col;
      slot.occupant = member->entity_id;
      out.push_back(slot);
      all_bounds.expand(slot.local_offset);
      body_bounds.expand(slot.local_offset);
      ++index;
    }
  }

  if (rule.consumes_depth) {
    cursor_z = rear_z - depth_step * rule.line_gap_scale;
  }
}

void emit_split_flanks(const DoctrineLineRule& rule,
                       const std::vector<const ArmyFormationMember*>& members,
                       float base_spacing,
                       int row_cap,
                       int rows_allowed,
                       const QVector3D& lateral_axis,
                       float front_anchor_z,
                       float body_half_width,
                       FlankPreference preference,
                       int& next_slot_id,
                       std::vector<FormationSlot>& out,
                       Bounds& all_bounds) {
  if (members.empty()) {
    return;
  }

  std::vector<const ArmyFormationMember*> sorted = members;
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

  float right_weight = rule.right_side_weight;
  switch (preference) {
  case FlankPreference::StrongLeft:
    right_weight = 0.35F;
    break;
  case FlankPreference::StrongRight:
    right_weight = 0.65F;
    break;
  case FlankPreference::Split:
    right_weight = 0.5F;
    break;
  case FlankPreference::Balanced:
    break;
  }

  auto const total = static_cast<int>(sorted.size());
  int right_count =
      static_cast<int>(std::lround(static_cast<float>(total) * right_weight));
  right_count = std::clamp(right_count, 0, total);
  int left_count = total - right_count;
  if (total > 1) {
    left_count = std::max(1, left_count);
    right_count = std::max(1, total - left_count);
    left_count = total - right_count;
  }

  std::vector<const ArmyFormationMember*> const left(sorted.begin(),
                                                     sorted.begin() + left_count);
  std::vector<const ArmyFormationMember*> const right(sorted.begin() + left_count,
                                                      sorted.end());

  auto emit_side = [&](const std::vector<const ArmyFormationMember*>& side,
                       float side_sign,
                       ArmyRole role) {
    if (side.empty()) {
      return;
    }
    float const lateral_step = max_lateral_step(side, base_spacing) *
                               std::max(1.0F, rule.lateral_spacing_scale);
    float const depth_step =
        max_depth_step(side, base_spacing) * std::max(1.0F, rule.depth_spacing_scale);
    float const spacing = lateral_step;
    int const abreast_for_depth =
        rows_allowed > 0
            ? (static_cast<int>(side.size()) + rows_allowed - 1) / rows_allowed
            : 1;
    int const max_per_row = std::clamp(
        std::min(std::max(std::min(rule.max_per_row, row_cap), abreast_for_depth),
                 row_cap),
        1,
        static_cast<int>(side.size()));
    int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
    auto const rows =
        balanced_rows(static_cast<int>(side.size()), max_per_row, min_per_row);

    int const widest_flank_row =
        rows.empty() ? 1 : *std::max_element(rows.begin(), rows.end());
    float const flank_half_width =
        (static_cast<float>(std::max(1, widest_flank_row) - 1) * lateral_step * 0.5F) +
        (lateral_step * 0.5F);
    float const flank_lane =
        lane_for(base_spacing) * k_flank_gap_lanes * rule.flank_gap_scale;
    float const flank_centre =
        side_sign * (body_half_width + flank_lane + flank_half_width);

    std::size_t index = 0;
    for (std::size_t row = 0; row < rows.size() && index < side.size(); ++row) {
      int const in_row = rows[row];
      float const row_z = front_anchor_z + rule.front_offset_scale * depth_step -
                          static_cast<float>(row) * depth_step;
      float const echelon =
          side_sign * static_cast<float>(row) * lateral_step * rule.row_echelon_scale;

      for (int col = 0; col < in_row && index < side.size(); ++col) {
        const auto* member = side[index];
        float const centred_col =
            static_cast<float>(col) - (static_cast<float>(in_row) - 1.0F) * 0.5F;
        float const lateral_noise =
            signed_noise(member->entity_id * 1597334677ULL + row * 41ULL +
                         static_cast<std::uint64_t>(col) * 11ULL) *
            lateral_step * rule.lateral_jitter_scale;
        float const depth_noise =
            signed_noise(member->entity_id * 3812015801ULL + row * 73ULL +
                         static_cast<std::uint64_t>(col) * 31ULL) *
            depth_step * rule.depth_jitter_scale;

        FormationSlot slot;
        slot.id = next_slot_id++;
        slot.role = role;
        slot.local_offset = QVector3D(
            flank_centre + centred_col * lateral_step + echelon + lateral_noise,
            0.0F,
            row_z + centred_col * depth_step * rule.flank_forward_step_scale +
                depth_noise);
        slot.facing = 0.0F;
        slot.rank = static_cast<int>(row);
        slot.file = col;
        slot.occupant = member->entity_id;
        out.push_back(slot);
        all_bounds.expand(slot.local_offset);
        ++index;
      }
    }
  };

  emit_side(left, -1.0F, ArmyRole::LeftFlank);
  emit_side(right, 1.0F, ArmyRole::RightFlank);
}

void resolve_overlaps(std::vector<FormationSlot>& slot_list,
                      const std::vector<float>& half_width,
                      const std::vector<float>& half_depth,
                      float gap) {
  constexpr int k_passes = 12;
  auto const count = slot_list.size();
  if (count < 2U || half_width.size() != count || half_depth.size() != count) {
    return;
  }

  for (int pass = 0; pass < k_passes; ++pass) {
    bool moved = false;
    for (std::size_t i = 0; i < count; ++i) {
      for (std::size_t j = i + 1; j < count; ++j) {
        float const dx = slot_list[j].local_offset.x() - slot_list[i].local_offset.x();
        float const dz = slot_list[j].local_offset.z() - slot_list[i].local_offset.z();
        float const need_x = half_width[i] + half_width[j] + gap;
        float const need_z = half_depth[i] + half_depth[j] + gap;
        float const over_x = need_x - std::abs(dx);
        float const over_z = need_z - std::abs(dz);
        if (over_x <= 0.0F || over_z <= 0.0F) {
          continue;
        }
        moved = true;

        if (over_x <= over_z) {
          float const push = (dx >= 0.0F ? over_x : -over_x) * 0.5F;
          slot_list[i].local_offset.setX(slot_list[i].local_offset.x() - push);
          slot_list[j].local_offset.setX(slot_list[j].local_offset.x() + push);
        } else {
          float const push = (dz >= 0.0F ? over_z : -over_z) * 0.5F;
          slot_list[i].local_offset.setZ(slot_list[i].local_offset.z() - push);
          slot_list[j].local_offset.setZ(slot_list[j].local_offset.z() + push);
        }
      }
    }
    if (!moved) {
      return;
    }
  }
}

constexpr float k_overlap_epsilon = 1.0e-3F;

auto local_overlap(float dx,
                   float dz,
                   float half_width_a,
                   float half_depth_a,
                   float half_width_b,
                   float half_depth_b,
                   float gap) -> bool {
  return std::abs(dx) + k_overlap_epsilon < half_width_a + half_width_b + gap &&
         std::abs(dz) + k_overlap_epsilon < half_depth_a + half_depth_b + gap;
}

void separate_footprints(std::vector<FormationSlot>& slot_list,
                         const std::vector<float>& half_width,
                         const std::vector<float>& half_depth,
                         float gap) {
  auto const count = slot_list.size();
  if (count < 2U || half_width.size() != count || half_depth.size() != count) {
    return;
  }
  resolve_overlaps(slot_list, half_width, half_depth, gap);

  std::vector<std::size_t> order(count);
  for (std::size_t i = 0; i < count; ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    if (slot_list[a].local_offset.z() != slot_list[b].local_offset.z()) {
      return slot_list[a].local_offset.z() > slot_list[b].local_offset.z();
    }
    return slot_list[a].local_offset.x() < slot_list[b].local_offset.x();
  });

  std::vector<std::size_t> settled;
  settled.reserve(count);
  for (auto const index : order) {
    auto& slot = slot_list[index];
    for (std::size_t guard = 0; guard <= settled.size(); ++guard) {
      bool pushed = false;
      for (auto const other_index : settled) {
        const auto& other = slot_list[other_index];
        float const dx = slot.local_offset.x() - other.local_offset.x();
        float const dz = slot.local_offset.z() - other.local_offset.z();
        if (!local_overlap(dx,
                           dz,
                           half_width[index],
                           half_depth[index],
                           half_width[other_index],
                           half_depth[other_index],
                           gap)) {
          continue;
        }
        slot.local_offset.setZ(other.local_offset.z() -
                               (half_depth[index] + half_depth[other_index] + gap));
        pushed = true;
        break;
      }
      if (!pushed) {
        break;
      }
    }
    settled.push_back(index);
  }
}

auto widest_rank(const std::vector<FormationSlot>& slot_list, float spacing) -> int {
  if (slot_list.empty()) {
    return 0;
  }
  std::vector<float> depths;
  depths.reserve(slot_list.size());
  for (const auto& slot : slot_list) {
    depths.push_back(slot.local_offset.z());
  }
  std::sort(depths.begin(), depths.end(), std::greater<>());
  float const band = std::max(spacing, 0.2F) * 0.5F;
  int widest = 0;
  int in_band = 0;
  float start = depths.front();
  for (float const depth : depths) {
    if (std::abs(depth - start) > band) {
      widest = std::max(widest, in_band);
      start = depth;
      in_band = 0;
    }
    ++in_band;
  }
  return std::max(widest, in_band);
}

void recentre_on_centroid(std::vector<FormationSlot>& slot_list) {
  if (slot_list.empty()) {
    return;
  }
  float cx = 0.0F;
  float cz = 0.0F;
  for (const auto& slot : slot_list) {
    cx += slot.local_offset.x();
    cz += slot.local_offset.z();
  }
  cx /= static_cast<float>(slot_list.size());
  cz /= static_cast<float>(slot_list.size());
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() - cx);
    slot.local_offset.setY(0.0F);
    slot.local_offset.setZ(slot.local_offset.z() - cz);
  }
}

void recentre(std::vector<FormationSlot>& slot_list) {
  if (slot_list.empty()) {
    return;
  }
  Bounds bounds;
  for (const auto& slot : slot_list) {
    bounds.expand(slot.local_offset);
  }
  if (!bounds.valid) {
    return;
  }
  float const cx = (bounds.min_x + bounds.max_x) * 0.5F;
  float const cz = (bounds.min_z + bounds.max_z) * 0.5F;
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() - cx);
    slot.local_offset.setY(0.0F);
    slot.local_offset.setZ(slot.local_offset.z() - cz);
  }
}

void apply_reserve_rows(std::vector<FormationSlot>& slot_list,
                        int reserve_rows,
                        float spacing) {
  if (reserve_rows <= 0 || slot_list.empty()) {
    return;
  }

  std::vector<FormationSlot*> ordered;
  ordered.reserve(slot_list.size());
  for (auto& slot : slot_list) {
    ordered.push_back(&slot);
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
    return a->local_offset.z() < b->local_offset.z();
  });

  int bands = 0;
  float previous = ordered.front()->local_offset.z();
  std::size_t count = 0;
  for (auto* slot : ordered) {
    if (std::abs(slot->local_offset.z() - previous) > spacing * 0.5F) {
      ++bands;
      previous = slot->local_offset.z();
      if (bands >= reserve_rows) {
        break;
      }
    }
    ++count;
  }

  if (count >= ordered.size()) {
    return;
  }

  for (std::size_t i = 0; i < count; ++i) {
    auto* slot = ordered[i];
    slot->local_offset.setZ(slot->local_offset.z() - spacing * 1.15F);
    if (slot->role == ArmyRole::Centre || slot->role == ArmyRole::Vanguard ||
        slot->role == ArmyRole::Screen) {
      slot->role = ArmyRole::Reserve;
    }
  }
}

void apply_ranged_placement(std::vector<FormationSlot>& slot_list,
                            RangedPlacement placement,
                            float spacing) {
  Bounds body;
  Bounds ranged;
  for (const auto& slot : slot_list) {
    if (slot.role == ArmyRole::Ranged) {
      ranged.expand(slot.local_offset);
    } else if (slot.role == ArmyRole::Centre || slot.role == ArmyRole::Screen ||
               slot.role == ArmyRole::Vanguard) {
      body.expand(slot.local_offset);
    }
  }
  if (!ranged.valid || !body.valid) {
    return;
  }

  float shift = 0.0F;
  float lateral_spread = 1.0F;
  switch (placement) {
  case RangedPlacement::Front:

    shift = (body.max_z + spacing * 0.9F) - ranged.min_z;
    break;
  case RangedPlacement::Skirmish:

    shift = (body.max_z + spacing * 2.2F) - ranged.min_z;
    lateral_spread = 1.45F;
    break;
  case RangedPlacement::Rear:
  case RangedPlacement::Automatic:

    shift = (body.min_z - spacing * 0.9F) - ranged.max_z;
    break;
  }

  for (auto& slot : slot_list) {
    if (slot.role != ArmyRole::Ranged) {
      continue;
    }
    slot.local_offset.setZ(slot.local_offset.z() + shift);
    slot.local_offset.setX(slot.local_offset.x() * lateral_spread);
  }
}

void scale_to_frontage(std::vector<FormationSlot>& slot_list,
                       float requested_frontage,
                       float frontage_scale,
                       float depth_scale,
                       float lateral_floor,
                       float max_frontage,
                       float max_depth) {
  if (slot_list.empty()) {
    return;
  }
  Bounds bounds;
  for (const auto& slot : slot_list) {
    bounds.expand(slot.local_offset);
  }
  float lateral_scale = frontage_scale;
  if (requested_frontage > 0.01F && bounds.width() > 0.01F) {

    lateral_scale = std::max(1.0F, requested_frontage / bounds.width());
  }

  if (max_frontage > 0.1F && bounds.width() > 0.01F) {
    lateral_scale = std::min(lateral_scale, max_frontage / bounds.width());
  }
  lateral_scale = std::clamp(std::max(lateral_scale, lateral_floor), 0.25F, 6.0F);

  float depth_multiplier = depth_scale;
  if (max_depth > 0.1F && bounds.depth() > 0.01F) {
    depth_multiplier = std::min(depth_multiplier, max_depth / bounds.depth());
  }
  depth_multiplier = std::clamp(depth_multiplier, 1.0F, 6.0F);
  for (auto& slot : slot_list) {
    slot.local_offset.setX(slot.local_offset.x() * lateral_scale);
    slot.local_offset.setZ(slot.local_offset.z() * depth_multiplier);
  }
}

auto rotate_offset(const QVector3D& local, float yaw_degrees) -> QVector3D {
  float const yaw = yaw_degrees * k_deg_to_rad;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {local.x() * cos_yaw + local.z() * sin_yaw,
          local.y(),
          -local.x() * sin_yaw + local.z() * cos_yaw};
}

struct FrameAxes {
  QVector3D lateral{1.0F, 0.0F, 0.0F};
  QVector3D depth{0.0F, 0.0F, 1.0F};
};

auto frame_axes(float facing_degrees) -> FrameAxes {
  return {rotate_offset(QVector3D(1.0F, 0.0F, 0.0F), facing_degrees),
          rotate_offset(QVector3D(0.0F, 0.0F, 1.0F), facing_degrees)};
}

class FootprintClaims {
public:
  FootprintClaims(float cell, const FrameAxes& axes, float gap)
      : m_cell(std::max(cell, 0.5F))
      , m_axes(axes)
      , m_gap(gap) {}

  [[nodiscard]] auto
  is_free(const QVector3D& point, float half_width, float half_depth) const -> bool {
    float const reach_metres =
        std::max(half_width, half_depth) + m_largest_extent + m_gap;
    int const reach = static_cast<int>(std::ceil(reach_metres * 1.415F / m_cell)) + 1;
    auto const cell_x = to_cell(point.x());
    auto const cell_z = to_cell(point.z());
    for (int dx = -reach; dx <= reach; ++dx) {
      for (int dz = -reach; dz <= reach; ++dz) {
        auto const it = m_cells.find(key(cell_x + dx, cell_z + dz));
        if (it == m_cells.end()) {
          continue;
        }
        for (const auto& claimed : it->second) {
          QVector3D const offset = point - claimed.point;
          if (local_overlap(QVector3D::dotProduct(offset, m_axes.lateral),
                            QVector3D::dotProduct(offset, m_axes.depth),
                            half_width,
                            half_depth,
                            claimed.half_width,
                            claimed.half_depth,
                            m_gap)) {
            return false;
          }
        }
      }
    }
    return true;
  }

  void claim(const QVector3D& point, float half_width, float half_depth) {
    m_cells[key(to_cell(point.x()), to_cell(point.z()))].push_back(
        {point, half_width, half_depth});
    m_largest_extent = std::max(m_largest_extent, std::max(half_width, half_depth));
  }

  void reserve(std::size_t count) { m_cells.reserve(count * 2U); }

private:
  [[nodiscard]] auto to_cell(float value) const -> std::int32_t {
    return static_cast<std::int32_t>(std::floor(value / m_cell));
  }

  [[nodiscard]] static auto key(std::int32_t cell_x,
                                std::int32_t cell_z) -> std::uint64_t {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x)) << 32U) |
           static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_z));
  }

  struct Claim {
    QVector3D point;
    float half_width{0.0F};
    float half_depth{0.0F};
  };

  std::unordered_map<std::uint64_t, std::vector<Claim>> m_cells;
  float m_cell{1.0F};
  FrameAxes m_axes;
  float m_gap{0.0F};
  float m_largest_extent{0.0F};
};

class SlotTerrainFitter {
public:
  SlotTerrainFitter(float spacing,
                    float gap,
                    float facing,
                    const QVector3D& anchor,
                    bool enabled,
                    std::size_t expected_slots)
      : m_claims(std::max(spacing, 0.5F), frame_axes(facing), gap)
      , m_axes(frame_axes(facing))
      , m_separation(spacing)
      , m_gap(gap)
      , m_anchor(anchor)
      , m_enabled(enabled)
      , m_pathfinder(enabled ? Game::Systems::NavGrid::get_pathfinder() : nullptr) {
    m_claims.reserve(expected_slots);
  }

  auto fit(const QVector3D& ideal,
           float half_width,
           float half_depth,
           bool heavy,
           SlotStatus& status) -> QVector3D {
    if (!m_enabled) {
      m_claims.claim(ideal, half_width, half_depth);
      status = SlotStatus::Valid;
      return ideal;
    }

    if (acceptable(ideal, half_width, half_depth, heavy)) {
      m_claims.claim(ideal, half_width, half_depth);
      status = SlotStatus::Valid;
      return ideal;
    }

    constexpr int k_max_rings = 8;
    constexpr int k_samples = 12;
    float const reach = max_displacement(half_width, half_depth);
    float const step =
        std::max(0.75F, (std::min(half_width, half_depth) * 2.0F + m_gap) * 0.5F);
    int const rings =
        std::clamp(static_cast<int>(std::ceil(reach / step)), 1, k_max_rings);
    for (int ring = 1; ring <= rings; ++ring) {
      float const radius = reach * static_cast<float>(ring) / static_cast<float>(rings);
      for (int sample = 0; sample < k_samples; ++sample) {
        int const side = (sample % 2 == 0) ? 1 : -1;
        float const turn = static_cast<float>((sample + 1) / 2) * (2.0F * k_pi) /
                           static_cast<float>(k_samples);
        float const angle = k_pi + static_cast<float>(side) * turn;
        QVector3D const direction =
            m_axes.depth * std::cos(angle) + m_axes.lateral * std::sin(angle);
        QVector3D const candidate(ideal.x() + direction.x() * radius,
                                  ideal.y(),
                                  ideal.z() + direction.z() * radius);
        if (!acceptable(candidate, half_width, half_depth, heavy)) {
          continue;
        }
        m_claims.claim(candidate, half_width, half_depth);
        status = SlotStatus::Adjusted;
        return candidate;
      }
    }

    auto const origin = Game::Systems::NavGrid::world_to_grid(ideal.x(), ideal.z());
    if (auto const nearest =
            Game::Systems::NavGrid::find_nearest_walkable_grid(origin, k_wide_cells)) {
      QVector3D const grounded = Game::Systems::NavGrid::grid_to_world(*nearest);
      QVector3D const candidate(grounded.x(), ideal.y(), grounded.z());
      QVector3D const shift(candidate.x() - ideal.x(), 0.0F, candidate.z() - ideal.z());
      if (shift.length() <= reach &&
          acceptable(candidate, half_width, half_depth, heavy)) {
        m_claims.claim(candidate, half_width, half_depth);
        status = SlotStatus::Adjusted;
        return candidate;
      }
    }

    status = SlotStatus::Blocked;
    return ideal;
  }

private:
  static constexpr int k_wide_cells = 12;

  static constexpr float k_footprint_inset = 0.5F;
  static constexpr int k_max_samples_per_axis = 6;

  [[nodiscard]] auto max_displacement(float half_width,
                                      float half_depth) const -> float {
    return std::max(m_separation, std::max(half_width, half_depth) * 2.0F + m_gap);
  }

  [[nodiscard]] auto acceptable(const QVector3D& centre,
                                float half_width,
                                float half_depth,
                                bool heavy) -> bool {
    return m_claims.is_free(centre, half_width, half_depth) &&
           footprint_walkable(centre, half_width, half_depth, heavy) &&
           connected_to_anchor(centre, heavy);
  }

  [[nodiscard]] auto walkable(const QVector3D& point, bool heavy) const -> bool {
    if (m_pathfinder == nullptr) {
      return Game::Systems::NavGrid::is_world_position_walkable(point);
    }
    return m_pathfinder->is_world_position_walkable(
        point,
        heavy ? Game::Systems::Pathfinding::Passability::Heavy
              : Game::Systems::Pathfinding::Passability::Light);
  }

  [[nodiscard]] auto footprint_walkable(const QVector3D& centre,
                                        float half_width,
                                        float half_depth,
                                        bool heavy) const -> bool {
    if (!walkable(centre, heavy)) {
      return false;
    }
    float const sample_step = std::max(
        0.75F, m_pathfinder != nullptr ? m_pathfinder->grid_cell_size() : 1.0F);
    float const reach_x = half_width * k_footprint_inset;
    float const reach_z = half_depth * k_footprint_inset;
    int const across =
        std::clamp(static_cast<int>(std::ceil(reach_x * 2.0F / sample_step)) + 1,
                   2,
                   k_max_samples_per_axis);
    int const along =
        std::clamp(static_cast<int>(std::ceil(reach_z * 2.0F / sample_step)) + 1,
                   2,
                   k_max_samples_per_axis);
    for (int i = 0; i < across; ++i) {
      float const u = -reach_x + (2.0F * reach_x) * static_cast<float>(i) /
                                     static_cast<float>(across - 1);
      for (int j = 0; j < along; ++j) {
        float const v = -reach_z + (2.0F * reach_z) * static_cast<float>(j) /
                                       static_cast<float>(along - 1);
        QVector3D const point = centre + m_axes.lateral * u + m_axes.depth * v;
        if (!walkable(point, heavy)) {
          return false;
        }
      }
    }
    return true;
  }

  [[nodiscard]] auto connected_to_anchor(const QVector3D& centre, bool heavy) -> bool {
    if (m_pathfinder == nullptr) {
      return true;
    }
    auto const passability = heavy ? Game::Systems::Pathfinding::Passability::Heavy
                                   : Game::Systems::Pathfinding::Passability::Light;
    auto& anchor_region = heavy ? m_heavy_anchor_region : m_light_anchor_region;
    if (!anchor_region.has_value()) {
      anchor_region = m_pathfinder->region_of(
          m_pathfinder->world_to_grid(m_anchor.x(), m_anchor.z()), passability);
    }
    if (*anchor_region == Game::Systems::Pathfinding::k_unreachable_region) {
      return true;
    }
    return m_pathfinder->region_of(m_pathfinder->world_to_grid(centre.x(), centre.z()),
                                   passability) == *anchor_region;
  }

  FootprintClaims m_claims;
  FrameAxes m_axes;
  float m_separation{1.0F};
  float m_gap{0.0F};
  QVector3D m_anchor;
  bool m_enabled{true};
  Game::Systems::Pathfinding* m_pathfinder{nullptr};
  std::optional<std::uint32_t> m_light_anchor_region;
  std::optional<std::uint32_t> m_heavy_anchor_region;
};

auto doctrine_for_entity(Engine::Core::World& world,
                         EntityID id) -> FormationDoctrineId {
  auto* entity = world.get_entity(id);
  if (entity == nullptr) {
    return k_neutral_doctrine;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return k_neutral_doctrine;
  }
  const auto* nation =
      Game::Systems::NationRegistry::instance().get_nation(unit->nation_id);
  if (nation != nullptr && !nation->doctrine.empty()) {
    return nation->doctrine;
  }
  return default_doctrine_for_nation(unit->nation_id);
}

} // namespace

auto ArmyFormationPlan::slot_for(EntityID entity) const -> const FormationSlot* {
  auto it =
      std::find_if(slot_list.begin(), slot_list.end(), [entity](const auto& slot) {
        return slot.occupant == entity;
      });
  return it == slot_list.end() ? nullptr : &(*it);
}

auto ArmyFormationPlan::placed_count() const -> int {
  return static_cast<int>(
      std::count_if(slot_list.begin(), slot_list.end(), [](const auto& s) {
        return s.status != SlotStatus::Blocked;
      }));
}

auto ArmyFormationPlan::depth_bands() const -> std::vector<int> {
  std::vector<int> bands;
  if (slot_list.empty()) {
    return bands;
  }

  std::vector<float> depths;
  depths.reserve(slot_list.size());
  for (const auto& slot : slot_list) {
    depths.push_back(slot.local_offset.z());
  }
  std::sort(depths.begin(), depths.end(), std::greater<>());

  float const band_gap = std::max(slot_spacing, 0.2F) * 0.5F;
  float band_start = depths.front();
  int in_band = 0;
  for (float const depth : depths) {
    if (std::abs(depth - band_start) > band_gap) {
      bands.push_back(in_band);
      band_start = depth;
      in_band = 0;
    }
    ++in_band;
  }
  bands.push_back(in_band);
  return bands;
}

auto ArmyFormationPlan::rank_count() const -> int {
  return static_cast<int>(depth_bands().size());
}

auto ArmyFormationPlan::file_count() const -> int {
  auto const bands = depth_bands();
  return bands.empty() ? 0 : *std::max_element(bands.begin(), bands.end());
}

auto ArmyFormationPlanner::collect_members(Engine::Core::World& world,
                                           const std::vector<EntityID>& entities)
    -> std::vector<ArmyFormationMember> {
  std::vector<ArmyFormationMember> members;
  members.reserve(entities.size());

  for (auto const id : entities) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }
    auto troop = Game::Units::spawn_typeToTroopType(unit->spawn_type);
    if (!troop.has_value()) {
      continue;
    }

    ArmyFormationMember member;
    member.entity_id = id;
    member.troop_type = *troop;
    member.roles = TroopRoleRegistry::instance().roles(*troop);
    member.current_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    member.footprint =
        Game::Units::TroopConfig::instance().get_selection_ring_size(*troop);
    measure_footprint(*entity, member.footprint, member);
    if (const auto* movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      member.heavy = !movement->get_can_enter_forest();
    }
    member.doctrine = doctrine_for_entity(world, id);
    members.push_back(member);
  }

  return members;
}

auto ArmyFormationPlanner::combined_roles(
    const std::vector<ArmyFormationMember>& members) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (const auto& member : members) {
    set |= member.roles;
  }
  return set;
}

auto ArmyFormationPlanner::resolve_doctrine(
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request) -> FormationDoctrineId {
  if (!request.doctrine.empty() && request.options.doctrine_locked) {
    return request.doctrine;
  }
  if (members.empty()) {
    return request.doctrine.empty() ? k_neutral_doctrine : request.doctrine;
  }

  switch (request.options.mixed_policy) {
  case MixedDoctrinePolicy::CompositeByRole:
    return k_neutral_doctrine;

  case MixedDoctrinePolicy::CommanderDoctrine: {
    for (const auto& member : members) {
      if (has_role(member.roles, RoleTag::Command)) {
        return member.doctrine;
      }
    }
    break;
  }

  case MixedDoctrinePolicy::SeparateContingents:
  case MixedDoctrinePolicy::MajorityDoctrine:
    break;
  }

  std::unordered_map<FormationDoctrineId, int> counts;
  for (const auto& member : members) {
    ++counts[member.doctrine];
  }
  FormationDoctrineId best = members.front().doctrine;
  int best_count = 0;
  for (const auto& entry : counts) {
    if (entry.second > best_count ||
        (entry.second == best_count && entry.first < best)) {
      best = entry.first;
      best_count = entry.second;
    }
  }
  if (!request.doctrine.empty() && counts.count(request.doctrine) != 0U) {
    return request.doctrine;
  }
  return best;
}

auto ArmyFormationPlanner::plan_local_slots(
    const std::vector<ArmyFormationMember>& members,
    const DoctrineIntentTemplate& tmpl,
    const ArmyFormationOptions& options,
    float spacing,
    float requested_frontage,
    float facing,
    float* slot_spacing_out,
    int row_cap_override,
    int* row_cap_used) -> std::vector<FormationSlot> {
  std::vector<FormationSlot> slot_list;
  if (members.empty()) {
    return slot_list;
  }
  slot_list.reserve(members.size());

  auto lines = assign_lines(tmpl, members, options.preserve_member_order, facing);

  float slot_spacing = 0.0F;
  for (const auto& line : lines) {
    if (line.rule == nullptr || line.members.empty()) {
      continue;
    }
    float const line_step = max_lateral_step(line.members, spacing) *
                            std::max(1.0F, line.rule->lateral_spacing_scale);
    slot_spacing = slot_spacing > 0.0F ? std::min(slot_spacing, line_step) : line_step;
  }
  if (slot_spacing <= 0.0F) {
    slot_spacing = spacing;
  }

  constexpr float k_lateral_floor = 1.0F;

  float depth_unit = spacing;
  for (const auto& line : lines) {
    if (line.rule == nullptr || line.members.empty()) {
      continue;
    }
    depth_unit = std::max(depth_unit, max_depth_step(line.members, spacing));
  }

  int rows_allowed = 0;
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
    rows_allowed = std::max(1, total_rows / std::max(1, stacked));
  }

  float widest_step = slot_spacing;
  for (const auto& line : lines) {
    if (line.rule == nullptr || line.members.empty()) {
      continue;
    }
    widest_step = std::max(widest_step,
                           max_lateral_step(line.members, spacing) *
                               std::max(1.0F, line.rule->lateral_spacing_scale));
  }

  int row_cap = std::numeric_limits<int>::max();
  if (requested_frontage > 0.01F) {
    float const step = std::max(0.2F, slot_spacing);
    row_cap = std::max(1, static_cast<int>(std::floor(requested_frontage / step)) + 1);
  } else if (tmpl.max_frontage > 0.1F) {
    row_cap = std::max(
        1,
        static_cast<int>(std::floor(tmpl.max_frontage / std::max(0.2F, widest_step))));
  }
  if (row_cap_override > 0) {
    row_cap = row_cap_override;
  }
  if (row_cap_used != nullptr) {
    *row_cap_used = row_cap;
  }
  float const yaw = facing * k_deg_to_rad;
  QVector3D const lateral_axis(std::cos(yaw), 0.0F, -std::sin(yaw));

  Bounds all_bounds;
  Bounds body_bounds;
  float cursor_z = 0.0F;
  int next_slot_id = 0;

  for (const auto& line : lines) {
    if (line.rule == nullptr || line.rule->placement == LinePlacement::SplitFlanks) {
      continue;
    }
    emit_centre_block(*line.rule,
                      line.members,
                      spacing,
                      row_cap,
                      rows_allowed,
                      cursor_z,
                      next_slot_id,
                      slot_list,
                      all_bounds,
                      body_bounds);
  }

  float body_half_width = body_bounds.half_width();

  FlankPreference preference = options.flank_preference;
  if (preference == FlankPreference::Balanced) {
    preference = tmpl.default_flank;
  }

  for (const auto& line : lines) {
    if (line.rule == nullptr || line.rule->placement != LinePlacement::SplitFlanks) {
      continue;
    }
    if (!body_bounds.valid) {
      DoctrineLineRule collapsed = *line.rule;
      collapsed.placement = LinePlacement::CentreBlock;
      collapsed.consumes_depth = true;
      collapsed.front_offset_scale = 0.0F;
      emit_centre_block(collapsed,
                        line.members,
                        spacing,
                        row_cap,
                        rows_allowed,
                        cursor_z,
                        next_slot_id,
                        slot_list,
                        all_bounds,
                        body_bounds);
      body_half_width = body_bounds.half_width();
      continue;
    }
    emit_split_flanks(*line.rule,
                      line.members,
                      spacing,
                      row_cap,
                      rows_allowed,
                      lateral_axis,
                      body_bounds.max_z,
                      body_half_width,
                      preference,
                      next_slot_id,
                      slot_list,
                      all_bounds);
  }

  int const reserve_rows =
      options.reserve_rows >= 0 ? options.reserve_rows : tmpl.reserve_rows;
  apply_reserve_rows(slot_list, reserve_rows, depth_unit);

  RangedPlacement ranged = options.ranged_placement;
  if (ranged == RangedPlacement::Automatic) {
    ranged = tmpl.default_ranged;
  }
  apply_ranged_placement(slot_list, ranged, depth_unit);

  scale_to_frontage(slot_list,
                    requested_frontage,
                    tmpl.frontage_scale * options.frontage_scale,
                    tmpl.depth_scale * options.depth_scale,
                    k_lateral_floor,
                    requested_frontage > 0.01F ? 0.0F : tmpl.max_frontage,
                    tmpl.max_depth);

  {
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

  recentre(slot_list);

  if (slot_spacing_out != nullptr) {
    *slot_spacing_out = slot_spacing;
  }
  return slot_list;
}

auto ArmyFormationPlanner::make_member(EntityID entity_id,
                                       Game::Units::TroopType troop_type,
                                       const QVector3D& position,
                                       const FormationDoctrineId& doctrine)
    -> ArmyFormationMember {
  ArmyFormationMember member;
  member.entity_id = entity_id;
  member.troop_type = troop_type;
  member.roles = TroopRoleRegistry::instance().roles(troop_type);
  member.current_position = position;
  member.footprint =
      Game::Units::TroopConfig::instance().get_selection_ring_size(troop_type);

  member.half_width = member.footprint;
  member.half_depth = member.footprint;
  member.doctrine = doctrine.empty() ? k_neutral_doctrine : doctrine;
  return member;
}

void ArmyFormationPlanner::measure_footprint(const Engine::Core::Entity& entity,
                                             float fallback_radius,
                                             ArmyFormationMember& member) {
  member.half_width = fallback_radius;
  member.half_depth = fallback_radius;
  member.individuals = 1;
  member.files = 1;
  member.soldier_body_radius = fallback_radius;
  member.extents_by_files.clear();

  auto const layout = Game::Systems::FormationCombat::resolve_layout(entity);
  if (layout.all_slots.empty()) {
    return;
  }
  member.individuals = std::max(1, static_cast<int>(layout.all_slots.size()));
  member.soldier_body_radius = std::max(0.05F, layout.body_radius);

  auto const natural =
      Game::Systems::FormationCombat::layout_reach_for_files(entity, 0);
  member.files = std::max(1, natural.files);
  member.half_width = natural.half_x + natural.body_radius;
  member.half_depth = natural.half_z + natural.body_radius;
  member.soldier_file_step =
      member.files > 1 ? (natural.half_x * 2.0F) / static_cast<float>(member.files - 1)
                       : std::max(0.1F, layout.spacing);
  int const rows = std::max(1, (member.individuals + member.files - 1) / member.files);
  member.soldier_rank_step =
      rows > 1 ? (natural.half_z * 2.0F) / static_cast<float>(rows - 1)
               : std::max(0.1F, layout.spacing);

  constexpr int k_max_tabulated_files = 48;
  int const tabulated = std::min(member.individuals, k_max_tabulated_files);
  member.extents_by_files.reserve(static_cast<std::size_t>(tabulated));
  for (int files = 1; files <= tabulated; ++files) {
    auto const reach =
        Game::Systems::FormationCombat::layout_reach_for_files(entity, files);
    member.extents_by_files.emplace_back(reach.half_x + reach.body_radius,
                                         reach.half_z + reach.body_radius);
  }
}

void ArmyFormationPlanner::shape_member_for_intent(ArmyFormationMember& member,
                                                   float aspect) {
  if (aspect <= 0.0F || member.individuals <= 1) {
    return;
  }
  int const files = std::clamp(static_cast<int>(std::lround(std::sqrt(
                                   static_cast<float>(member.individuals) * aspect))),
                               1,
                               member.individuals);
  member.files = files;
  if (static_cast<std::size_t>(files) <= member.extents_by_files.size()) {
    member.half_width =
        member.extents_by_files[static_cast<std::size_t>(files) - 1U].first;
    member.half_depth =
        member.extents_by_files[static_cast<std::size_t>(files) - 1U].second;
    return;
  }
  int const rows = (member.individuals + files - 1) / files;
  member.half_width = static_cast<float>(files - 1) * member.soldier_file_step * 0.5F +
                      member.soldier_body_radius;
  member.half_depth = static_cast<float>(rows - 1) * member.soldier_rank_step * 0.5F +
                      member.soldier_body_radius;
}

namespace {

constexpr float k_tolerated_displaced_share = 0.12F;

auto hungarian_assignment(const std::vector<std::vector<float>>& cost)
    -> std::vector<int> {
  auto const n = static_cast<int>(cost.size());
  constexpr float k_inf = std::numeric_limits<float>::max() / 4.0F;
  std::vector<float> u(static_cast<std::size_t>(n) + 1U, 0.0F);
  std::vector<float> v(static_cast<std::size_t>(n) + 1U, 0.0F);
  std::vector<int> p(static_cast<std::size_t>(n) + 1U, 0);
  std::vector<int> way(static_cast<std::size_t>(n) + 1U, 0);
  for (int i = 1; i <= n; ++i) {
    p[0] = i;
    int j0 = 0;
    std::vector<float> minv(static_cast<std::size_t>(n) + 1U, k_inf);
    std::vector<bool> used(static_cast<std::size_t>(n) + 1U, false);
    do {
      used[static_cast<std::size_t>(j0)] = true;
      int const i0 = p[static_cast<std::size_t>(j0)];
      float delta = k_inf;
      int j1 = 0;
      for (int j = 1; j <= n; ++j) {
        if (used[static_cast<std::size_t>(j)]) {
          continue;
        }
        float const cur =
            cost[static_cast<std::size_t>(i0 - 1)][static_cast<std::size_t>(j - 1)] -
            u[static_cast<std::size_t>(i0)] - v[static_cast<std::size_t>(j)];
        if (cur < minv[static_cast<std::size_t>(j)]) {
          minv[static_cast<std::size_t>(j)] = cur;
          way[static_cast<std::size_t>(j)] = j0;
        }
        if (minv[static_cast<std::size_t>(j)] < delta) {
          delta = minv[static_cast<std::size_t>(j)];
          j1 = j;
        }
      }
      for (int j = 0; j <= n; ++j) {
        if (used[static_cast<std::size_t>(j)]) {
          u[static_cast<std::size_t>(p[static_cast<std::size_t>(j)])] += delta;
          v[static_cast<std::size_t>(j)] -= delta;
        } else {
          minv[static_cast<std::size_t>(j)] -= delta;
        }
      }
      j0 = j1;
    } while (p[static_cast<std::size_t>(j0)] != 0);
    do {
      int const j1 = way[static_cast<std::size_t>(j0)];
      p[static_cast<std::size_t>(j0)] = p[static_cast<std::size_t>(j1)];
      j0 = j1;
    } while (j0 != 0);
  }
  std::vector<int> row_to_column(static_cast<std::size_t>(n), -1);
  for (int j = 1; j <= n; ++j) {
    if (p[static_cast<std::size_t>(j)] > 0) {
      row_to_column[static_cast<std::size_t>(p[static_cast<std::size_t>(j)] - 1)] =
          j - 1;
    }
  }
  return row_to_column;
}

void assign_nearest_troops(ArmyFormationPlan& plan,
                           const ArmyFormationLayout& layout,
                           const QVector3D& anchor,
                           float facing) {
  auto const count = plan.slot_list.size();
  std::vector<bool> done(count, false);
  std::vector<EntityID> occupants(count, 0U);
  std::vector<int> files(count, 0);
  for (std::size_t first = 0; first < count; ++first) {
    if (done[first]) {
      continue;
    }
    std::vector<std::size_t> bucket;
    for (std::size_t i = first; i < count; ++i) {
      if (!done[i] && layout.slot_kind[i] == layout.slot_kind[first]) {
        bucket.push_back(i);
        done[i] = true;
      }
    }
    std::vector<std::vector<float>> cost(bucket.size(),
                                         std::vector<float>(bucket.size(), 0.0F));
    for (std::size_t r = 0; r < bucket.size(); ++r) {
      QVector3D const start = layout.slot_start[bucket[r]];
      for (std::size_t c = 0; c < bucket.size(); ++c) {
        QVector3D const rotated =
            rotate_offset(plan.slot_list[bucket[c]].local_offset, facing);
        QVector3D const target(
            anchor.x() + rotated.x(), 0.0F, anchor.z() + rotated.z());
        cost[r][c] =
            QVector3D(target.x() - start.x(), 0.0F, target.z() - start.z()).length();
      }
    }
    auto const chosen = hungarian_assignment(cost);
    for (std::size_t r = 0; r < bucket.size(); ++r) {
      auto const column = chosen[r] >= 0 ? static_cast<std::size_t>(chosen[r]) : r;
      occupants[bucket[column]] = plan.slot_list[bucket[r]].occupant;
      files[bucket[column]] = plan.slot_files[bucket[r]];
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    plan.slot_list[i].occupant = occupants[i];
    plan.slot_files[i] = files[i];
  }
}

auto displacement_score(const ArmyFormationPlan& plan,
                        const QVector3D& requested_anchor) -> float {
  float const pitch = std::max(1.0F, plan.slot_spacing);
  QVector3D const moved(plan.anchor.x() - requested_anchor.x(),
                        0.0F,
                        plan.anchor.z() - requested_anchor.z());
  return static_cast<float>(plan.blocked_count * 4 + plan.adjusted_count) +
         plan.displacement / pitch + (plan.narrowed ? 1.0F : 0.0F) +
         moved.length() / pitch * 0.5F;
}

} // namespace

auto ArmyFormationPlan::keeps_shape() const -> bool {
  if (!valid) {
    return false;
  }
  auto const tolerated = static_cast<int>(
      std::floor(static_cast<float>(slot_list.size()) * k_tolerated_displaced_share));
  return blocked_count <= tolerated &&
         blocked_count + adjusted_count <= std::max(1, tolerated);
}

auto ArmyFormationPlanner::fit_to_ground(
    const ArmyFormationLayout& first_layout,
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request,
    const ArmyFormation* previous_group) -> ArmyFormationPlan {
  constexpr int k_attempts = 4;
  constexpr float k_narrowing = 0.62F;

  ArmyFormationPlan best = place(first_layout, request);
  if (!best.valid || best.keeps_shape() || !request.resolve_terrain) {
    return best;
  }
  QVector3D const requested_anchor = best.anchor;

  if (request.allow_anchor_shift) {
    float const yaw = request.facing * k_deg_to_rad;
    QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
    QVector3D const lateral(std::cos(yaw), 0.0F, -std::sin(yaw));
    float const depth = std::max(best.depth, best.slot_spacing);
    float const frontage = std::max(best.frontage, best.slot_spacing);
    std::array<QVector3D, 6> const shifts{forward * (-0.25F * depth),
                                          forward * (-0.5F * depth),
                                          lateral * (-0.25F * frontage),
                                          lateral * (0.25F * frontage),
                                          forward * (0.25F * depth),
                                          forward * (0.5F * depth)};
    for (const auto& shift : shifts) {
      ArmyFormationRequest shifted = request;
      shifted.anchor = request.anchor + shift;
      auto plan = place(first_layout, shifted);
      if (!plan.valid) {
        continue;
      }
      if (displacement_score(plan, requested_anchor) <
          displacement_score(best, requested_anchor)) {
        best = std::move(plan);
      }
      if (best.keeps_shape()) {
        return best;
      }
    }
  }

  ArmyFormationRequest attempt = request;
  float current = request.frontage > 0.01F ? request.frontage : best.frontage;
  for (int index = 1; index < k_attempts; ++index) {
    attempt.frontage = std::max(1.0F, current * k_narrowing);
    if (attempt.frontage >= current - 0.01F) {
      break;
    }
    current = attempt.frontage;
    auto plan = place(build_layout(members, attempt, previous_group), attempt);
    if (!plan.valid) {
      break;
    }
    plan.narrowed = true;
    if (displacement_score(plan, requested_anchor) <
        displacement_score(best, requested_anchor)) {
      best = std::move(plan);
    }
    if (best.keeps_shape()) {
      break;
    }
  }
  return best;
}

auto ArmyFormationPlanner::plan(const std::vector<ArmyFormationMember>& members,
                                const ArmyFormationRequest& request,
                                const ArmyFormation* previous_group)
    -> ArmyFormationPlan {
  return fit_to_ground(
      build_layout(members, request, previous_group), members, request, previous_group);
}

auto ArmyFormationPlanner::plan(Engine::Core::World& world,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  const ArmyFormation* previous =
      request.group_id != k_invalid_group
          ? ArmyFormationRegistry::for_world(world).find(request.group_id)
          : nullptr;
  const auto members = collect_members(world, request.members);
  return plan(members, request, previous);
}

auto ArmyFormationPlanner::resolve_movement_policy(
    MovementPolicy requested, const DoctrineIntentTemplate& tmpl) -> MovementPolicy {
  if (requested != MovementPolicy::DoctrineDefault) {
    return requested;
  }
  return tmpl.default_movement == MovementPolicy::DoctrineDefault
             ? MovementPolicy::ReformAtDestination
             : tmpl.default_movement;
}

auto ArmyFormationPlanner::layout_from_reference(const ArmyFormation& formation)
    -> ArmyFormationLayout {
  ArmyFormationLayout layout;
  layout.doctrine = formation.doctrine;
  layout.intent = formation.intent;
  layout.spacing = formation.spacing;
  layout.slot_spacing = formation.slot_spacing;
  layout.footprint_gap = lane_for(formation.spacing) * 0.5F;
  layout.movement_policy = formation.options.movement_policy;
  layout.slot_list = formation.reference_slots;
  Bounds bounds;
  for (const auto& slot : layout.slot_list) {
    bounds.expand(slot.local_offset);
    layout.slot_clearance.push_back(std::min(slot.half_width, slot.half_depth));
    layout.slot_half_width.push_back(slot.half_width);
    layout.slot_half_depth.push_back(slot.half_depth);
    layout.slot_files.push_back(0);
  }
  layout.frontage = bounds.width();
  layout.depth = bounds.depth();
  layout.valid = !layout.slot_list.empty();
  if (!layout.valid) {
    layout.rejection_reason = "The formation has no reference shape.";
  }
  return layout;
}

void ArmyFormationPlanner::fold_onto_reference(
    ArmyFormationPlan& plan, const std::vector<FormationSlot>& reference) {
  std::unordered_map<EntityID, const FormationSlot*> reference_of;
  for (const auto& slot : reference) {
    if (slot.occupant != 0U) {
      reference_of.emplace(slot.occupant, &slot);
    }
  }
  for (const auto& slot : plan.slot_list) {
    if (reference_of.count(slot.occupant) == 0U) {
      return;
    }
  }

  auto funnel_before = [](const QVector3D& a, const QVector3D& b) {
    if (std::abs(a.z() - b.z()) > 0.01F) {
      return a.z() > b.z();
    }
    if (std::abs(std::abs(a.x()) - std::abs(b.x())) > 0.01F) {
      return std::abs(a.x()) < std::abs(b.x());
    }
    return a.x() < b.x();
  };

  auto size_key = [](const FormationSlot& slot) {
    return (static_cast<std::uint64_t>(slot.role) << 1U) |
           static_cast<std::uint64_t>(slot.heavy);
  };

  auto const count = plan.slot_list.size();
  std::vector<bool> done(count, false);
  std::vector<EntityID> occupants(count, 0U);
  for (std::size_t first = 0; first < count; ++first) {
    if (done[first]) {
      continue;
    }
    auto const key = size_key(plan.slot_list[first]);
    std::vector<std::size_t> positions;
    std::vector<EntityID> troops;
    for (std::size_t i = first; i < count; ++i) {
      if (!done[i] && size_key(plan.slot_list[i]) == key) {
        done[i] = true;
        positions.push_back(i);
        troops.push_back(plan.slot_list[i].occupant);
      }
    }
    std::stable_sort(
        positions.begin(), positions.end(), [&](std::size_t a, std::size_t b) {
          return funnel_before(plan.slot_list[a].local_offset,
                               plan.slot_list[b].local_offset);
        });
    std::stable_sort(troops.begin(), troops.end(), [&](EntityID a, EntityID b) {
      return funnel_before(reference_of[a]->local_offset,
                           reference_of[b]->local_offset);
    });
    for (std::size_t k = 0; k < positions.size(); ++k) {
      occupants[positions[k]] = troops[k];
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    plan.slot_list[i].occupant = occupants[i];
    plan.slot_list[i].id = reference_of[occupants[i]]->id;
  }
}

auto ArmyFormationPlanner::min_cost_assignment(
    const std::vector<std::vector<float>>& cost) -> std::vector<int> {
  return hungarian_assignment(cost);
}

auto ArmyFormationPlanner::footprints_overlap(const FormationSlot& a,
                                              const FormationSlot& b,
                                              float gap) -> bool {
  auto const axes = frame_axes(a.facing);
  QVector3D const offset = b.world_position - a.world_position;
  return local_overlap(QVector3D::dotProduct(offset, axes.lateral),
                       QVector3D::dotProduct(offset, axes.depth),
                       a.half_width,
                       a.half_depth,
                       b.half_width,
                       b.half_depth,
                       gap);
}

auto ArmyFormationPlanner::first_overlap(const std::vector<FormationSlot>& slot_list,
                                         float gap) -> std::pair<int, int> {
  for (std::size_t i = 0; i < slot_list.size(); ++i) {
    if (slot_list[i].status == SlotStatus::Blocked) {
      continue;
    }
    for (std::size_t j = i + 1; j < slot_list.size(); ++j) {
      if (slot_list[j].status == SlotStatus::Blocked) {
        continue;
      }
      if (footprints_overlap(slot_list[i], slot_list[j], gap)) {
        return {static_cast<int>(i), static_cast<int>(j)};
      }
    }
  }
  return {-1, -1};
}

auto ArmyFormationPlanner::layout_signature(
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request,
    const ArmyFormation* previous_group) -> std::uint64_t {
  Hasher hasher;
  for (const auto& member : members) {
    hasher.mix(member.entity_id);
    hasher.mix(static_cast<std::uint64_t>(member.troop_type));
    hasher.mix(static_cast<std::uint64_t>(member.roles));
    hasher.mix(member.doctrine);
    hasher.mix_float(member.footprint);

    hasher.mix_float(member.current_position.x());
    hasher.mix_float(member.current_position.z());
  }

  hasher.mix_float(request.facing);
  hasher.mix(static_cast<std::uint64_t>(request.intent));
  for (const auto& member : members) {
    hasher.mix(static_cast<std::uint64_t>(member.individuals));
  }
  hasher.mix(request.doctrine);
  hasher.mix_float(request.spacing);
  hasher.mix_float(request.frontage);
  hasher.mix(request.group_id);
  hasher.mix(static_cast<std::uint64_t>(request.preserve_previous_slots));
  hasher.mix(static_cast<std::uint64_t>(request.assign_nearest));

  const auto& options = request.options;
  hasher.mix(static_cast<std::uint64_t>(options.flank_preference));
  hasher.mix(static_cast<std::uint64_t>(options.ranged_placement));
  hasher.mix(static_cast<std::uint64_t>(options.mixed_policy));
  hasher.mix(static_cast<std::uint64_t>(options.movement_policy));
  hasher.mix_float(options.frontage_scale);
  hasher.mix_float(options.depth_scale);
  hasher.mix_float(options.spacing_scale);
  hasher.mix(static_cast<std::uint64_t>(options.reserve_rows));
  hasher.mix(static_cast<std::uint64_t>(options.preserve_member_order));
  hasher.mix(static_cast<std::uint64_t>(options.doctrine_locked));

  if (request.preserve_previous_slots && request.group_id != k_invalid_group) {
    const auto* previous = previous_group;
    if (previous != nullptr) {
      for (const auto& slot : previous->slot_list) {
        hasher.mix(slot.occupant);
        hasher.mix(static_cast<std::uint64_t>(slot.id));
      }
    }
  }

  return hasher.value();
}

namespace {

constexpr float k_lateral_gap_metres = 1.6F;
constexpr float k_rank_gap_metres = 2.0F;

auto is_wing(ArmyRole role) -> bool {
  return role == ArmyRole::LeftFlank || role == ArmyRole::RightFlank;
}

auto split_rows(int total, const std::vector<float>& weights) -> std::vector<int> {
  std::vector<int> rows;
  float weight_sum = 0.0F;
  for (float const w : weights) {
    weight_sum += w;
  }
  int assigned = 0;
  for (std::size_t r = 0; r < weights.size(); ++r) {
    int size = r + 1U == weights.size()
                   ? total - assigned
                   : static_cast<int>(std::lround(static_cast<float>(total) *
                                                  weights[r] / weight_sum));
    size = std::clamp(size, 0, total - assigned);
    if (size > 0) {
      rows.push_back(size);
    }
    assigned += size;
  }
  return rows;
}

auto even_rows(int total, int row_count) -> std::vector<int> {
  row_count = std::clamp(row_count, 1, std::max(1, total));
  std::vector<int> rows;
  int const base = total / row_count;
  int const extra = total % row_count;
  for (int r = 0; r < row_count; ++r) {
    int const size = base + (r < extra ? 1 : 0);
    if (size > 0) {
      rows.push_back(size);
    }
  }
  return rows;
}

auto silhouette_rows(ArmyFormationIntent intent,
                     int total,
                     const ArmyFormationOptions& options,
                     bool has_rear_tier = false) -> std::vector<int> {
  float const depth_bias = std::clamp(options.depth_scale, 0.4F, 3.0F) /
                           std::clamp(options.frontage_scale, 0.4F, 3.0F);
  auto scaled = [&](int rows) {
    return std::max(
        1, static_cast<int>(std::lround(static_cast<float>(rows) * depth_bias)));
  };
  switch (intent) {
  case ArmyFormationIntent::Line:
  case ArmyFormationIntent::Encirclement:
    return even_rows(total, total <= 3 ? 1 : scaled(2));
  case ArmyFormationIntent::Column: {
    int const files = std::clamp(static_cast<int>(std::lround((total <= 2   ? 1.0F
                                                               : total <= 6 ? 2.0F
                                                                            : 3.0F) /
                                                              depth_bias)),
                                 1,
                                 total);
    return even_rows(total, (total + files - 1) / files);
  }
  case ArmyFormationIntent::Assault:
    if (total <= 3) {
      return even_rows(total, total);
    }
    return split_rows(total, {5.0F, 3.0F, 1.0F});
  case ArmyFormationIntent::SiegeEscort:
    return total <= 4 ? even_rows(total, 2) : split_rows(total, {7.0F, 5.0F});
  case ArmyFormationIntent::FactionDefault:
  case ArmyFormationIntent::Defensive:
    if (total <= 4 || has_rear_tier) {
      return even_rows(total, scaled(total <= 2 ? 1 : 2));
    }
    return split_rows(total, {5.0F, 5.0F, 3.0F});
  }
  return even_rows(total, 2);
}

auto is_core_role(ArmyRole role) -> bool {
  return role == ArmyRole::Centre || role == ArmyRole::Screen ||
         role == ArmyRole::Vanguard;
}

void regularize_silhouette(
    std::vector<FormationSlot>& slot_list,
    const std::unordered_map<EntityID, const ArmyFormationMember*>& by_id,
    const ArmyFormationRequest& request,
    const DoctrineIntentTemplate& tmpl,
    float spacing) {
  if (slot_list.size() < 2U) {
    return;
  }
  float const gap_scale = std::clamp(request.options.spacing_scale, 0.5F, 2.5F);
  float const lateral_gap = std::max(k_lateral_gap_metres, spacing) * gap_scale;
  float const rank_gap = std::max(k_rank_gap_metres, spacing) * gap_scale;
  auto half_width = [&](std::size_t index) {
    auto const it = by_id.find(slot_list[index].occupant);
    return it == by_id.end() ? 0.5F : it->second->half_width;
  };
  auto half_depth = [&](std::size_t index) {
    auto const it = by_id.find(slot_list[index].occupant);
    return it == by_id.end() ? 0.5F : it->second->half_depth;
  };

  auto const intent = request.intent;
  bool const wings_apart = intent != ArmyFormationIntent::Column;
  std::vector<std::size_t> left_wing;
  std::vector<std::size_t> right_wing;
  struct Tier {
    ArmyRole role{ArmyRole::Centre};
    bool core{false};
    float mean_z{0.0F};
    std::vector<std::size_t> members;
  };
  std::vector<Tier> tiers;
  for (std::size_t i = 0; i < slot_list.size(); ++i) {
    auto const role = slot_list[i].role;
    if (wings_apart && role == ArmyRole::LeftFlank) {
      left_wing.push_back(i);
      continue;
    }
    if (wings_apart && role == ArmyRole::RightFlank) {
      right_wing.push_back(i);
      continue;
    }
    bool const core = is_core_role(role) || is_wing(role);
    auto tier = std::find_if(tiers.begin(), tiers.end(), [&](const Tier& t) {
      return core ? t.core : (!t.core && t.role == role);
    });
    if (tier == tiers.end()) {
      tiers.push_back({role, core, 0.0F, {}});
      tier = std::prev(tiers.end());
    }
    tier->members.push_back(i);
  }
  for (auto& tier : tiers) {
    float sum = 0.0F;
    for (auto const index : tier.members) {
      sum += slot_list[index].local_offset.z();
    }
    tier.mean_z =
        sum / static_cast<float>(std::max<std::size_t>(1U, tier.members.size()));
    std::stable_sort(
        tier.members.begin(), tier.members.end(), [&](std::size_t a, std::size_t b) {
          float const za = slot_list[a].local_offset.z();
          float const zb = slot_list[b].local_offset.z();
          if (std::abs(za - zb) > 0.25F) {
            return za > zb;
          }
          return slot_list[a].local_offset.x() < slot_list[b].local_offset.x();
        });
  }
  std::stable_sort(tiers.begin(), tiers.end(), [](const Tier& a, const Tier& b) {
    return a.mean_z > b.mean_z;
  });
  if (tiers.empty()) {
    return;
  }

  auto row_width = [&](const std::vector<std::size_t>& row) {
    float width = 0.0F;
    for (auto const index : row) {
      width += 2.0F * half_width(index);
    }
    return width + lateral_gap * static_cast<float>(row.empty() ? 0U : row.size() - 1U);
  };
  float average_width = 0.0F;
  std::size_t counted = 0;
  for (const auto& tier : tiers) {
    for (auto const index : tier.members) {
      average_width += 2.0F * half_width(index) + lateral_gap;
      ++counted;
    }
  }
  average_width /= static_cast<float>(std::max<std::size_t>(1U, counted));

  int core_count = 0;
  for (const auto& tier : tiers) {
    if (tier.core) {
      core_count += static_cast<int>(tier.members.size());
    }
  }
  std::vector<int> core_rows;
  if (request.frontage > 0.01F) {
    int const per_row =
        std::max(1, static_cast<int>(std::floor(request.frontage / average_width)) + 1);
    core_rows = even_rows(core_count, (core_count + per_row - 1) / per_row);
  } else {
    bool const has_rear_tier =
        std::any_of(tiers.begin(), tiers.end(), [](const Tier& t) { return !t.core; });
    core_rows = silhouette_rows(intent, core_count, request.options, has_rear_tier);
  }
  int row_capacity = 1;
  for (int const size : core_rows) {
    row_capacity = std::max(row_capacity, size);
  }
  if (core_count == 0) {
    row_capacity = std::max(
        1,
        silhouette_rows(intent, static_cast<int>(slot_list.size()), request.options)
            .front());
  }
  if (tmpl.max_frontage > 0.1F && request.frontage <= 0.01F) {
    auto wing_width = [&](const std::vector<std::size_t>& wing) {
      float width = 0.0F;
      for (std::size_t k = 0; k < wing.size(); k += 2U) {
        float column = half_width(wing[k]);
        if (k + 1U < wing.size()) {
          column = std::max(column, half_width(wing[k + 1U]));
        }
        width += 2.0F * column + lateral_gap;
      }
      return width;
    };
    float const core_room =
        std::max(average_width,
                 tmpl.max_frontage - wing_width(left_wing) - wing_width(right_wing));
    int const fits = std::max(
        1, static_cast<int>(std::floor((core_room + lateral_gap) / average_width)));
    if (row_capacity > fits) {
      row_capacity = fits;
      core_rows = even_rows(core_count, (core_count + fits - 1) / fits);
    }
  }

  RangedPlacement ranged_placement = request.options.ranged_placement;
  if (ranged_placement == RangedPlacement::Automatic) {
    ranged_placement = tmpl.default_ranged;
  }
  auto build_rows = [&](int capacity, const std::vector<int>& core_sizes) {
    std::vector<std::vector<std::size_t>> rows;
    bool core_placed = false;
    for (const auto& tier : tiers) {
      auto const count = static_cast<int>(tier.members.size());
      std::vector<int> const sizes =
          tier.core ? core_sizes : even_rows(count, (count + capacity - 1) / capacity);
      if (!tier.core && intent == ArmyFormationIntent::Defensive &&
          tier.role == ArmyRole::Reserve && !rows.empty()) {
        rows.emplace_back();
      }
      std::size_t cursor = 0;
      for (int const size : sizes) {
        std::vector<std::size_t> row(
            tier.members.begin() + static_cast<std::ptrdiff_t>(cursor),
            tier.members.begin() + static_cast<std::ptrdiff_t>(cursor) + size);
        cursor += static_cast<std::size_t>(size);
        std::stable_sort(row.begin(), row.end(), [&](std::size_t a, std::size_t b) {
          return slot_list[a].local_offset.x() < slot_list[b].local_offset.x();
        });
        rows.push_back(std::move(row));
      }
      if (!tier.core && !core_placed && tier.role == ArmyRole::Ranged &&
          ranged_placement == RangedPlacement::Skirmish) {
        rows.emplace_back();
      }
      core_placed = core_placed || tier.core;
    }
    return rows;
  };
  auto rows_depth = [&](const std::vector<std::vector<std::size_t>>& rows) {
    float depth = 0.0F;
    float previous = 0.0F;
    bool first_row = true;
    for (const auto& row : rows) {
      if (row.empty()) {
        depth += rank_gap + 2.0F * previous;
        continue;
      }
      float row_half_depth = 0.0F;
      for (auto const index : row) {
        row_half_depth = std::max(row_half_depth, half_depth(index));
      }
      if (!first_row) {
        depth += previous + rank_gap + row_half_depth;
      }
      first_row = false;
      previous = row_half_depth;
    }
    return depth;
  };
  auto placed_rows = build_rows(row_capacity, core_rows);
  int const total = static_cast<int>(slot_list.size());
  while (tmpl.max_depth > 0.1F && rows_depth(placed_rows) > tmpl.max_depth &&
         row_capacity < total) {
    ++row_capacity;
    core_rows = even_rows(core_count, (core_count + row_capacity - 1) / row_capacity);
    placed_rows = build_rows(row_capacity, core_rows);
  }

  float z = 0.0F;
  float previous_half_depth = 0.0F;
  float front_z = 0.0F;
  float front_half_depth = 0.0F;
  float front_half_width = 0.0F;
  bool first = true;
  int rank = 0;
  for (auto const& row : placed_rows) {
    if (row.empty()) {
      z -= rank_gap + 2.0F * previous_half_depth;
      continue;
    }
    float row_half_depth = 0.0F;
    for (auto const index : row) {
      row_half_depth = std::max(row_half_depth, half_depth(index));
    }
    float gap = lateral_gap;
    if (request.frontage > 0.01F && row.size() > 1U) {
      float const outer = half_width(row.front()) + half_width(row.back());
      float const bodies =
          row_width(row) - lateral_gap * static_cast<float>(row.size() - 1U) - outer;
      gap = std::max(lateral_gap,
                     (request.frontage - bodies) / static_cast<float>(row.size() - 1U));
    }
    float width = 0.0F;
    for (auto const index : row) {
      width += 2.0F * half_width(index);
    }
    width += gap * static_cast<float>(row.size() - 1U);
    if (!first) {
      z -= previous_half_depth + rank_gap + row_half_depth;
    }
    float x = -width * 0.5F;
    int file = 0;
    for (auto const index : row) {
      float const hw = half_width(index);
      slot_list[index].local_offset = QVector3D(x + hw, 0.0F, z);
      slot_list[index].rank = rank;
      slot_list[index].file = file++;
      x += 2.0F * hw + gap;
    }
    if (first) {
      front_z = z;
      front_half_depth = row_half_depth;
      front_half_width = width * 0.5F;
      first = false;
    }
    previous_half_depth = row_half_depth;
    ++rank;
  }

  auto place_wing = [&](std::vector<std::size_t>& wing, float side) {
    std::stable_sort(wing.begin(), wing.end(), [&](std::size_t a, std::size_t b) {
      return side * slot_list[a].local_offset.x() <
             side * slot_list[b].local_offset.x();
    });
    float const reach = intent == ArmyFormationIntent::Encirclement
                            ? front_half_depth * 2.0F + rank_gap
                            : 0.0F;
    float x = front_half_width + lateral_gap;
    int column = 0;
    float column_width = 0.0F;
    float wing_z = front_z + reach;
    for (std::size_t k = 0; k < wing.size(); ++k) {
      auto& slot = slot_list[wing[k]];
      float const hw = half_width(wing[k]);
      float const hd = half_depth(wing[k]);
      column_width = std::max(column_width, hw);
      slot.local_offset = QVector3D(side * (x + hw), 0.0F, wing_z);
      slot.rank = column;
      wing_z -= 2.0F * hd + rank_gap;
      if ((k + 1U) % 2U == 0U) {
        x += 2.0F * column_width + lateral_gap;
        column_width = 0.0F;
        wing_z = front_z + reach;
        ++column;
      }
    }
  };
  place_wing(left_wing, -1.0F);
  place_wing(right_wing, 1.0F);
  recentre_on_centroid(slot_list);
}

} // namespace

auto ArmyFormationPlanner::build_layout(const std::vector<ArmyFormationMember>& members,
                                        const ArmyFormationRequest& request,
                                        const ArmyFormation* previous_group)
    -> ArmyFormationLayout {
  ArmyFormationLayout layout;
  layout.intent = request.intent;
  layout.signature = layout_signature(members, request, previous_group);

  if (members.empty()) {
    layout.rejection_reason =
        QCoreApplication::translate("Formation",
                                    "No units eligible for formation placement.")
            .toStdString();
    return layout;
  }

  layout.doctrine = resolve_doctrine(members, request);
  const auto& doctrine = DoctrineRegistry::instance().get_or_neutral(layout.doctrine);

  auto const roles = combined_roles(members);
  auto const reason = DoctrineRegistry::instance().availability_reason(
      layout.doctrine, request.intent, roles, static_cast<int>(members.size()));
  if (!reason.empty()) {
    layout.rejection_reason = reason;
    return layout;
  }

  const auto* tmpl = doctrine.resolve_template(request.intent);
  if (tmpl == nullptr) {
    layout.rejection_reason =
        QCoreApplication::translate(
            "Formation", "This doctrine has no template for the chosen formation.")
            .toStdString();
    return layout;
  }

  float const spacing =
      std::max(0.2F,
               request.spacing * tmpl->spacing_scale *
                   std::clamp(request.options.spacing_scale, 0.4F, 2.5F));
  layout.spacing = spacing;
  layout.footprint_gap = lane_for(spacing) * 0.5F;
  layout.movement_policy =
      resolve_movement_policy(request.options.movement_policy, *tmpl);

  std::vector<ArmyFormationMember> shaped = members;
  for (auto& member : shaped) {
    shape_member_for_intent(member, tmpl->unit_files_aspect);
  }

  layout.slot_spacing = spacing;

  {
    constexpr int k_bound_attempts = 6;
    int row_cap_override = 0;
    std::vector<FormationSlot> best;
    float best_excess = std::numeric_limits<float>::max();
    float best_spacing = spacing;
    for (int attempt = 0; attempt < k_bound_attempts; ++attempt) {
      float attempt_spacing = spacing;
      int cap_used = 0;
      auto candidate = plan_local_slots(shaped,
                                        *tmpl,
                                        request.options,
                                        spacing,
                                        request.frontage,
                                        request.facing,
                                        &attempt_spacing,
                                        row_cap_override,
                                        &cap_used);
      Bounds measured;
      for (const auto& slot : candidate) {
        measured.expand(slot.local_offset);
      }
      float const over_width = tmpl->max_frontage > 0.1F && request.frontage <= 0.01F
                                   ? measured.width() - tmpl->max_frontage
                                   : 0.0F;
      float const over_depth =
          tmpl->max_depth > 0.1F ? measured.depth() - tmpl->max_depth : 0.0F;
      float const excess = std::max(0.0F, over_width) + std::max(0.0F, over_depth);
      if (excess < best_excess) {
        best_excess = excess;
        best = candidate;
        best_spacing = attempt_spacing;
      }
      if (excess <= 0.0F) {
        break;
      }

      int const current = std::max(1, cap_used);
      int const next = over_width > over_depth
                           ? std::max(1, current - std::max(1, current / 5))
                           : current + std::max(1, current / 5);
      if (next == current) {
        break;
      }
      row_cap_override = next;
    }
    layout.slot_list = std::move(best);
    layout.slot_spacing = best_spacing;
  }
  if (layout.slot_list.empty()) {
    layout.rejection_reason = "The formation template produced no slot_list.";
    return layout;
  }

  std::unordered_map<EntityID, const ArmyFormationMember*> by_id;
  by_id.reserve(shaped.size());
  for (const auto& member : shaped) {
    by_id.emplace(member.entity_id, &member);
  }

  regularize_silhouette(layout.slot_list, by_id, request, *tmpl, spacing);

  if (request.preserve_previous_slots && request.group_id != k_invalid_group &&
      previous_group != nullptr) {

    auto footprint_key = [&by_id](EntityID id) -> std::uint64_t {
      auto const found = by_id.find(id);
      if (found == by_id.end()) {
        return 0U;
      }
      Hasher hasher;
      hasher.mix(static_cast<std::uint64_t>(found->second->troop_type));
      hasher.mix(static_cast<std::uint64_t>(found->second->individuals));
      hasher.mix(static_cast<std::uint64_t>(found->second->heavy));
      return hasher.value();
    };

    std::unordered_map<EntityID, std::size_t> desired;
    for (const auto& old_slot : previous_group->slot_list) {
      if (old_slot.occupant == 0U || old_slot.id < 0) {
        continue;
      }
      auto const index = static_cast<std::size_t>(old_slot.id);
      if (index < layout.slot_list.size()) {
        desired.emplace(old_slot.occupant, index);
      }
    }

    auto const slot_count = layout.slot_list.size();
    std::vector<std::uint64_t> slot_keys(slot_count, 0U);
    for (std::size_t i = 0; i < slot_count; ++i) {
      slot_keys[i] = footprint_key(layout.slot_list[i].occupant);
    }
    std::vector<EntityID> occupants(slot_count, 0U);
    std::vector<bool> bucket_done(slot_count, false);
    for (std::size_t first = 0; first < slot_count; ++first) {
      if (bucket_done[first]) {
        continue;
      }
      std::vector<std::size_t> bucket;
      for (std::size_t i = first; i < slot_count; ++i) {
        if (!bucket_done[i] && slot_keys[i] == slot_keys[first]) {
          bucket.push_back(i);
          bucket_done[i] = true;
        }
      }
      std::vector<EntityID> pending;
      for (auto const i : bucket) {
        auto const entity = layout.slot_list[i].occupant;
        auto const wanted = desired.find(entity);
        bool const in_bucket = wanted != desired.end() && wanted->second < slot_count &&
                               slot_keys[wanted->second] == slot_keys[first];
        if (in_bucket && occupants[wanted->second] == 0U) {
          occupants[wanted->second] = entity;
        } else {
          pending.push_back(entity);
        }
      }
      std::size_t next = 0;
      for (auto const i : bucket) {
        if (occupants[i] == 0U && next < pending.size()) {
          occupants[i] = pending[next++];
        }
      }
    }
    for (std::size_t i = 0; i < slot_count; ++i) {
      layout.slot_list[i].occupant = occupants[i];
    }
  }

  layout.slot_clearance.assign(layout.slot_list.size(), layout.slot_spacing * 0.5F);
  layout.slot_half_width.assign(layout.slot_list.size(), layout.slot_spacing * 0.5F);
  layout.slot_half_depth.assign(layout.slot_list.size(), layout.slot_spacing * 0.5F);
  layout.slot_files.assign(layout.slot_list.size(), 0);
  for (std::size_t i = 0; i < layout.slot_list.size(); ++i) {
    auto& slot = layout.slot_list[i];
    auto const found = by_id.find(slot.occupant);
    if (found == by_id.end()) {
      slot.half_width = layout.slot_half_width[i];
      slot.half_depth = layout.slot_half_depth[i];
      continue;
    }
    layout.slot_files[i] = tmpl->unit_files_aspect > 0.0F ? found->second->files : 0;
    layout.slot_clearance[i] =
        std::min(found->second->half_width, found->second->half_depth);
    layout.slot_half_width[i] = found->second->half_width;
    layout.slot_half_depth[i] = found->second->half_depth;
    slot.half_width = found->second->half_width;
    slot.half_depth = found->second->half_depth;
    slot.heavy = found->second->heavy;
  }

  separate_footprints(layout.slot_list,
                      layout.slot_half_width,
                      layout.slot_half_depth,
                      layout.footprint_gap);
  recentre_on_centroid(layout.slot_list);

  layout.assign_by_distance =
      request.assign_nearest &&
      !(request.preserve_previous_slots && request.group_id != k_invalid_group &&
        previous_group != nullptr);
  layout.slot_start.assign(layout.slot_list.size(), QVector3D());
  layout.slot_kind.assign(layout.slot_list.size(), 0U);
  for (std::size_t i = 0; i < layout.slot_list.size(); ++i) {
    auto const found = by_id.find(layout.slot_list[i].occupant);
    if (found == by_id.end()) {
      continue;
    }
    layout.slot_start[i] = found->second->current_position;
    Hasher hasher;
    hasher.mix(static_cast<std::uint64_t>(found->second->troop_type));
    hasher.mix(static_cast<std::uint64_t>(found->second->individuals));
    hasher.mix(static_cast<std::uint64_t>(found->second->heavy));
    layout.slot_kind[i] = hasher.value();
  }

  Bounds bounds;
  for (const auto& slot : layout.slot_list) {
    bounds.expand(slot.local_offset);
  }
  layout.frontage = bounds.width();
  layout.depth = bounds.depth();
  layout.valid = true;
  return layout;
}

auto ArmyFormationPlanner::place(const ArmyFormationLayout& layout,
                                 const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  ArmyFormationPlan plan;
  plan.anchor = request.anchor;
  plan.facing = request.facing;
  plan.intent = layout.intent;
  plan.doctrine = layout.doctrine;
  plan.spacing = layout.spacing;
  plan.slot_spacing = layout.slot_spacing;
  plan.frontage = layout.frontage;
  plan.depth = layout.depth;
  plan.footprint_gap = layout.footprint_gap;
  plan.movement_policy = layout.movement_policy;

  if (!layout.valid) {
    plan.rejection_reason = layout.rejection_reason;
    return plan;
  }

  plan.slot_list = layout.slot_list;
  plan.slot_clearance = layout.slot_clearance;
  plan.slot_half_width = layout.slot_half_width;
  plan.slot_half_depth = layout.slot_half_depth;
  plan.slot_files = layout.slot_files;
  plan.slot_clearance.resize(plan.slot_list.size(), layout.slot_spacing * 0.5F);
  plan.slot_half_width.resize(plan.slot_list.size(), layout.slot_spacing * 0.5F);
  plan.slot_half_depth.resize(plan.slot_list.size(), layout.slot_spacing * 0.5F);
  plan.slot_files.resize(plan.slot_list.size(), 0);

  QVector3D const anchor =
      request.resolve_terrain &&
              !Game::Systems::NavGrid::is_world_position_walkable(request.anchor)
          ? Game::Systems::NavGrid::snap_to_walkable_ground(request.anchor, 15)
          : request.anchor;
  plan.anchor = anchor;
  SlotTerrainFitter fitter(layout.slot_spacing,
                           layout.footprint_gap,
                           request.facing,
                           anchor,
                           request.resolve_terrain,
                           plan.slot_list.size());

  if (layout.assign_by_distance && layout.slot_start.size() == plan.slot_list.size() &&
      layout.slot_kind.size() == plan.slot_list.size()) {
    assign_nearest_troops(plan, layout, anchor, request.facing);
  }

  std::vector<std::size_t> ordered;
  ordered.reserve(plan.slot_list.size());
  for (std::size_t i = 0; i < plan.slot_list.size(); ++i) {
    ordered.push_back(i);
  }

  auto const& placed = plan.slot_list;
  std::stable_sort(
      ordered.begin(), ordered.end(), [&placed](std::size_t ia, std::size_t ib) {
        const auto* a = &placed[ia];
        const auto* b = &placed[ib];
        if (a->local_offset.z() != b->local_offset.z()) {
          return a->local_offset.z() > b->local_offset.z();
        }
        return std::abs(a->local_offset.x()) < std::abs(b->local_offset.x());
      });

  for (auto const index : ordered) {
    auto* slot = &plan.slot_list[index];
    QVector3D const rotated = rotate_offset(slot->local_offset, request.facing);
    QVector3D const ideal(
        anchor.x() + rotated.x(), anchor.y(), anchor.z() + rotated.z());
    SlotStatus status = SlotStatus::Valid;
    slot->world_position = fitter.fit(ideal,
                                      plan.slot_half_width[index],
                                      plan.slot_half_depth[index],
                                      slot->heavy,
                                      status);
    slot->status = status;
    slot->facing = request.facing;
    if (status == SlotStatus::Adjusted) {
      plan.displacement += (slot->world_position - ideal).length();
    }
    if (status == SlotStatus::Blocked) {
      ++plan.blocked_count;
    } else if (status == SlotStatus::Adjusted) {
      ++plan.adjusted_count;
    }
  }

  if (plan.blocked_count == static_cast<int>(plan.slot_list.size())) {
    plan.rejection_reason =
        QCoreApplication::translate(
            "Formation", "No part of this formation fits on the chosen ground.")
            .toStdString();
    return plan;
  }

  plan.valid = true;
  return plan;
}

auto ArmyFormationPlanner::scatter_offsets(int count,
                                           float spacing) -> std::vector<QVector3D> {
  std::vector<QVector3D> offsets;
  if (count <= 0) {
    return offsets;
  }
  offsets.reserve(static_cast<std::size_t>(count));
  int const side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  for (int i = 0; i < count; ++i) {
    float const column = static_cast<float>(i % side);
    float const row = static_cast<float>(i / side);
    float const half = static_cast<float>(side - 1) * 0.5F;
    offsets.emplace_back((column - half) * spacing, 0.0F, (row - half) * spacing);
  }
  return offsets;
}

auto ArmyFormationPlanner::scatter_layout(
    const std::vector<ArmyFormationMember>& members,
    float spacing) -> ArmyFormationLayout {
  ArmyFormationLayout layout;
  layout.spacing = spacing;
  layout.slot_spacing = spacing;
  layout.doctrine = k_neutral_doctrine;
  auto const offsets = scatter_offsets(static_cast<int>(members.size()), spacing);
  layout.slot_list.reserve(members.size());
  Bounds bounds;
  for (std::size_t i = 0; i < members.size() && i < offsets.size(); ++i) {
    FormationSlot slot;
    slot.id = static_cast<int>(i);
    slot.occupant = members[i].entity_id;
    slot.local_offset = offsets[i];
    layout.slot_list.push_back(slot);
    bounds.expand(offsets[i]);
  }
  layout.frontage = bounds.width();
  layout.depth = bounds.depth();
  layout.valid = !layout.slot_list.empty();
  if (!layout.valid) {
    layout.rejection_reason = "No members to scatter.";
  }
  return layout;
}

auto ArmyFormationPlanner::plan(const std::vector<ArmyFormationMember>& members,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  return plan(members, request, nullptr);
}

} // namespace Game::Formation
