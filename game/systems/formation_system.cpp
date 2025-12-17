#include "formation_system.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <memory>
#include <qglobal.h>
#include <qvectornd.h>
#include <random>
#include <utility>
#include <vector>

namespace Game::Systems {

namespace {
constexpr float ROMAN_LINE_SPACING = 3.5F;
constexpr float ROMAN_UNIT_SPACING = 2.5F;
constexpr float CARTHAGE_LINE_SPACING = 3.0F;
constexpr float CARTHAGE_UNIT_SPACING = 2.8F;

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
  return type == Game::Units::TroopType::Healer;
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

  if (!infantry.empty()) {
    int const units_per_row =
        std::max(3, std::min(8, static_cast<int>(infantry.size())));
    float const spacing = ROMAN_UNIT_SPACING * base_spacing;

    for (size_t i = 0; i < infantry.size(); ++i) {
      int const row = static_cast<int>(i) / units_per_row;
      int const col = static_cast<int>(i) % units_per_row;

      float const x_offset = (col - (units_per_row - 1) * 0.5F) * spacing;
      float const z_offset =
          row_offset + row * ROMAN_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }

    int const inf_rows =
        (static_cast<int>(infantry.size()) + units_per_row - 1) / units_per_row;
    row_offset += inf_rows * ROMAN_LINE_SPACING * base_spacing;
  }

  if (!archers.empty()) {
    int const units_per_row =
        std::max(4, std::min(10, static_cast<int>(archers.size())));
    float const spacing = ROMAN_UNIT_SPACING * base_spacing * 0.9F;

    for (size_t i = 0; i < archers.size(); ++i) {
      int const row = static_cast<int>(i) / units_per_row;
      int const col = static_cast<int>(i) % units_per_row;

      float const x_offset = (col - (units_per_row - 1) * 0.5F) * spacing;
      float const z_offset =
          row_offset + row * ROMAN_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }

    int const archer_rows =
        (static_cast<int>(archers.size()) + units_per_row - 1) / units_per_row;
    row_offset += archer_rows * ROMAN_LINE_SPACING * base_spacing;
  }

  if (!cavalry.empty()) {
    float const flank_spacing = ROMAN_UNIT_SPACING * base_spacing * 1.2F;
    float const cavalry_z_offset =
        center.z() - ROMAN_LINE_SPACING * base_spacing * 0.5F;

    for (size_t i = 0; i < cavalry.size(); ++i) {
      float x_offset;
      if (i % 2 == 0) {

        x_offset = (i / 2 + 1) * flank_spacing + 5.0F * base_spacing;
      } else {

        x_offset = -((i / 2 + 1) * flank_spacing + 5.0F * base_spacing);
      }

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), cavalry_z_offset);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }
  }

  if (!siege.empty()) {
    float const spacing = ROMAN_UNIT_SPACING * base_spacing * 1.5F;

    for (size_t i = 0; i < siege.size(); ++i) {
      float const x_offset =
          (static_cast<int>(i) - (static_cast<int>(siege.size()) - 1) * 0.5F) *
          spacing;
      float const z_offset = row_offset + ROMAN_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }

    row_offset += ROMAN_LINE_SPACING * base_spacing * 1.5F;
  }

  if (!support.empty()) {
    float const spacing = ROMAN_UNIT_SPACING * base_spacing;

    for (size_t i = 0; i < support.size(); ++i) {
      float const x_offset = (static_cast<int>(i) -
                              (static_cast<int>(support.size()) - 1) * 0.5F) *
                             spacing;
      float const z_offset = row_offset;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
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

  for (const auto &pos : simple_pos) {
    FormationPosition fpos;
    fpos.position = pos;
    fpos.facing_angle = 0.0F;
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

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-0.3F, 0.3F);

  int const side = std::ceil(std::sqrt(static_cast<float>(unit_count)));

  for (int i = 0; i < unit_count; ++i) {
    int const gx = i % side;
    int const gy = i / side;

    float const base_x = (gx - (side - 1) * 0.5F) * spacing;
    float const base_z = (gy - (side - 1) * 0.5F) * spacing;

    float const jitter_x = dist(rng) * spacing;
    float const jitter_z = dist(rng) * spacing;

    positions.emplace_back(center.x() + base_x + jitter_x, center.y(),
                           center.z() + base_z + jitter_z);
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

  if (!siege.empty()) {
    float const spacing = CARTHAGE_UNIT_SPACING * base_spacing * 2.0F;

    for (size_t i = 0; i < siege.size(); ++i) {
      float const x_offset =
          (static_cast<int>(i) - (static_cast<int>(siege.size()) - 1) * 0.5F) *
          spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + row_offset);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }

    row_offset += CARTHAGE_LINE_SPACING * base_spacing * 1.5F;
  }

  std::vector<UnitFormationInfo> center_units;
  center_units.insert(center_units.end(), infantry.begin(), infantry.end());
  center_units.insert(center_units.end(), archers.begin(), archers.end());

  if (!center_units.empty()) {
    int const units_per_row =
        std::max(4, std::min(7, static_cast<int>(center_units.size())));
    float const spacing = CARTHAGE_UNIT_SPACING * base_spacing;

    for (size_t i = 0; i < center_units.size(); ++i) {
      int const row = static_cast<int>(i) / units_per_row;
      int const col = static_cast<int>(i) % units_per_row;

      float const x_offset = (col - (units_per_row - 1) * 0.5F) * spacing;
      float const z_offset =
          row_offset + row * CARTHAGE_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }

    int const center_rows =
        (static_cast<int>(center_units.size()) + units_per_row - 1) /
        units_per_row;
    row_offset += center_rows * CARTHAGE_LINE_SPACING * base_spacing;
  }

  if (!cavalry.empty()) {
    float const flank_spacing = CARTHAGE_UNIT_SPACING * base_spacing * 1.3F;

    float const cavalry_z_offset =
        center.z() - CARTHAGE_LINE_SPACING * base_spacing * 1.0F;

    int const right_flank_count = (static_cast<int>(cavalry.size()) + 1) / 2;
    int const left_flank_count =
        static_cast<int>(cavalry.size()) - right_flank_count;

    for (int i = 0; i < right_flank_count; ++i) {
      float const x_offset = (i + 1) * flank_spacing + 6.0F * base_spacing;
      float const z_forward = i * -0.5F * base_spacing;

      FormationPosition pos;
      pos.position = QVector3D(center.x() + x_offset, center.y(),
                               cavalry_z_offset + z_forward);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }

    for (int i = 0; i < left_flank_count; ++i) {
      float const x_offset = -((i + 1) * flank_spacing + 6.0F * base_spacing);
      float const z_forward = i * -0.3F * base_spacing;

      FormationPosition pos;
      pos.position = QVector3D(center.x() + x_offset, center.y(),
                               cavalry_z_offset + z_forward);
      pos.facing_angle = forward_facing;
      positions.push_back(pos);
    }
  }

  if (!support.empty()) {
    float const spacing = CARTHAGE_UNIT_SPACING * base_spacing;

    for (size_t i = 0; i < support.size(); ++i) {
      float const x_offset = (static_cast<int>(i) -
                              (static_cast<int>(support.size()) - 1) * 0.5F) *
                             spacing;
      float const z_offset = row_offset + CARTHAGE_LINE_SPACING * base_spacing;

      FormationPosition pos;
      pos.position =
          QVector3D(center.x() + x_offset, center.y(), center.z() + z_offset);
      pos.facing_angle = forward_facing;
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

  std::mt19937 rng(84);
  std::uniform_real_distribution<float> dist(-0.25F, 0.25F);

  int const rows = std::max(1, static_cast<int>(std::sqrt(unit_count * 0.8F)));
  int const cols = (unit_count + rows - 1) / rows;

  for (int i = 0; i < unit_count; ++i) {
    int const row = i / cols;
    int const col = i % cols;

    float const base_x = (col - (cols - 1) * 0.5F) * spacing;
    float const base_z = (row - (rows - 1) * 0.5F) * spacing * 0.85F;

    float const jitter_x = dist(rng) * spacing;
    float const jitter_z = dist(rng) * spacing;

    positions.emplace_back(center.x() + base_x + jitter_x, center.y(),
                           center.z() + base_z + jitter_z);
  }

  return positions;
}

auto FormationSystem::instance() -> FormationSystem & {
  static FormationSystem inst;
  return inst;
}

FormationSystem::FormationSystem() { initializeDefaults(); }

void FormationSystem::initializeDefaults() {
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

void FormationSystem::registerFormation(FormationType type,
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
