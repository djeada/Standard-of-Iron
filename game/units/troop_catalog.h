#pragma once

#include "troop_type.h"
#include <string>
#include <unordered_map>

namespace Game::Units {

struct TroopCombatStats {
  int health = 100;
  int max_health = 100;
  float speed = 1.0F;
  float vision_range = 12.0F;

  float ranged_range = 2.0F;
  int ranged_damage = 10;
  float ranged_cooldown = 1.0F;

  float melee_range = 1.5F;
  int melee_damage = 10;
  float melee_cooldown = 1.0F;

  bool can_ranged = false;
  bool can_melee = true;
};

struct TroopProductionStats {
  int cost = 100;
  float build_time = 4.0F;
  int priority = 0;
  bool is_melee = true;
};

struct TroopVisualStats {
  float render_scale = 1.0F;
  float selection_ring_size = 0.5F;
  float selection_ring_y_offset = 0.0F;
  float selection_ring_ground_offset = 0.0F;
  std::string renderer_id;
};

struct TroopClass {
  Game::Units::TroopType unit_type;
  std::string display_name;

  TroopProductionStats production;
  TroopCombatStats combat;
  TroopVisualStats visuals;

  int individuals_per_unit = 1;
  int max_units_per_row = 1;
};

class TroopCatalog {
public:
  static auto instance() -> TroopCatalog &;

  void register_class(TroopClass troop_class);

  [[nodiscard]] auto
  get_class(Game::Units::TroopType type) const -> const TroopClass *;

  [[nodiscard]] auto get_class_or_fallback(Game::Units::TroopType type) const
      -> const TroopClass &;

  [[nodiscard]] auto get_all_classes() const
      -> const std::unordered_map<Game::Units::TroopType, TroopClass> & {
    return m_classes;
  }

  void clear();

private:
  TroopCatalog();

  void register_defaults();

  std::unordered_map<Game::Units::TroopType, TroopClass> m_classes;
  TroopClass m_fallback{};
};

} // namespace Game::Units
