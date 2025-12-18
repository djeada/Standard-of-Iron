#include "formation_system.h"
#include "../units/troop_config.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <memory>
#include <qglobal.h>
#include <qvectornd.h>
#include <utility>
#include <vector>

namespace Game::Systems {

namespace {
constexpr float ROMAN_LINE_SPACING = 3.5F;
constexpr float ROMAN_UNIT_SPACING = 2.5F;
constexpr float CARTHAGE_LINE_SPACING = 3.0F;
constexpr float CARTHAGE_UNIT_SPACING = 2.8F;

auto get_unit_spacing(Game::Units::TroopType type,
                      float base_spacing) -> float {
  float const selection_size =
      Game::Units::TroopConfig::instance().getSelectionRingSize(type);
  return (selection_size * 2.0F + 0.5F) * base_spacing;
}

auto is_infantry(Game::Units::TroopType type) -> bool {
  return type == Game::Units::TroopType::Swordsman ||
         type == Game::Units::TroopType::Spearman;
}

auto is_ranged(Game::Units::TroopType type) -> bool {
  return type == Game::Units::TroopType::Archer;
}

auto is_cavalry(Game::Units::TroopType type) -> bool {
  return type == Game::Units::TroopType::MountedKnight ||
         type == Game::Units::TroopType::HorseArcher ||
         type == Game::Units::TroopType::HorseSpearman;
}

auto is_siege(Game::Units::TroopType type) -> bool {
  return type == Game::Units::TroopType::Catapult ||
         type == Game::Units::TroopType::Ballista;
}

auto is_support(Game::Units::TroopType type) -> bool {
  return type == Game::Units::TroopType::Healer ||
         type == Game::Units::TroopType::Builder;
}

auto calculate_balanced_rows(int total_units, int max_per_row,
                             int min_per_row) -> std::vector<int> {
  std::vector<int> row_sizes;

  if (total_units <= 0) {
    return row_sizes;
  }

  if (total_units <= max_per_row) {
    row_sizes.push_back(total_units);
    return row_sizes;
  }

  int num_rows = (total_units + max_per_row - 1) / max_per_row;

  int base_per_row = total_units / num_rows;
  int extra_units = total_units % num_rows;

  for (int r = 0; r < num_rows; ++r) {

    int units_in_row = base_per_row + (r < extra_units ? 1 : 0);
    units_in_row = std::max(min_per_row, std::min(max_per_row, units_in_row));
    row_sizes.push_back(units_in_row);
  }

  return row_sizes;
}
} // namespace

auto RomanFormation::calculatePositions(int unit_count, const QVector3D &center,
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

    positions.emplace_back(center.x() + offset_x, center.y(),
                           center.z() + offset_z);
  }

  return positions;
}

auto RomanFormation::calculateFormationPositions(
    const std::vector<UnitFormationInfo> &units, const QVector3D &center,
    float base_spacing) const -> std::vector<FormationPosition> {
  std::vector<FormationPosition> positions;
  positions.reserve(units.size());

  if (units.empty()) {
    return positions;
  }

  std::vector<UnitFormationInfo> infantry, archers, cavalry, siege, support;

  for (const auto &unit : units) {
    if (is_infantry(unit.troop_type)) {
      infantry.push_back(unit);
    } else if (is_ranged(unit.troop_type)) {
      archers.push_back(unit);
    } else if (is_cavalry(unit.troop_type)) {
      cavalry.push_back(unit);
    } else if (is_siege(unit.troop_type)) {
      siege.push_back(unit);
    } else if (is_support(unit.troop_type)) {
      support.push_back(unit);
    }
  }

  float const forward_facing = 0.0F;
  float row_offset = 0.0F;

  float max_row_width = 0.0F;

  if (!infantry.empty()) {
    int const max_per_row = std::min(8, static_cast<int>(infantry.size()));
    int const min_per_row = std::max(3, max_per_row / 2);
    auto row_sizes = calculate_balanced_rows(static_cast<int>(infantry.size()),
                                             max_per_row, min_per_row);

    float const unit_spacing =
        get_unit_spacing(infantry[0].troop_type, base_spacing);

    float const row_spacing = unit_spacing;

    int max_units_in_row = 0;
    for (int size : row_sizes) {
      max_units_in_row = std::max(max_units_in_row, size);
    }
    float const type_max_width = (max_units_in_row - 1) * unit_spacing;
    max_row_width = std::max(max_row_width, type_max_width);

    size_t unit_idx = 0;
    for (size_t row = 0; row < row_sizes.size() && unit_idx < infantry.size();
         ++row) {
      int const units_in_row = row_sizes[row];

      for (int col = 0; col < units_in_row && unit_idx < infantry.size();
           ++col) {

        float const x_offset = (col - (units_in_row - 1) * 0.5F) * unit_spacing;

        float const z_offset =
            row_offset - static_cast<float>(row) * row_spacing;

        FormationPosition pos;
        pos.position =
            QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
        pos.facing_angle = forward_facing;
        pos.entity_id = infantry[unit_idx].entity_id;
        positions.push_back(pos);
        ++unit_idx;
      }
    }

    row_offset -= static_cast<float>(row_sizes.size()) * row_spacing;
  }

  if (!archers.empty()) {
    int const max_per_row = std::min(10, static_cast<int>(archers.size()));
    int const min_per_row = std::max(4, max_per_row / 2);
    auto row_sizes = calculate_balanced_rows(static_cast<int>(archers.size()),
                                             max_per_row, min_per_row);

    float const unit_spacing =
        get_unit_spacing(archers[0].troop_type, base_spacing);

    float const row_spacing = unit_spacing;

    int max_units_in_row = 0;
    for (int size : row_sizes) {
      max_units_in_row = std::max(max_units_in_row, size);
    }
    float const type_max_width = (max_units_in_row - 1) * unit_spacing;
    max_row_width = std::max(max_row_width, type_max_width);

    size_t unit_idx = 0;
    for (size_t row = 0; row < row_sizes.size() && unit_idx < archers.size();
         ++row) {
      int const units_in_row = row_sizes[row];

      for (int col = 0; col < units_in_row && unit_idx < archers.size();
           ++col) {

        float const x_offset = (col - (units_in_row - 1) * 0.5F) * unit_spacing;

        float const z_offset =
            row_offset - static_cast<float>(row) * row_spacing;

        FormationPosition pos;
        pos.position =
            QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
        pos.facing_angle = forward_facing;
        pos.entity_id = archers[unit_idx].entity_id;
        positions.push_back(pos);
        ++unit_idx;
      }
    }

    row_offset -= static_cast<float>(row_sizes.size()) * row_spacing;
  }

  if (!cavalry.empty()) {
    float const cavalry_z_offset = center.z();

    for (size_t i = 0; i < cavalry.size(); ++i) {
      float const spacing =
          get_unit_spacing(cavalry[i].troop_type, base_spacing) * 1.2F;
      float x_offset;
      if (i % 2 == 0) {
        x_offset = (i / 2 + 1) * spacing + 5.0F * base_spacing;
      } else {
        x_offset = -((i / 2 + 1) * spacing + 5.0F * base_spacing);
      }

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), cavalry_z_offset);
      pos.facing_angle = forward_facing;
      pos.entity_id = cavalry[i].entity_id;
      positions.push_back(pos);
    }
  }

  if (!siege.empty()) {
    float const spacing =
        get_unit_spacing(siege[0].troop_type, base_spacing) * 1.5F;

    for (size_t i = 0; i < siege.size(); ++i) {
      float const x_offset =
          (static_cast<int>(i) - (static_cast<int>(siege.size()) - 1) * 0.5F) *
          spacing;
      float const z_offset = row_offset - ROMAN_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      pos.entity_id = siege[i].entity_id;
      positions.push_back(pos);
    }

    row_offset -= ROMAN_LINE_SPACING * base_spacing * 1.5F;
  }

  if (!support.empty()) {
    float const spacing = get_unit_spacing(support[0].troop_type, base_spacing);

    for (size_t i = 0; i < support.size(); ++i) {
      float const x_offset = (static_cast<int>(i) -
                              (static_cast<int>(support.size()) - 1) * 0.5F) *
                             spacing;
      float const z_offset = row_offset - ROMAN_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      pos.entity_id = support[i].entity_id;
      positions.push_back(pos);
    }
  }

  return positions;
}

auto BarbarianFormation::calculateFormationPositions(
    const std::vector<UnitFormationInfo> &units, const QVector3D &center,
    float base_spacing) const -> std::vector<FormationPosition> {

  std::vector<FormationPosition> positions;
  auto simple_pos =
      calculatePositions(static_cast<int>(units.size()), center, base_spacing);

  for (size_t i = 0; i < simple_pos.size() && i < units.size(); ++i) {
    FormationPosition fpos;
    fpos.position = simple_pos[i];
    fpos.facing_angle = 0.0F;
    fpos.entity_id = units[i].entity_id;
    positions.push_back(fpos);
  }

  return positions;
}

auto BarbarianFormation::calculatePositions(
    int unit_count, const QVector3D &center,
    float base_spacing) const -> std::vector<QVector3D> {
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

    positions.emplace_back(center.x() + base_x, center.y(),
                           center.z() + base_z);
  }

  return positions;
}

auto CarthageFormation::calculateFormationPositions(
    const std::vector<UnitFormationInfo> &units, const QVector3D &center,
    float base_spacing) const -> std::vector<FormationPosition> {
  std::vector<FormationPosition> positions;
  positions.reserve(units.size());

  if (units.empty()) {
    return positions;
  }

  std::vector<UnitFormationInfo> infantry, archers, cavalry, siege, support;

  for (const auto &unit : units) {
    if (is_infantry(unit.troop_type)) {
      infantry.push_back(unit);
    } else if (is_ranged(unit.troop_type)) {
      archers.push_back(unit);
    } else if (is_cavalry(unit.troop_type)) {
      cavalry.push_back(unit);
    } else if (is_siege(unit.troop_type)) {
      siege.push_back(unit);
    } else if (is_support(unit.troop_type)) {
      support.push_back(unit);
    }
  }

  float const forward_facing = 0.0F;
  float row_offset = 0.0F;

  if (!infantry.empty()) {
    int const max_per_row = std::min(7, static_cast<int>(infantry.size()));
    int const min_per_row = std::max(4, max_per_row / 2);
    auto row_sizes = calculate_balanced_rows(static_cast<int>(infantry.size()),
                                             max_per_row, min_per_row);

    float const spacing =
        get_unit_spacing(infantry[0].troop_type, base_spacing);

    size_t unit_idx = 0;
    for (size_t row = 0; row < row_sizes.size() && unit_idx < infantry.size();
         ++row) {
      int const units_in_row = row_sizes[row];
      float const row_echelon = static_cast<float>(row) * 0.8F * spacing;

      for (int col = 0; col < units_in_row && unit_idx < infantry.size();
           ++col) {
        float const x_offset =
            (col - (units_in_row - 1) * 0.5F) * spacing + row_echelon;
        float const z_offset = row_offset - static_cast<float>(row) *
                                                CARTHAGE_LINE_SPACING *
                                                base_spacing;

        FormationPosition pos;
        pos.position =
            QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
        pos.facing_angle = forward_facing;
        pos.entity_id = infantry[unit_idx].entity_id;
        positions.push_back(pos);
        ++unit_idx;
      }
    }

    row_offset -= static_cast<float>(row_sizes.size()) * CARTHAGE_LINE_SPACING *
                  base_spacing;
  }

  if (!archers.empty()) {
    int const max_per_row = std::min(9, static_cast<int>(archers.size()));
    int const min_per_row = std::max(5, max_per_row / 2);
    auto row_sizes = calculate_balanced_rows(static_cast<int>(archers.size()),
                                             max_per_row, min_per_row);

    float const spacing = get_unit_spacing(archers[0].troop_type, base_spacing);

    size_t unit_idx = 0;
    for (size_t row = 0; row < row_sizes.size() && unit_idx < archers.size();
         ++row) {
      int const units_in_row = row_sizes[row];
      float const row_echelon = -static_cast<float>(row) * 0.8F * spacing;

      for (int col = 0; col < units_in_row && unit_idx < archers.size();
           ++col) {
        float const x_offset =
            (col - (units_in_row - 1) * 0.5F) * spacing + row_echelon;
        float const z_offset = row_offset - static_cast<float>(row) *
                                                CARTHAGE_LINE_SPACING *
                                                base_spacing;

        FormationPosition pos;
        pos.position =
            QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
        pos.facing_angle = forward_facing;
        pos.entity_id = archers[unit_idx].entity_id;
        positions.push_back(pos);
        ++unit_idx;
      }
    }

    row_offset -= static_cast<float>(row_sizes.size()) * CARTHAGE_LINE_SPACING *
                  base_spacing;
  }

  if (!siege.empty()) {
    for (size_t i = 0; i < siege.size(); ++i) {
      float const spacing =
          get_unit_spacing(siege[i].troop_type, base_spacing) * 2.0F;
      float const x_offset =
          (static_cast<int>(i) - (static_cast<int>(siege.size()) - 1) * 0.5F) *
          spacing;

      FormationPosition pos;
      pos.position = QVector3D(center.x() + x_offset, center.y(),
                               center.z() + row_offset -
                                   CARTHAGE_LINE_SPACING * base_spacing);
      pos.facing_angle = forward_facing;
      pos.entity_id = siege[i].entity_id;
      positions.push_back(pos);
    }

    row_offset -= CARTHAGE_LINE_SPACING * base_spacing * 1.5F;
  }

  if (!cavalry.empty()) {
    float const cavalry_z_offset = center.z();

    int const right_flank_count = (static_cast<int>(cavalry.size()) + 1) / 2;
    int const left_flank_count =
        static_cast<int>(cavalry.size()) - right_flank_count;

    for (int i = 0; i < right_flank_count; ++i) {
      float const spacing =
          get_unit_spacing(cavalry[static_cast<size_t>(i)].troop_type,
                           base_spacing) *
          1.3F;
      float const x_offset = (i + 1) * spacing + 6.0F * base_spacing;
      float const z_forward = i * 0.7F * base_spacing;

      FormationPosition pos;
      pos.position = QVector3D(center.x() + x_offset, center.y(),
                               cavalry_z_offset + z_forward);
      pos.facing_angle = forward_facing;
      pos.entity_id = cavalry[static_cast<size_t>(i)].entity_id;
      positions.push_back(pos);
    }

    for (int i = 0; i < left_flank_count; ++i) {
      float const spacing =
          get_unit_spacing(
              cavalry[static_cast<size_t>(right_flank_count + i)].troop_type,
              base_spacing) *
          1.3F;
      float const x_offset = -((i + 1) * spacing + 6.0F * base_spacing);
      float const z_forward = i * 0.7F * base_spacing;

      FormationPosition pos;
      pos.position = QVector3D(center.x() + x_offset, center.y(),
                               cavalry_z_offset + z_forward);
      pos.facing_angle = forward_facing;
      pos.entity_id =
          cavalry[static_cast<size_t>(right_flank_count + i)].entity_id;
      positions.push_back(pos);
    }
  }

  if (!support.empty()) {
    for (size_t i = 0; i < support.size(); ++i) {
      float const spacing =
          get_unit_spacing(support[i].troop_type, base_spacing);
      float const x_offset = (static_cast<int>(i) -
                              (static_cast<int>(support.size()) - 1) * 0.5F) *
                             spacing;
      float const z_offset = row_offset - CARTHAGE_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      pos.entity_id = support[i].entity_id;
      positions.push_back(pos);
    }
  }

  return positions;
}

auto CarthageFormation::calculatePositions(
    int unit_count, const QVector3D &center,
    float base_spacing) const -> std::vector<QVector3D> {
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

    positions.emplace_back(center.x() + base_x, center.y(),
                           center.z() + base_z);
  }

  return positions;
}

auto FormationSystem::instance() -> FormationSystem & {
  static FormationSystem inst;
  return inst;
}

FormationSystem::FormationSystem() { initializeDefaults(); }

void FormationSystem::initialize_defaults() {
  registerFormation(FormationType::Roman, std::make_unique<RomanFormation>());
  registerFormation(FormationType::Barbarian,
                    std::make_unique<BarbarianFormation>());
  registerFormation(FormationType::Carthage,
                    std::make_unique<CarthageFormation>());
}

auto FormationSystem::get_formation_positions(
    FormationType type, int unit_count, const QVector3D &center,
    float base_spacing) -> std::vector<QVector3D> {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    qWarning() << "Formation type not found, using default spread";
    return RomanFormation().calculatePositions(unit_count, center,
                                               base_spacing);
  }

  return it->second->calculatePositions(unit_count, center, base_spacing);
}

auto FormationSystem::get_formation_positions_with_facing(
    FormationType type, const std::vector<UnitFormationInfo> &units,
    const QVector3D &center,
    float base_spacing) -> std::vector<FormationPosition> {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    qWarning() << "Formation type not found, using default";
    return RomanFormation().calculateFormationPositions(units, center,
                                                        base_spacing);
  }

  return it->second->calculateFormationPositions(units, center, base_spacing);
}

void FormationSystem::register_formation(FormationType type,
                                        std::unique_ptr<IFormation> formation) {
  m_formations[type] = std::move(formation);
}

auto FormationSystem::get_formation(FormationType type) const
    -> const IFormation * {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    return nullptr;
  }
  return it->second.get();
}

} // namespace Game::Systems
