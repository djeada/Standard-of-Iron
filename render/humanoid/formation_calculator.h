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
  calculate_offset(int idx, int row, int col, int rows, int cols, float spacing,
                   uint32_t seed) const -> FormationOffset = 0;

  [[nodiscard]] virtual auto get_description() const -> const char * = 0;
};

class RomanInfantryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculate_offset(int idx, int row, int col, int rows, int cols, float spacing,
                   uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Roman Infantry (Perfect Grid)";
  }
};

class RomanCavalryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculate_offset(int idx, int row, int col, int rows, int cols, float spacing,
                   uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Roman Cavalry (Wide Grid)";
  }
};

class CarthageInfantryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculate_offset(int idx, int row, int col, int rows, int cols, float spacing,
                   uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Carthage Infantry (Irregular)";
  }
};

class CarthageCavalryFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculate_offset(int idx, int row, int col, int rows, int cols, float spacing,
                   uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Carthage Cavalry (Loose/Skirmish)";
  }
};

class BuilderCircleFormation : public IFormationCalculator {
public:
  [[nodiscard]] auto
  calculate_offset(int idx, int row, int col, int rows, int cols, float spacing,
                   uint32_t seed) const -> FormationOffset override;

  [[nodiscard]] auto get_description() const -> const char * override {
    return "Builder Circle (Construction)";
  }
};

class FormationCalculatorFactory {
public:
  enum class Nation { Roman, Carthage };

  enum class UnitCategory { Infantry, Cavalry, BuilderConstruction };

  static auto get_calculator(Nation nation, UnitCategory category)
      -> const IFormationCalculator *;

private:
  static RomanInfantryFormation s_roman_infantry;
  static RomanCavalryFormation s_roman_cavalry;
  static CarthageInfantryFormation s_carthage_infantry;
  static CarthageCavalryFormation s_carthage_cavalry;
  static BuilderCircleFormation s_builder_circle;
};

} // namespace Render::GL
