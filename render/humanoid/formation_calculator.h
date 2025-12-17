#pragma once

#include <QVector2D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {

struct FormationOffset {
  float offset_x;
  float offset_z;
};

class IFormationCalculator {
public:
  virtual ~IFormationCalculator() = default;

  [[nodiscard]] virtual auto
  calculateOffset(int idx, int row, int col, int rows, int cols, float spacing,
                  uint32_t seed) const -> FormationOffset = 0;

  [[nodiscard]] virtual auto get_description() const -> const char * = 0;
};

class RomanInfantryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculateOffset(int idx, int row, int col, int rows, int cols, float spacing,
                  uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Roman Infantry (Perfect Grid)";
  }
};

class RomanCavalryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculateOffset(int idx, int row, int col, int rows, int cols, float spacing,
                  uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Roman Cavalry (Wide Grid)";
  }
};

class CarthageInfantryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculateOffset(int idx, int row, int col, int rows, int cols, float spacing,
                  uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Carthage Infantry (Irregular)";
  }
};

class CarthageCavalryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculateOffset(int idx, int row, int col, int rows, int cols, float spacing,
                  uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Carthage Cavalry (Loose/Skirmish)";
  }
};

class BuilderCircleFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculateOffset(int idx, int row, int col, int rows, int cols, float spacing,
                  uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Builder Circle (Construction)";
  }
};

class FormationCalculatorFactory {
public:
  enum class Nation { Roman, Carthage };

  enum class UnitCategory { Infantry, Cavalry, BuilderConstruction };

  static auto getCalculator(Nation nation, UnitCategory category)
      -> const IFormationCalculator *;

private:
  static RomanInfantryFormation s_romanInfantry;
  static RomanCavalryFormation s_romanCavalry;
  static CarthageInfantryFormation s_carthageInfantry;
  static CarthageCavalryFormation s_carthageCavalry;
  static BuilderCircleFormation s_builderCircle;
};

} // namespace Render::GL
