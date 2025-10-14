#include "formation_system.h"
#include <QDebug>
#include <cmath>
#include <random>

namespace Game {
namespace Systems {

std::vector<QVector3D>
RomanFormation::calculatePositions(int unitCount, const QVector3D &center,
                                   float baseSpacing) const {
  std::vector<QVector3D> positions;
  positions.reserve(unitCount);

  if (unitCount <= 0)
    return positions;

  float spacing = baseSpacing * 1.2f;

  if (unitCount > 100) {
    spacing *= 2.0f;
  } else if (unitCount > 50) {
    spacing *= 1.5f;
  }

  int rows = std::max(1, static_cast<int>(std::sqrt(unitCount * 0.7f)));
  int cols = (unitCount + rows - 1) / rows;

  for (int i = 0; i < unitCount; ++i) {
    int row = i / cols;
    int col = i % cols;

    float offsetX = (col - (cols - 1) * 0.5f) * spacing;
    float offsetZ = (row - (rows - 1) * 0.5f) * spacing * 0.9f;

    positions.emplace_back(center.x() + offsetX, center.y(),
                           center.z() + offsetZ);
  }

  return positions;
}

std::vector<QVector3D>
BarbarianFormation::calculatePositions(int unitCount, const QVector3D &center,
                                       float baseSpacing) const {
  std::vector<QVector3D> positions;
  positions.reserve(unitCount);

  if (unitCount <= 0)
    return positions;

  float spacing = baseSpacing * 1.8f;

  if (unitCount > 100) {
    spacing *= 2.0f;
  } else if (unitCount > 50) {
    spacing *= 1.5f;
  }

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-0.3f, 0.3f);

  int side = std::ceil(std::sqrt(static_cast<float>(unitCount)));

  for (int i = 0; i < unitCount; ++i) {
    int gx = i % side;
    int gy = i / side;

    float baseX = (gx - (side - 1) * 0.5f) * spacing;
    float baseZ = (gy - (side - 1) * 0.5f) * spacing;

    float jitterX = dist(rng) * spacing;
    float jitterZ = dist(rng) * spacing;

    positions.emplace_back(center.x() + baseX + jitterX, center.y(),
                           center.z() + baseZ + jitterZ);
  }

  return positions;
}

FormationSystem &FormationSystem::instance() {
  static FormationSystem inst;
  return inst;
}

FormationSystem::FormationSystem() { initializeDefaults(); }

void FormationSystem::initializeDefaults() {
  registerFormation(FormationType::Roman, std::make_unique<RomanFormation>());
  registerFormation(FormationType::Barbarian,
                    std::make_unique<BarbarianFormation>());
}

std::vector<QVector3D>
FormationSystem::getFormationPositions(FormationType type, int unitCount,
                                       const QVector3D &center,
                                       float baseSpacing) {
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

const IFormation *FormationSystem::getFormation(FormationType type) const {
  auto it = m_formations.find(type);
  if (it == m_formations.end()) {
    return nullptr;
  }
  return it->second.get();
}

} // namespace Systems
} // namespace Game
