#include "army_formation_planner.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <unordered_map>

#include "../core/component_core.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/formation_combat_geometry.h"
#include "../systems/nation_registry.h"
#include "../systems/nav_grid.h"
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

class ClaimGrid {
public:
  explicit ClaimGrid(float cell)
      : m_cell(std::max(cell, 0.05F)) {}

  [[nodiscard]] auto is_free(const QVector3D& point, float clearance) const -> bool {
    auto const cell_x = to_cell(point.x());
    auto const cell_z = to_cell(point.z());
    int const reach = static_cast<int>(std::ceil(clearance / m_cell)) + 1;
    for (int dx = -reach; dx <= reach; ++dx) {
      for (int dz = -reach; dz <= reach; ++dz) {
        auto const it = m_cells.find(key(cell_x + dx, cell_z + dz));
        if (it == m_cells.end()) {
          continue;
        }
        for (const auto& claimed : it->second) {
          float const off_x = claimed.point.x() - point.x();
          float const off_z = claimed.point.z() - point.z();
          float const apart = clearance + claimed.clearance;
          if ((off_x * off_x + off_z * off_z) < apart * apart) {
            return false;
          }
        }
      }
    }
    return true;
  }

  void claim(const QVector3D& point, float clearance) {
    m_cells[key(to_cell(point.x()), to_cell(point.z()))].push_back({point, clearance});
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
    float clearance{0.0F};
  };

  std::unordered_map<std::uint64_t, std::vector<Claim>> m_cells;
  float m_cell{1.0F};
};

class SlotTerrainFitter {
public:
  SlotTerrainFitter(float spacing, bool enabled, std::size_t expected_slots)
      : m_grid(std::max(spacing, 0.5F))
      , m_separation(spacing)
      , m_enabled(enabled) {
    m_grid.reserve(expected_slots);
  }

  auto fit(const QVector3D& ideal, float clearance, SlotStatus& status) -> QVector3D {
    if (!m_enabled) {
      m_grid.claim(ideal, clearance);
      status = SlotStatus::Valid;
      return ideal;
    }

    if (m_grid.is_free(ideal, clearance) &&
        Game::Systems::NavGrid::is_world_position_walkable(ideal)) {
      m_grid.claim(ideal, clearance);
      status = SlotStatus::Valid;
      return ideal;
    }

    constexpr int k_rings = 6;
    constexpr int k_samples = 12;
    float const step = std::max(m_separation, clearance * 2.0F) * 1.05F;
    for (int ring = 1; ring <= k_rings; ++ring) {
      float const radius = step * static_cast<float>(ring);
      for (int sample = 0; sample < k_samples; ++sample) {
        float const angle =
            (static_cast<float>(sample) / static_cast<float>(k_samples)) * 2.0F * k_pi +
            static_cast<float>(ring) * 0.37F;
        QVector3D const candidate(ideal.x() + std::cos(angle) * radius,
                                  ideal.y(),
                                  ideal.z() + std::sin(angle) * radius);
        if (!m_grid.is_free(candidate, clearance)) {
          continue;
        }
        if (!Game::Systems::NavGrid::is_world_position_walkable(candidate)) {
          continue;
        }
        m_grid.claim(candidate, clearance);
        status = SlotStatus::Adjusted;
        return candidate;
      }
    }

    auto const origin = Game::Systems::NavGrid::world_to_grid(ideal.x(), ideal.z());
    if (auto const nearest =
            Game::Systems::NavGrid::find_nearest_walkable_grid(origin, k_wide_cells)) {
      QVector3D const grounded = Game::Systems::NavGrid::grid_to_world(*nearest);
      QVector3D const candidate(grounded.x(), ideal.y(), grounded.z());
      if (m_grid.is_free(candidate, clearance)) {
        m_grid.claim(candidate, clearance);
        status = SlotStatus::Adjusted;
        return candidate;
      }
    }

    status = SlotStatus::Blocked;
    return ideal;
  }

private:
  static constexpr int k_wide_cells = 12;

  ClaimGrid m_grid;
  float m_separation{1.0F};
  bool m_enabled{true};
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
    resolve_overlaps(slot_list, half_width, half_depth, lane_for(spacing) * 0.5F);
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

  auto const layout = Game::Systems::FormationCombat::resolve_layout(entity);
  if (layout.all_slots.empty()) {
    return;
  }
  float extent_x = 0.0F;
  float extent_z = 0.0F;
  for (const auto& slot : layout.all_slots) {
    extent_x = std::max(extent_x, std::abs(slot.local_x));
    extent_z = std::max(extent_z, std::abs(slot.local_z));
  }
  member.individuals = std::max(1, static_cast<int>(layout.all_slots.size()));
  member.files = std::max(1, layout.cols);
  member.soldier_body_radius = std::max(0.05F, layout.body_radius);

  member.soldier_file_step =
      layout.cols > 1 ? (extent_x * 2.0F) / static_cast<float>(layout.cols - 1)
                      : std::max(0.1F, layout.spacing);
  int const rows = std::max(1, layout.rows);
  member.soldier_rank_step = rows > 1 ? (extent_z * 2.0F) / static_cast<float>(rows - 1)
                                      : std::max(0.1F, layout.spacing);
  member.half_width = std::max(member.half_width, extent_x + layout.body_radius);
  member.half_depth = std::max(member.half_depth, extent_z + layout.body_radius);
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
  int const rows = (member.individuals + files - 1) / files;
  member.half_width = static_cast<float>(files - 1) * member.soldier_file_step * 0.5F +
                      member.soldier_body_radius;
  member.half_depth = static_cast<float>(rows - 1) * member.soldier_rank_step * 0.5F +
                      member.soldier_body_radius;
}

namespace {

auto plan_fitting_the_ground(const std::vector<ArmyFormationMember>& members,
                             const ArmyFormationRequest& request,
                             const ArmyFormation* previous) -> ArmyFormationPlan {
  constexpr int k_attempts = 4;
  constexpr float k_narrowing = 0.62F;
  constexpr float k_tolerated_blocked_share = 0.12F;

  ArmyFormationPlan best;
  ArmyFormationRequest attempt = request;
  for (int index = 0; index < k_attempts; ++index) {
    auto plan = ArmyFormationPlanner::place(
        ArmyFormationPlanner::build_layout(members, attempt, previous), attempt);
    if (!plan.valid) {
      if (best.slot_list.empty()) {
        best = std::move(plan);
      }
      break;
    }
    auto const total = static_cast<float>(plan.slot_list.size());
    float const blocked_share =
        total > 0.0F ? static_cast<float>(plan.blocked_count) / total : 1.0F;
    if (best.slot_list.empty() || plan.blocked_count < best.blocked_count) {
      best = plan;
    }
    if (blocked_share <= k_tolerated_blocked_share) {
      return best;
    }

    float const current = attempt.frontage > 0.01F ? attempt.frontage : plan.frontage;
    attempt.frontage = std::max(1.0F, current * k_narrowing);
  }
  return best;
}

} // namespace

auto ArmyFormationPlanner::plan(Engine::Core::World& world,
                                const ArmyFormationRequest& request)
    -> ArmyFormationPlan {
  const ArmyFormation* previous =
      request.group_id != k_invalid_group
          ? ArmyFormationRegistry::for_world(world).find(request.group_id)
          : nullptr;
  const auto members = collect_members(world, request.members);
  return plan_fitting_the_ground(members, request, previous);
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

  if (request.preserve_previous_slots && request.group_id != k_invalid_group) {
    const auto* previous = previous_group;
    if (previous != nullptr) {
      std::vector<FormationSlot> reordered = layout.slot_list;
      std::vector<bool> claimed(reordered.size(), false);
      std::unordered_map<EntityID, std::size_t> desired;
      for (const auto& old_slot : previous->slot_list) {
        if (old_slot.occupant == 0U) {
          continue;
        }
        auto const index = static_cast<std::size_t>(old_slot.id);
        if (index < reordered.size()) {
          desired.emplace(old_slot.occupant, index);
        }
      }
      std::vector<EntityID> unassigned;
      std::vector<EntityID> occupants(reordered.size(), 0U);
      for (const auto& slot : layout.slot_list) {
        auto it = desired.find(slot.occupant);
        if (it != desired.end() && !claimed[it->second]) {
          claimed[it->second] = true;
          occupants[it->second] = slot.occupant;
        } else {
          unassigned.push_back(slot.occupant);
        }
      }
      std::size_t next = 0;
      for (std::size_t i = 0; i < occupants.size(); ++i) {
        if (occupants[i] != 0U) {
          continue;
        }
        if (next < unassigned.size()) {
          occupants[i] = unassigned[next++];
        }
      }
      for (std::size_t i = 0; i < reordered.size(); ++i) {
        reordered[i].occupant = occupants[i];
      }
      layout.slot_list = reordered;
    }
  }

  {
    std::unordered_map<EntityID, const ArmyFormationMember*> by_id;
    by_id.reserve(shaped.size());
    for (const auto& member : shaped) {
      by_id.emplace(member.entity_id, &member);
    }
    layout.slot_clearance.assign(layout.slot_list.size(), layout.slot_spacing * 0.5F);
    layout.slot_half_width.assign(layout.slot_list.size(), layout.slot_spacing * 0.5F);
    layout.slot_half_depth.assign(layout.slot_list.size(), layout.slot_spacing * 0.5F);
    layout.slot_files.assign(layout.slot_list.size(), 0);
    for (std::size_t i = 0; i < layout.slot_list.size(); ++i) {
      auto const found = by_id.find(layout.slot_list[i].occupant);
      if (found == by_id.end()) {
        continue;
      }
      layout.slot_files[i] = tmpl->unit_files_aspect > 0.0F ? found->second->files : 0;

      layout.slot_clearance[i] =
          std::min(found->second->half_width, found->second->half_depth);
      layout.slot_half_width[i] = found->second->half_width;
      layout.slot_half_depth[i] = found->second->half_depth;
    }
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

  SlotTerrainFitter fitter(
      layout.slot_spacing, request.resolve_terrain, plan.slot_list.size());
  QVector3D const anchor =
      request.resolve_terrain
          ? Game::Systems::NavGrid::snap_to_walkable_ground(request.anchor, 15)
          : request.anchor;
  plan.anchor = anchor;

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
    slot->world_position = fitter.fit(ideal, plan.slot_clearance[index], status);
    slot->status = status;
    slot->facing = request.facing;
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
  return plan_fitting_the_ground(members, request, nullptr);
}

} // namespace Game::Formation
