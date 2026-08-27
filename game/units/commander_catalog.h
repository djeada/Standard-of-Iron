#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../systems/nation_id.h"
#include "spawn_type.h"
#include "troop_type.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Units {

enum class CommanderSignatureMove : std::uint8_t {
  None = 0,

  BracingThrust,

  ConsularRiposte,

  PointBlankVolley,

  PhalanxSweep,

  HuntingShot,

  EncirclingCut,
};

struct CommanderSignature {
  CommanderSignatureMove move = CommanderSignatureMove::None;
  std::string display_name;

  float cooldown_seconds = 9.0F;
  float damage_multiplier = 1.0F;

  float bonus_reach = 0.0F;

  float stagger_seconds = 0.0F;
  int max_targets = 1;
};

struct CommanderDoctrine {

  std::string ai_strategy;

  std::string ai_posture;
  float aggression = 0.5F;
  float defense = 0.5F;
  float harassment = 0.5F;

  [[nodiscard]] auto is_authored() const -> bool { return !ai_strategy.empty(); }
};

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
  int bodyguard_count = 0;
  float aura_radius = 12.0F;
  float aura_morale_bonus = 5.0F;
  float aura_bonus_value = 0.0F;
  float rally_range = 10.0F;
  float rally_cooldown = 45.0F;
  float rally_morale_restore = 25.0F;
  float death_shock_radius = 14.0F;
  float death_morale_shock = 25.0F;
  float aura_ability_duration = 15.0F;
  float aura_ability_cooldown = 60.0F;
  Game::Units::SpawnType aura_affinity_spawn_type = Game::Units::SpawnType::Knight;
  CommanderSignature signature{};
  CommanderDoctrine doctrine{};
};

[[nodiscard]] auto
all_commander_definitions() -> const std::vector<CommanderDefinition>&;
[[nodiscard]] auto
commander_definition(TroopType troop_type) -> const CommanderDefinition*;
void configure_commander_component(Engine::Core::Entity& entity, TroopType troop_type);
[[nodiscard]] auto commander_definitions_for_nation(Game::Systems::NationID nation_id)
    -> std::vector<const CommanderDefinition*>;

} // namespace Game::Units
