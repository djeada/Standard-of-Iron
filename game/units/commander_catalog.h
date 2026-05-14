#pragma once

#include <string>
#include <vector>

#include "../systems/nation_id.h"
#include "troop_type.h"

namespace Game::Units {

struct CommanderDefinition {
  TroopType troop_type;
  Game::Systems::NationID nation_id;
  std::string id;
  std::string display_name;
  std::string strategic_identity;
  std::string recruitment_effect;
  std::string battlefield_role;
  std::string strengths;
  std::string weaknesses;
  std::string passive_aura;
  std::string bonus_type;
  std::string bonus_summary;
  std::string rally_ability;
  std::string death_consequence;
  std::string visual_requirements;
  int bodyguard_count = 6;
  float aura_radius = 12.0F;
  float aura_morale_bonus = 5.0F;
  float aura_bonus_value = 0.0F;
  float rally_range = 10.0F;
  float rally_cooldown = 45.0F;
  float rally_morale_restore = 25.0F;
  float death_shock_radius = 14.0F;
  float death_morale_shock = 25.0F;
};

[[nodiscard]] auto
all_commander_definitions() -> const std::vector<CommanderDefinition>&;
[[nodiscard]] auto
commander_definition(TroopType troop_type) -> const CommanderDefinition*;
[[nodiscard]] auto commander_definitions_for_nation(Game::Systems::NationID nation_id)
    -> std::vector<const CommanderDefinition*>;

} // namespace Game::Units
