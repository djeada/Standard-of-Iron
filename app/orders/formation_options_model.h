#pragma once

#include <QString>

#include "game/formation/army_formation_types.h"

namespace App::Controllers {

struct FormationPresetIndices {
  int frontage = 0;
  int depth = 0;
  int spacing = 0;
  int flank = 0;
  int ranged = 0;
  int reserve = 0;
  int mixed = 0;
  int preserve = 0;
};

class FormationOptionsModel {
public:
  [[nodiscard]] auto intent() const -> Game::Formation::ArmyFormationIntent {
    return m_intent;
  }
  [[nodiscard]] auto options() const -> const Game::Formation::ArmyFormationOptions& {
    return m_options;
  }
  [[nodiscard]] auto
  doctrine_override() const -> const Game::Formation::FormationDoctrineId& {
    return m_doctrine_override;
  }

  [[nodiscard]] auto set_intent(const QString& intent_id) -> bool;
  [[nodiscard]] auto set_flank_preference(const QString& preference) -> bool;
  [[nodiscard]] auto set_ranged_placement(const QString& placement) -> bool;
  [[nodiscard]] auto set_movement_policy(const QString& policy) -> bool;
  [[nodiscard]] auto set_mixed_policy(const QString& policy) -> bool;

  void adjust_depth(float wheel_delta);
  void set_preserve_order(bool preserve);
  void set_frontage_preset(const QString& preset);
  void set_depth_preset(const QString& preset);
  void set_spacing_preset(const QString& preset);
  void set_reserve_rows(int rows);
  void set_doctrine_override(const QString& doctrine);
  void reset();

  [[nodiscard]] auto preset_indices() const -> FormationPresetIndices;
  [[nodiscard]] auto movement_from_doctrine() const -> bool;
  [[nodiscard]] auto
  movement_index(Game::Formation::MovementPolicy effective) const -> int;

private:
  Game::Formation::ArmyFormationIntent m_intent =
      Game::Formation::ArmyFormationIntent::FactionDefault;
  Game::Formation::ArmyFormationOptions m_options;
  Game::Formation::FormationDoctrineId m_doctrine_override;
};

} // namespace App::Controllers
