#pragma once

#include <QVector3D>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

enum class FormationType { Roman, Barbarian };

}

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

  [[nodiscard]] virtual auto getType() const -> FormationType = 0;
};

class RomanFormation : public IFormation {
public:
  [[nodiscard]] auto calculatePositions(int unit_count, const QVector3D &center,
                                        float base_spacing = 1.0F) const
      -> std::vector<QVector3D> override;

  [[nodiscard]] auto getType() const -> FormationType override {
    return FormationType::Roman;
  }
};

class BarbarianFormation : public IFormation {
public:
  [[nodiscard]] auto calculatePositions(int unit_count, const QVector3D &center,
                                        float base_spacing = 1.0F) const
      -> std::vector<QVector3D> override;

  [[nodiscard]] auto getType() const -> FormationType override {
    return FormationType::Barbarian;
  }
};

class FormationSystem {
public:
  static auto instance() -> FormationSystem &;

  auto
  getFormationPositions(FormationType type, int unit_count,
                        const QVector3D &center,
                        float base_spacing = 1.0F) -> std::vector<QVector3D>;

  void registerFormation(FormationType type,
                         std::unique_ptr<IFormation> formation);

  auto getFormation(FormationType type) const -> const IFormation *;

private:
  FormationSystem();
  void initializeDefaults();

  std::unordered_map<FormationType, std::unique_ptr<IFormation>> m_formations;
};

} // namespace Game::Systems
