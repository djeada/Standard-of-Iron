#pragma once

#include "../units/troop_type.h"
#include <QVector3D>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Core {
using EntityID = unsigned int;
class World;
} // namespace Engine::Core

namespace Game::Systems {

enum class FormationType { Roman, Barbarian, Carthage };

struct UnitFormationInfo {
  Engine::Core::EntityID entity_id;
  Game::Units::TroopType troop_type;
  QVector3D current_position;
};

struct FormationPosition {
  QVector3D position;
  float facing_angle;
  Engine::Core::EntityID entity_id{0};
};

} // namespace Game::Systems

namespace std {
template <> struct hash<Game::Systems::FormationType> {
  auto operator()(const Game::Systems::FormationType &ft) const -> std::size_t {
    return std::hash<int>()(static_cast<int>(ft));
  }
};
} // namespace std

namespace Game::Systems {

class IFormation {
public:
  virtual ~IFormation() = default;

  [[nodiscard]] virtual auto calculatePositions(
      int unit_count, const QVector3D &center,
      float base_spacing = 1.0F) const -> std::vector<QVector3D> = 0;

  [[nodiscard]] virtual auto calculateFormationPositions(
      const std::vector<UnitFormationInfo> &units, const QVector3D &center,
      float base_spacing = 1.0F) const -> std::vector<FormationPosition> = 0;

  [[nodiscard]] virtual auto getType() const -> FormationType = 0;
};

class RomanFormation : public IFormation {
public:
  [[nodiscard]] auto calculatePositions(int unit_count, const QVector3D &center,
                                        float base_spacing = 1.0F) const
      -> std::vector<QVector3D> override;

  [[nodiscard]] auto
  calculateFormationPositions(const std::vector<UnitFormationInfo> &units,
                              const QVector3D &center,
                              float base_spacing = 1.0F) const
      -> std::vector<FormationPosition> override;

  [[nodiscard]] auto getType() const -> FormationType override {
    return FormationType::Roman;
  }
};

class BarbarianFormation : public IFormation {
public:
  [[nodiscard]] auto calculatePositions(int unit_count, const QVector3D &center,
                                        float base_spacing = 1.0F) const
      -> std::vector<QVector3D> override;

  [[nodiscard]] auto
  calculateFormationPositions(const std::vector<UnitFormationInfo> &units,
                              const QVector3D &center,
                              float base_spacing = 1.0F) const
      -> std::vector<FormationPosition> override;

  [[nodiscard]] auto getType() const -> FormationType override {
    return FormationType::Barbarian;
  }
};

class CarthageFormation : public IFormation {
public:
  [[nodiscard]] auto calculatePositions(int unit_count, const QVector3D &center,
                                        float base_spacing = 1.0F) const
      -> std::vector<QVector3D> override;

  [[nodiscard]] auto
  calculateFormationPositions(const std::vector<UnitFormationInfo> &units,
                              const QVector3D &center,
                              float base_spacing = 1.0F) const
      -> std::vector<FormationPosition> override;

  [[nodiscard]] auto getType() const -> FormationType override {
    return FormationType::Carthage;
  }
};

class FormationSystem {
public:
  static auto instance() -> FormationSystem &;

  auto
  get_formation_positions(FormationType type, int unit_count,
                          const QVector3D &center,
                          float base_spacing = 1.0F) -> std::vector<QVector3D>;

  auto get_formation_positions_with_facing(
      FormationType type, const std::vector<UnitFormationInfo> &units,
      const QVector3D &center,
      float base_spacing = 1.0F) -> std::vector<FormationPosition>;

  void registerFormation(FormationType type,
                         std::unique_ptr<IFormation> formation);

  auto get_formation(FormationType type) const -> const IFormation *;

private:
  FormationSystem();
  void initializeDefaults();

  std::unordered_map<FormationType, std::unique_ptr<IFormation>> m_formations;
};

} // namespace Game::Systems
