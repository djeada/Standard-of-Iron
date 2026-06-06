#include "formation_system.h"

#include <QDebug>
#include <qglobal.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include "../units/troop_config.h"

namespace Game::Systems {

namespace {
constexpr float ROMAN_LINE_SPACING = 3.5F;
constexpr float ROMAN_UNIT_SPACING = 2.5F;
constexpr float CARTHAGE_LINE_SPACING = 3.0F;
constexpr float CARTHAGE_UNIT_SPACING = 2.8F;
constexpr float k_roman_infantry_lateral_spacing_scale = 1.10F;
constexpr float k_roman_infantry_depth_spacing_scale = 1.12F;
constexpr float k_cavalry_lateral_spacing_scale = 0.72F;
constexpr float k_cavalry_rank_stagger_scale = 0.50F;
constexpr float k_cavalry_rear_depth_bias_scale = 0.18F;

auto get_unit_spacing(Game::Units::TroopType type, float base_spacing) -> float {
  float const selection_size =
      Game::Units::TroopConfig::instance().get_selection_ring_size(type);
  return (selection_size * 2.0F + 0.5F) * base_spacing;
}

enum class TacticalPlacement {
  CenterBlock,
  SplitFlanks
};

struct TacticalLineRule {
  std::vector<Game::Units::TroopType> troop_types;
  TacticalPlacement placement{TacticalPlacement::CenterBlock};
  int max_per_row{1};
  int min_per_row{1};
  float lateral_spacing_scale{1.0F};
  float depth_spacing_scale{1.0F};
  float line_gap_scale{1.0F};
  float row_echelon_scale{0.0F};
  float row_stagger_scale{0.0F};
  float lateral_jitter_scale{0.0F};
  float depth_jitter_scale{0.0F};
  float front_offset_scale{0.0F};
  float flank_gap_scale{1.8F};
  float right_side_weight{0.5F};
  float flank_forward_step_scale{0.0F};
  bool consumes_depth{true};
};

struct TacticalRuleSet {
  std::vector<TacticalLineRule> lines;
};

struct AssignedLine {
  const TacticalLineRule* rule{nullptr};
  std::vector<UnitFormationInfo> units;
};

struct FormationBounds {
  bool valid{false};
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};
};

auto signed_noise(std::uint32_t seed) -> float {
  seed ^= seed >> 16;
  seed *= 0x7feb352dU;
  seed ^= seed >> 15;
  seed *= 0x846ca68bU;
  seed ^= seed >> 16;
  return (float(seed & 0xFFFFU) / 32767.5F) - 1.0F;
}

void expand_bounds(FormationBounds& bounds, const QVector3D& pos) {
  if (!bounds.valid) {
    bounds.valid = true;
    bounds.min_x = bounds.max_x = pos.x();
    bounds.min_z = bounds.max_z = pos.z();
    return;
  }
  bounds.min_x = std::min(bounds.min_x, pos.x());
  bounds.max_x = std::max(bounds.max_x, pos.x());
  bounds.min_z = std::min(bounds.min_z, pos.z());
  bounds.max_z = std::max(bounds.max_z, pos.z());
}

auto contains_troop_type(const TacticalLineRule& rule,
                         Game::Units::TroopType troop_type) -> bool {
  return std::find(rule.troop_types.begin(), rule.troop_types.end(), troop_type) !=
         rule.troop_types.end();
}

auto troop_type_order(const TacticalLineRule& rule,
                      Game::Units::TroopType troop_type) -> int {
  auto it = std::find(rule.troop_types.begin(), rule.troop_types.end(), troop_type);
  if (it == rule.troop_types.end()) {
    return static_cast<int>(rule.troop_types.size());
  }
  return static_cast<int>(std::distance(rule.troop_types.begin(), it));
}

void sort_line_units(const TacticalLineRule& rule,
                     std::vector<UnitFormationInfo>& units) {
  std::stable_sort(units.begin(),
                   units.end(),
                   [&rule](const UnitFormationInfo& a, const UnitFormationInfo& b) {
                     int const a_order = troop_type_order(rule, a.troop_type);
                     int const b_order = troop_type_order(rule, b.troop_type);
                     if (a_order != b_order) {
                       return a_order < b_order;
                     }
                     if (a.current_position.x() != b.current_position.x()) {
                       return a.current_position.x() < b.current_position.x();
                     }
                     return a.entity_id < b.entity_id;
                   });
}

auto max_line_spacing(const std::vector<UnitFormationInfo>& units,
                      float base_spacing) -> float {
  float spacing = base_spacing;
  for (const auto& unit : units) {
    spacing = std::max(spacing, get_unit_spacing(unit.troop_type, base_spacing));
  }
  return spacing;
}

auto build_assigned_lines(const TacticalRuleSet& rules,
                          const std::vector<UnitFormationInfo>& units)
    -> std::vector<AssignedLine> {
  std::vector<AssignedLine> assigned;
  assigned.reserve(rules.lines.size());
  for (const auto& rule : rules.lines) {
    assigned.push_back({&rule, {}});
  }

  for (const auto& unit : units) {
    bool matched = false;
    for (auto& line : assigned) {
      if (line.rule != nullptr && contains_troop_type(*line.rule, unit.troop_type)) {
        line.units.push_back(unit);
        matched = true;
        break;
      }
    }
    if (!matched) {
      qWarning() << "Unassigned troop type in tactical formation"
                 << static_cast<int>(unit.troop_type) << "- sending to rear support";
      if (!assigned.empty()) {
        assigned.back().units.push_back(unit);
      }
    }
  }

  for (auto& line : assigned) {
    if (line.rule != nullptr) {
      sort_line_units(*line.rule, line.units);
    }
  }
  return assigned;
}

auto validate_ruleset(const TacticalRuleSet& rules,
                      FormationType formation_type) -> bool {
  std::vector<Game::Units::TroopType> seen;
  for (const auto& rule : rules.lines) {
    for (auto troop_type : rule.troop_types) {
      if (std::find(seen.begin(), seen.end(), troop_type) != seen.end()) {
        qWarning() << "Duplicate troop mapping in formation ruleset"
                   << static_cast<int>(formation_type) << static_cast<int>(troop_type);
        Q_ASSERT(false);
        return false;
      }
      seen.push_back(troop_type);
    }
  }
  return true;
}

auto calculate_balanced_rows(int total_units,
                             int max_per_row,
                             int min_per_row) -> std::vector<int> {
  std::vector<int> row_sizes;

  if (total_units <= 0) {
    return row_sizes;
  }

  if (total_units <= max_per_row) {
    row_sizes.push_back(total_units);
    return row_sizes;
  }

  int const num_rows = (total_units + max_per_row - 1) / max_per_row;

  int const base_per_row = total_units / num_rows;
  int const extra_units = total_units % num_rows;

  for (int r = 0; r < num_rows; ++r) {

    int units_in_row = base_per_row + (r < extra_units ? 1 : 0);
    units_in_row = std::max(min_per_row, std::min(max_per_row, units_in_row));
    row_sizes.push_back(units_in_row);
  }

  return row_sizes;
}

auto cavalry_lateral_spacing(float forward_spacing) -> float {
  return forward_spacing * k_cavalry_lateral_spacing_scale;
}

auto cavalry_front_rank_index(int row, int rows) -> int {
  return std::max(0, rows - 1 - row);
}

auto cavalry_row_yaw(int row, int rows) -> float {
  switch (cavalry_front_rank_index(row, rows) % 3) {
  case 1:
    return -5.0F;
  case 2:
    return 5.0F;
  default:
    return 0.0F;
  }
}

auto roman_infantry_local_offset(int row, int col, int rows, int cols, float spacing)
    -> FormationOffset {
  float const lateral =
      (col - (cols - 1) * 0.5F) * spacing * k_roman_infantry_lateral_spacing_scale;
  float const depth =
      (row - (rows - 1) * 0.5F) * spacing * k_roman_infantry_depth_spacing_scale;

  float const stagger = (row % 2 == 1) ? spacing * 0.15F : 0.0F;
  return {lateral + stagger, depth, 0.0F};
}

auto cavalry_local_offset(int row, int col, int rows, int cols, float spacing)
    -> FormationOffset {
  float const side_spacing = cavalry_lateral_spacing(spacing);
  float const centered_col = float(col) - float(cols - 1) * 0.5F;
  float const centered_row = float(row) - float(rows - 1) * 0.5F;
  float const stagger =
      (row % 2 == 1) ? side_spacing * k_cavalry_rank_stagger_scale : 0.0F;
  float const rear_depth_bias = float(cavalry_front_rank_index(row, rows)) * spacing *
                                k_cavalry_rear_depth_bias_scale;
  return {(centered_col * side_spacing) + stagger,
          (centered_row * spacing) - rear_depth_bias,
          cavalry_row_yaw(row, rows)};
}

auto carthage_infantry_local_offset(int idx,
                                    int row,
                                    int col,
                                    int rows,
                                    int cols,
                                    float spacing,
                                    std::uint32_t seed) -> FormationOffset {
  float const row_normalized = float(row) / float(rows > 1 ? rows - 1 : 1);
  float const col_normalized =
      float(col - (cols - 1) * 0.5F) / (cols > 1 ? (cols - 1) * 0.5F : 1);

  float const spread_factor = 1.0F + row_normalized * 0.3F;
  float const row_spacing = spacing * (1.0F + row_normalized * 0.15F);

  float offset_x = (col - (cols - 1) * 0.5F) * spacing * spread_factor;
  float offset_z = (row - (rows - 1) * 0.5F) * row_spacing;

  if (row % 2 == 1) {
    offset_x += spacing * 0.35F;
  }

  float const rank_wave = std::sin(col_normalized * std::numbers::pi_v<float>) *
                          spacing * 0.12F * (1.0F + row_normalized);
  offset_z += rank_wave;

  float const center_push = (1.0F - std::abs(col_normalized)) * spacing * 0.2F;
  offset_z -= center_push;

  std::uint32_t const variation_seed = seed ^ (std::uint32_t(idx) * 2654435761U);
  float const phase = float(variation_seed & 0xFFU) / 255.0F * 6.28318F;

  float const jitter_scale = spacing * 0.08F * (1.0F + row_normalized * 0.5F);
  offset_x += std::sin(phase) * jitter_scale;
  offset_z += std::cos(phase * 1.3F) * jitter_scale * 0.7F;

  int const cluster_id = idx / 4;
  float const cluster_phase = float(cluster_id * 137 + (seed & 0xFFU)) * 0.1F;
  float const cluster_pull = spacing * 0.06F;
  offset_x += std::sin(cluster_phase) * cluster_pull;
  offset_z += std::cos(cluster_phase * 0.7F) * cluster_pull;

  return {offset_x, offset_z, 0.0F};
}

auto carthage_cavalry_local_offset(int idx,
                                   int row,
                                   int col,
                                   int rows,
                                   int cols,
                                   float spacing,
                                   std::uint32_t seed) -> FormationOffset {
  FormationOffset offset = cavalry_local_offset(row, col, rows, cols, spacing);
  std::uint32_t rng_state = seed ^ (std::uint32_t(idx) * 7919U);

  auto fast_random = [](std::uint32_t& state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  offset.offset_x +=
      (fast_random(rng_state) - 0.5F) * cavalry_lateral_spacing(spacing) * 0.18F;
  offset.offset_z += (fast_random(rng_state) - 0.5F) * spacing * 0.12F;

  offset.offset_x +=
      std::sin(float(idx) * 0.7F) * cavalry_lateral_spacing(spacing) * 0.08F;
  offset.offset_z += std::cos(float(idx) * 0.5F) * spacing * 0.06F;
  return offset;
}

auto builder_circle_local_offset(
    int idx, int rows, int cols, float spacing, std::uint32_t seed) -> FormationOffset {
  int const total_units = rows * cols;

  if (total_units <= 1) {
    return {0.0F, 0.0F, 0.0F};
  }

  float const angle =
      (float(idx) / float(total_units)) * 2.0F * std::numbers::pi_v<float>;
  float const radius = spacing * 2.8F;

  float world_x = std::cos(angle) * radius;
  float world_z = std::sin(angle) * radius;

  auto fast_random = [](std::uint32_t& state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };
  std::uint32_t rng_state = seed ^ (std::uint32_t(idx) * 2654435761U);
  float const jitter = spacing * 0.08F;
  world_x += (fast_random(rng_state) - 0.5F) * jitter;
  world_z += (fast_random(rng_state) - 0.5F) * jitter;

  float const yaw_offset =
      std::atan2(-world_x, -world_z) * (180.0F / std::numbers::pi_v<float>);

  float const local_r = std::sqrt(world_x * world_x + world_z * world_z);
  return {0.0F, -local_r, yaw_offset};
}

void center_positions_on_target(std::vector<FormationPosition>& positions,
                                const QVector3D& center) {
  if (positions.empty()) {
    return;
  }

  FormationBounds bounds;
  for (const auto& pos : positions) {
    expand_bounds(bounds, pos.position);
  }
  if (!bounds.valid) {
    return;
  }

  float const anchor_x = (bounds.min_x + bounds.max_x) * 0.5F;
  float const anchor_z = (bounds.min_z + bounds.max_z) * 0.5F;
  for (auto& pos : positions) {
    pos.position.setX(center.x() + (pos.position.x() - anchor_x));
    pos.position.setY(center.y());
    pos.position.setZ(center.z() + (pos.position.z() - anchor_z));
  }
}

void append_center_block_positions(const TacticalLineRule& rule,
                                   const std::vector<UnitFormationInfo>& units,
                                   float base_spacing,
                                   float& cursor_z,
                                   std::vector<FormationPosition>& positions,
                                   FormationBounds& all_bounds,
                                   FormationBounds& body_bounds) {
  if (units.empty()) {
    return;
  }

  float const spacing = max_line_spacing(units, base_spacing);
  float const lateral_spacing = spacing * rule.lateral_spacing_scale;
  float const row_spacing = spacing * rule.depth_spacing_scale;
  int const max_per_row =
      std::max(1, std::min(rule.max_per_row, static_cast<int>(units.size())));
  int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
  auto const row_sizes =
      calculate_balanced_rows(static_cast<int>(units.size()), max_per_row, min_per_row);
  float const line_front_z = cursor_z + rule.front_offset_scale * row_spacing;

  size_t unit_idx = 0;
  float line_rear_z = line_front_z;
  for (size_t row = 0; row < row_sizes.size() && unit_idx < units.size(); ++row) {
    int const units_in_row = row_sizes[row];
    float const row_z = line_front_z - static_cast<float>(row) * row_spacing;
    line_rear_z = std::min(line_rear_z, row_z);
    float const echelon =
        static_cast<float>(row) * lateral_spacing * rule.row_echelon_scale;
    float const stagger =
        (row % 2 == 1U) ? lateral_spacing * rule.row_stagger_scale : 0.0F;

    for (int col = 0; col < units_in_row && unit_idx < units.size(); ++col) {
      float const centered_col =
          static_cast<float>(col) - (static_cast<float>(units_in_row) - 1.0F) * 0.5F;
      float const lateral_noise =
          signed_noise(units[unit_idx].entity_id * 2654435761U +
                       static_cast<std::uint32_t>(row * 97U + col * 17U)) *
          lateral_spacing * rule.lateral_jitter_scale;
      float const depth_noise =
          signed_noise(units[unit_idx].entity_id * 2246822519U +
                       static_cast<std::uint32_t>(row * 53U + col * 29U)) *
          row_spacing * rule.depth_jitter_scale;

      FormationPosition pos;
      pos.position =
          QVector3D(centered_col * lateral_spacing + echelon + stagger + lateral_noise,
                    0.0F,
                    row_z + depth_noise);
      pos.facing_angle = 0.0F;
      pos.entity_id = units[unit_idx].entity_id;
      positions.push_back(pos);
      expand_bounds(all_bounds, pos.position);
      expand_bounds(body_bounds, pos.position);
      ++unit_idx;
    }
  }

  if (rule.consumes_depth) {
    cursor_z = line_rear_z - row_spacing * rule.line_gap_scale;
  }
}

void append_split_flank_positions(const TacticalLineRule& rule,
                                  const std::vector<UnitFormationInfo>& units,
                                  float base_spacing,
                                  float front_anchor_z,
                                  float body_half_width,
                                  std::vector<FormationPosition>& positions,
                                  FormationBounds& all_bounds) {
  if (units.empty()) {
    return;
  }

  std::vector<UnitFormationInfo> sorted_units = units;
  std::stable_sort(sorted_units.begin(),
                   sorted_units.end(),
                   [](const UnitFormationInfo& a, const UnitFormationInfo& b) {
                     if (a.current_position.x() != b.current_position.x()) {
                       return a.current_position.x() < b.current_position.x();
                     }
                     return a.entity_id < b.entity_id;
                   });

  int right_count = static_cast<int>(
      std::round(static_cast<float>(sorted_units.size()) * rule.right_side_weight));
  int left_count = static_cast<int>(sorted_units.size()) - right_count;
  if (sorted_units.size() > 1U) {
    left_count = std::max(1, left_count);
    right_count = std::max(1, right_count);
    if (left_count + right_count > static_cast<int>(sorted_units.size())) {
      if (right_count > left_count) {
        --right_count;
      } else {
        --left_count;
      }
    }
  }

  std::vector<UnitFormationInfo> const left_units(sorted_units.begin(),
                                                  sorted_units.begin() + left_count);
  std::vector<UnitFormationInfo> const right_units(sorted_units.begin() + left_count,
                                                   sorted_units.end());

  auto append_side = [&](const std::vector<UnitFormationInfo>& side_units,
                         float side_sign) {
    if (side_units.empty()) {
      return;
    }

    float const spacing = max_line_spacing(side_units, base_spacing);
    float const lateral_spacing = spacing * rule.lateral_spacing_scale;
    float const row_spacing = spacing * rule.depth_spacing_scale;
    int const max_per_row =
        std::max(1, std::min(rule.max_per_row, static_cast<int>(side_units.size())));
    int const min_per_row = std::max(1, std::min(rule.min_per_row, max_per_row));
    auto const row_sizes = calculate_balanced_rows(
        static_cast<int>(side_units.size()), max_per_row, min_per_row);
    float const flank_center =
        side_sign * (body_half_width + spacing * rule.flank_gap_scale);

    size_t unit_idx = 0;
    for (size_t row = 0; row < row_sizes.size() && unit_idx < side_units.size();
         ++row) {
      int const units_in_row = row_sizes[row];
      float const row_z = front_anchor_z + rule.front_offset_scale * row_spacing -
                          static_cast<float>(row) * row_spacing;
      float const echelon = side_sign * static_cast<float>(row) * lateral_spacing *
                            rule.row_echelon_scale;

      for (int col = 0; col < units_in_row && unit_idx < side_units.size(); ++col) {
        float const centered_col =
            static_cast<float>(col) - (static_cast<float>(units_in_row) - 1.0F) * 0.5F;
        float const lateral_noise =
            signed_noise(side_units[unit_idx].entity_id * 1597334677U +
                         static_cast<std::uint32_t>(row * 41U + col * 11U)) *
            lateral_spacing * rule.lateral_jitter_scale;
        float const depth_noise =
            signed_noise(side_units[unit_idx].entity_id * 3812015801U +
                         static_cast<std::uint32_t>(row * 73U + col * 31U)) *
            row_spacing * rule.depth_jitter_scale;

        FormationPosition pos;
        pos.position = QVector3D(
            flank_center + centered_col * lateral_spacing + echelon + lateral_noise,
            0.0F,
            row_z + centered_col * row_spacing * rule.flank_forward_step_scale +
                depth_noise);
        pos.facing_angle = 0.0F;
        pos.entity_id = side_units[unit_idx].entity_id;
        positions.push_back(pos);
        expand_bounds(all_bounds, pos.position);
        ++unit_idx;
      }
    }
  };

  append_side(left_units, -1.0F);
  append_side(right_units, 1.0F);
}

auto build_rule_driven_positions(const TacticalRuleSet& rules,
                                 const std::vector<UnitFormationInfo>& units,
                                 const QVector3D& center,
                                 float base_spacing) -> std::vector<FormationPosition> {
  std::vector<FormationPosition> positions;
  positions.reserve(units.size());
  if (units.empty()) {
    return positions;
  }

  auto assigned_lines = build_assigned_lines(rules, units);
  FormationBounds all_bounds;
  FormationBounds body_bounds;
  float cursor_z = 0.0F;

  for (const auto& line : assigned_lines) {
    if (line.rule == nullptr ||
        line.rule->placement != TacticalPlacement::CenterBlock) {
      continue;
    }
    append_center_block_positions(*line.rule,
                                  line.units,
                                  base_spacing,
                                  cursor_z,
                                  positions,
                                  all_bounds,
                                  body_bounds);
  }

  float body_half_width = 0.0F;
  if (body_bounds.valid) {
    body_half_width =
        std::max(std::abs(body_bounds.min_x), std::abs(body_bounds.max_x));
  }

  for (const auto& line : assigned_lines) {
    if (line.rule == nullptr ||
        line.rule->placement != TacticalPlacement::SplitFlanks) {
      continue;
    }
    if (!body_bounds.valid) {
      TacticalLineRule collapsed_rule = *line.rule;
      collapsed_rule.placement = TacticalPlacement::CenterBlock;
      collapsed_rule.consumes_depth = true;
      collapsed_rule.front_offset_scale = 0.0F;
      append_center_block_positions(collapsed_rule,
                                    line.units,
                                    base_spacing,
                                    cursor_z,
                                    positions,
                                    all_bounds,
                                    body_bounds);
      body_half_width =
          std::max(std::abs(body_bounds.min_x), std::abs(body_bounds.max_x));
      continue;
    }
    float const front_anchor_z = body_bounds.max_z;
    append_split_flank_positions(*line.rule,
                                 line.units,
                                 base_spacing,
                                 front_anchor_z,
                                 body_half_width,
                                 positions,
                                 all_bounds);
  }

  center_positions_on_target(positions, center);
  return positions;
}

auto roman_tactical_rules() -> const TacticalRuleSet& {
  static TacticalRuleSet const rules = {{
      {{Game::Units::TroopType::Elephant},
       TacticalPlacement::CenterBlock,
       3,
       1,
       1.60F,
       1.20F,
       0.80F},
      {{Game::Units::TroopType::Spearman},
       TacticalPlacement::CenterBlock,
       8,
       2,
       1.18F,
       1.02F,
       0.85F},
      {{Game::Units::TroopType::Swordsman},
       TacticalPlacement::CenterBlock,
       8,
       2,
       1.08F,
       1.00F,
       0.95F},
      {{Game::Units::TroopType::Archer},
       TacticalPlacement::CenterBlock,
       10,
       2,
       1.02F,
       1.05F,
       1.05F,
       0.0F,
       0.20F},
      {{Game::Units::TroopType::Catapult, Game::Units::TroopType::Ballista},
       TacticalPlacement::CenterBlock,
       4,
       1,
       1.45F,
       1.15F,
       1.15F},
      {{Game::Units::TroopType::RomanLegionOrganizer,
        Game::Units::TroopType::RomanVeteranConsul,
        Game::Units::TroopType::RomanFieldCommander,
        Game::Units::TroopType::CarthageMercenaryBroker,
        Game::Units::TroopType::CarthageCavalryPatron,
        Game::Units::TroopType::CarthageElephantMaster,
        Game::Units::TroopType::Healer,
        Game::Units::TroopType::Builder,
        Game::Units::TroopType::Civilian},
       TacticalPlacement::CenterBlock,
       6,
       1,
       1.00F,
       1.00F,
       0.75F},
      {{Game::Units::TroopType::MountedKnight,
        Game::Units::TroopType::HorseSpearman,
        Game::Units::TroopType::HorseArcher},
       TacticalPlacement::SplitFlanks,
       3,
       1,
       1.08F,
       1.00F,
       1.00F,
       0.0F,
       0.0F,
       0.0F,
       0.0F,
       0.0F,
       1.90F,
       0.50F,
       0.00F,
       false},
  }};
  static bool const validated = validate_ruleset(rules, FormationType::Roman);
  Q_UNUSED(validated);
  return rules;
}

auto carthage_tactical_rules() -> const TacticalRuleSet& {
  static TacticalRuleSet const rules = {{
      {{Game::Units::TroopType::Elephant},
       TacticalPlacement::CenterBlock,
       3,
       1,
       1.70F,
       1.15F,
       0.60F},
      {{Game::Units::TroopType::Spearman},
       TacticalPlacement::CenterBlock,
       7,
       2,
       1.16F,
       1.00F,
       0.80F,
       0.30F,
       0.18F,
       0.05F,
       0.04F},
      {{Game::Units::TroopType::Swordsman},
       TacticalPlacement::CenterBlock,
       7,
       2,
       1.08F,
       1.00F,
       0.90F,
       -0.18F,
       0.12F,
       0.04F,
       0.03F},
      {{Game::Units::TroopType::Archer},
       TacticalPlacement::CenterBlock,
       9,
       2,
       1.05F,
       1.00F,
       1.00F,
       -0.25F,
       0.32F,
       0.08F,
       0.06F},
      {{Game::Units::TroopType::Catapult, Game::Units::TroopType::Ballista},
       TacticalPlacement::CenterBlock,
       4,
       1,
       1.55F,
       1.20F,
       1.15F,
       0.10F,
       0.0F,
       0.05F,
       0.04F},
      {{Game::Units::TroopType::RomanLegionOrganizer,
        Game::Units::TroopType::RomanVeteranConsul,
        Game::Units::TroopType::RomanFieldCommander,
        Game::Units::TroopType::CarthageMercenaryBroker,
        Game::Units::TroopType::CarthageCavalryPatron,
        Game::Units::TroopType::CarthageElephantMaster,
        Game::Units::TroopType::Healer,
        Game::Units::TroopType::Builder,
        Game::Units::TroopType::Civilian},
       TacticalPlacement::CenterBlock,
       6,
       1,
       1.00F,
       1.00F,
       0.75F,
       0.10F,
       0.20F,
       0.04F,
       0.03F},
      {{Game::Units::TroopType::MountedKnight,
        Game::Units::TroopType::HorseSpearman,
        Game::Units::TroopType::HorseArcher},
       TacticalPlacement::SplitFlanks,
       3,
       1,
       1.12F,
       1.00F,
       1.00F,
       0.25F,
       0.0F,
       0.08F,
       0.05F,
       0.35F,
       2.10F,
       0.60F,
       0.35F,
       false},
  }};
  static bool const validated = validate_ruleset(rules, FormationType::Carthage);
  Q_UNUSED(validated);
  return rules;
}
} // namespace

auto RomanFormation::calculate_positions(int unit_count,
                                         const QVector3D& center,
                                         float base_spacing) const
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(unit_count);

  if (unit_count <= 0) {
    return positions;
  }

  float spacing = base_spacing * 1.2F;

  if (unit_count > 100) {
    spacing *= 2.0F;
  } else if (unit_count > 50) {
    spacing *= 1.5F;
  }

  int const rows = std::max(1, static_cast<int>(std::sqrt(unit_count * 0.7F)));
  int const cols = (unit_count + rows - 1) / rows;

  for (int i = 0; i < unit_count; ++i) {
    int const row = i / cols;
    int const col = i % cols;

    float const offset_x = (col - (cols - 1) * 0.5F) * spacing;
    float const offset_z = (row - (rows - 1) * 0.5F) * spacing * 0.9F;

    positions.emplace_back(center.x() + offset_x, center.y(), center.z() + offset_z);
  }

  return positions;
}

auto RomanFormation::calculate_formation_positions(
    const std::vector<UnitFormationInfo>& units,
    const QVector3D& center,
    float base_spacing) const -> std::vector<FormationPosition> {
  return build_rule_driven_positions(
      roman_tactical_rules(), units, center, base_spacing);
}

auto BarbarianFormation::calculate_formation_positions(
    const std::vector<UnitFormationInfo>& units,
    const QVector3D& center,
    float base_spacing) const -> std::vector<FormationPosition> {

  std::vector<FormationPosition> positions;
  auto simple_pos =
      calculate_positions(static_cast<int>(units.size()), center, base_spacing);

  for (size_t i = 0; i < simple_pos.size() && i < units.size(); ++i) {
    FormationPosition fpos;
    fpos.position = simple_pos[i];
    fpos.facing_angle = 0.0F;
    fpos.entity_id = units[i].entity_id;
    positions.push_back(fpos);
  }

  return positions;
}

auto BarbarianFormation::calculate_positions(int unit_count,
                                             const QVector3D& center,
                                             float base_spacing) const
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(unit_count);

  if (unit_count <= 0) {
    return positions;
  }

  float spacing = base_spacing * 1.8F;

  if (unit_count > 100) {
    spacing *= 2.0F;
  } else if (unit_count > 50) {
    spacing *= 1.5F;
  }

  int const side = std::ceil(std::sqrt(static_cast<float>(unit_count)));

  for (int i = 0; i < unit_count; ++i) {
    int const gx = i % side;
    int const gy = i / side;

    float const base_x = (gx - (side - 1) * 0.5F) * spacing;
    float const base_z = (gy - (side - 1) * 0.5F) * spacing;

    positions.emplace_back(center.x() + base_x, center.y(), center.z() + base_z);
  }

  return positions;
}

auto CarthageFormation::calculate_formation_positions(
    const std::vector<UnitFormationInfo>& units,
    const QVector3D& center,
    float base_spacing) const -> std::vector<FormationPosition> {
  return build_rule_driven_positions(
      carthage_tactical_rules(), units, center, base_spacing);
}

auto CarthageFormation::calculate_positions(int unit_count,
                                            const QVector3D& center,
                                            float base_spacing) const
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(unit_count);

  if (unit_count <= 0) {
    return positions;
  }

  float spacing = base_spacing * 1.5F;

  if (unit_count > 100) {
    spacing *= 2.0F;
  } else if (unit_count > 50) {
    spacing *= 1.5F;
  }

  int const rows = std::max(1, static_cast<int>(std::sqrt(unit_count * 0.8F)));
  int const cols = (unit_count + rows - 1) / rows;

  for (int i = 0; i < unit_count; ++i) {
    int const row = i / cols;
    int const col = i % cols;

    float const base_x = (col - (cols - 1) * 0.5F) * spacing;
    float const base_z = (row - (rows - 1) * 0.5F) * spacing * 0.85F;

    positions.emplace_back(center.x() + base_x, center.y(), center.z() + base_z);
  }

  return positions;
}

auto FormationSystem::instance() -> FormationSystem& {
  static FormationSystem inst;
  return inst;
}

FormationSystem::FormationSystem() {
  initialize_defaults();
}

void FormationSystem::initialize_defaults() {
  register_formation(FormationType::Roman, std::make_unique<RomanFormation>());
  register_formation(FormationType::Barbarian, std::make_unique<BarbarianFormation>());
  register_formation(FormationType::Carthage, std::make_unique<CarthageFormation>());
}

auto FormationSystem::get_formation_positions(FormationType type,
                                              int unit_count,
                                              const QVector3D& center,
                                              float base_spacing)
    -> std::vector<QVector3D> {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    qWarning() << "Formation type not found, using default spread";
    return RomanFormation().calculate_positions(unit_count, center, base_spacing);
  }

  return it->second->calculate_positions(unit_count, center, base_spacing);
}

auto FormationSystem::get_formation_positions_with_facing(
    FormationType type,
    const std::vector<UnitFormationInfo>& units,
    const QVector3D& center,
    float base_spacing) -> std::vector<FormationPosition> {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    qWarning() << "Formation type not found, using default";
    return RomanFormation().calculate_formation_positions(units, center, base_spacing);
  }

  return it->second->calculate_formation_positions(units, center, base_spacing);
}

void FormationSystem::register_formation(FormationType type,
                                         std::unique_ptr<IFormation> formation) {
  m_formations[type] = std::move(formation);
}

auto FormationSystem::get_formation(FormationType type) const -> const IFormation* {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    return nullptr;
  }
  return it->second.get();
}

auto FormationSystem::get_local_offset(FormationType type,
                                       FormationUnitCategory category,
                                       int idx,
                                       int row,
                                       int col,
                                       int rows,
                                       int cols,
                                       float spacing,
                                       std::uint32_t seed) const -> FormationOffset {
  if (category == FormationUnitCategory::BuilderConstruction) {
    return builder_circle_local_offset(idx, rows, cols, spacing, seed);
  }

  switch (type) {
  case FormationType::Roman:
    if (category == FormationUnitCategory::Cavalry) {
      return cavalry_local_offset(row, col, rows, cols, spacing);
    }
    return roman_infantry_local_offset(row, col, rows, cols, spacing);
  case FormationType::Carthage:
    if (category == FormationUnitCategory::Cavalry) {
      return carthage_cavalry_local_offset(idx, row, col, rows, cols, spacing, seed);
    }
    return carthage_infantry_local_offset(idx, row, col, rows, cols, spacing, seed);
  case FormationType::Barbarian:
    return roman_infantry_local_offset(row, col, rows, cols, spacing);
  }

  return roman_infantry_local_offset(row, col, rows, cols, spacing);
}

} // namespace Game::Systems
