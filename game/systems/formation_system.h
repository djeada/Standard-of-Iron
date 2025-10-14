#pragma once

#include <QVector3D>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game {
namespace Systems {

enum class FormationType { Roman, Barbarian };

}
} // namespace Game

namespace std {
template <> struct hash<Game::Systems::FormationType> {
  std::size_t operator()(const Game::Systems::FormationType &ft) const {
    return std::hash<int>()(static_cast<int>(ft));
  }
};
} // namespace std

namespace Game {
namespace Systems {

class IFormation {
public:
  virtual ~IFormation() = default;

  virtual std::vector<QVector3D>
  calculatePositions(int unitCount, const QVector3D &center,
                     float baseSpacing = 1.0f) const = 0;

  virtual FormationType getType() const = 0;
};

class RomanFormation : public IFormation {
public:
  std::vector<QVector3D>
  calculatePositions(int unitCount, const QVector3D &center,
                     float baseSpacing = 1.0f) const override;

  FormationType getType() const override { return FormationType::Roman; }
};

class BarbarianFormation : public IFormation {
public:
  std::vector<QVector3D>
  calculatePositions(int unitCount, const QVector3D &center,
                     float baseSpacing = 1.0f) const override;

  FormationType getType() const override { return FormationType::Barbarian; }
};

class FormationSystem {
public:
  static FormationSystem &instance();

  std::vector<QVector3D> getFormationPositions(FormationType type,
                                               int unitCount,
                                               const QVector3D &center,
                                               float baseSpacing = 1.0f);

  void registerFormation(FormationType type,
                         std::unique_ptr<IFormation> formation);

  const IFormation *getFormation(FormationType type) const;

private:
  FormationSystem();
  void initializeDefaults();

  std::unordered_map<FormationType, std::unique_ptr<IFormation>> m_formations;
};

} // namespace Systems
} // namespace Game
