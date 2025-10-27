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

auto RomanFormation::calculatePositions(int unitCount, const QVector3D &center,
                                        float baseSpacing) const
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(unitCount);

  if (unitCount <= 0) {
    return positions;
  }

  float spacing = baseSpacing * 1.2F;

  if (unitCount > 100) {
    spacing *= 2.0F;
  } else if (unitCount > 50) {
    spacing *= 1.5F;
  }

  int const rows = std::max(1, static_cast<int>(std::sqrt(unitCount * 0.7F)));
  int const cols = (unitCount + rows - 1) / rows;

  for (int i = 0; i < unitCount; ++i) {
    int const row = i / cols;
    int const col = i % cols;

    float const offset_x = (col - (cols - 1) * 0.5F) * spacing;
    float const offset_z = (row - (rows - 1) * 0.5F) * spacing * 0.9F;

    positions.emplace_back(center.x() + offset_x, center.y(),
                           center.z() + offset_z);
  }

  return positions;
}

auto BarbarianFormation::calculatePositions(
    int unitCount, const QVector3D &center,
    float baseSpacing) const -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(unitCount);

  if (unitCount <= 0) {
    return positions;
  }

  float spacing = baseSpacing * 1.8F;

  if (unitCount > 100) {
    spacing *= 2.0F;
  } else if (unitCount > 50) {
    spacing *= 1.5F;
  }

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-0.3F, 0.3F);

  int const side = std::ceil(std::sqrt(static_cast<float>(unitCount)));

  for (int i = 0; i < unitCount; ++i) {
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

auto FormationSystem::instance() -> FormationSystem & {
  static FormationSystem inst;
  return inst;
}

FormationSystem::FormationSystem() { initializeDefaults(); }

void FormationSystem::initializeDefaults() {
  registerFormation(FormationType::Roman, std::make_unique<RomanFormation>());
  registerFormation(FormationType::Barbarian,
                    std::make_unique<BarbarianFormation>());
}

auto FormationSystem::getFormationPositions(
    FormationType type, int unitCount, const QVector3D &center,
    float baseSpacing) -> std::vector<QVector3D> {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    qWarning() << "Formation type not found, using default spread";
    return RomanFormation().calculatePositions(unitCount, center, baseSpacing);
  }

  return it->second->calculatePositions(unitCount, center, baseSpacing);
}

void FormationSystem::registerFormation(FormationType type,
                                        std::unique_ptr<IFormation> formation) {
  m_formations[type] = std::move(formation);
}

auto FormationSystem::getFormation(FormationType type) const
    -> const IFormation * {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    return nullptr;
  }
  return it->second.get();
}

} // namespace Game::Systems
