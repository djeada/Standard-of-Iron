#pragma once

#include <cstdint>
#include <string>
#include <string_view>
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

enum class CommanderBarkKind : std::uint8_t {
  Rally,
  Charge,
  FallBack,
};

// A handful of one-line battlefield shouts. Historical cameo commanders carry
// these instead of the full chatter banks in data/commanders/voices, which
// only the playable roster has; nothing plays them as speech.
struct CommanderBarks {
  std::vector<std::string> rally;
  std::vector<std::string> charge;
  std::vector<std::string> fall_back;

  [[nodiscard]] auto
  lines(CommanderBarkKind kind) const -> const std::vector<std::string>&;
  [[nodiscard]] auto empty() const -> bool {
    return rally.empty() && charge.empty() && fall_back.empty();
  }
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
  Game::Units::SpawnType aura_affinity_spawn_type = Game::Units::SpawnType::Swordsman;
  CommanderSignature signature{};
  CommanderDoctrine doctrine{};

  // False for historical cameo commanders: they can be spawned by missions,
  // maps and arena scenarios, but never appear in a commander picker, never
  // need a voice bank and are never handed to a skirmish seat.
  bool playable = true;
  // Cameos keep the body (troop_type) of a playable commander and replace its
  // look with this renderer; empty means "use the troop profile's renderer".
  std::string renderer_id;
  // One-line historical identification, shown in mission briefings.
  std::string historical_note;
  CommanderBarks barks{};
};

// The playable roster: the six commanders a player can pick or meet as a
// skirmish opponent. Every entry has playable == true.
[[nodiscard]] auto
all_commander_definitions() -> const std::vector<CommanderDefinition>&;

// Historical cameo commanders (issue #1522). Every entry has playable == false,
// borrows the troop_type of a playable commander and is addressed by its id.
[[nodiscard]] auto
historical_commander_definitions() -> const std::vector<CommanderDefinition>&;
[[nodiscard]] auto historical_commander_definition(std::string_view commander_id)
    -> const CommanderDefinition*;
[[nodiscard]] auto is_historical_commander_id(std::string_view commander_id) -> bool;
// Looks an id up in both rosters.
[[nodiscard]] auto
find_commander_definition(std::string_view commander_id) -> const CommanderDefinition*;
// The commander body a cameo uses when fielded by an owner of `nation_id`: the
// playable commander of that nation wielding the same weapon (spear, sword or
// bow) as the cameo's own troop_type.
[[nodiscard]] auto
historical_commander_troop_for_nation(const CommanderDefinition& definition,
                                      Game::Systems::NationID nation_id) -> TroopType;
// Gives a spawned commander the identity, aura numbers and look of a cameo.
// Returns false (and leaves the entity untouched) for unknown ids or entities
// that are not commanders.
auto apply_historical_commander(Engine::Core::Entity& entity,
                                std::string_view commander_id) -> bool;
[[nodiscard]] auto
commander_bark_lines(std::string_view commander_id,
                     CommanderBarkKind kind) -> const std::vector<std::string>&;

[[nodiscard]] auto
commander_definition(TroopType troop_type) -> const CommanderDefinition*;
void configure_commander_component(Engine::Core::Entity& entity, TroopType troop_type);
void configure_commander_component(Engine::Core::Entity& entity,
                                   const CommanderDefinition& definition);
// Playable commanders of a nation, the list commander pickers and skirmish
// seats draw from. Cameos (playable == false) are never in it.
[[nodiscard]] auto commander_definitions_for_nation(Game::Systems::NationID nation_id)
    -> std::vector<const CommanderDefinition*>;

} // namespace Game::Units
