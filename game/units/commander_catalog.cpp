#include "commander_catalog.h"

#include "../core/component.h"
#include "../core/entity.h"

namespace Game::Units {

auto all_commander_definitions() -> const std::vector<CommanderDefinition>& {
  using Game::Systems::NationID;
  static const std::vector<CommanderDefinition> definitions = {
      {TroopType::RomanLegionOrganizer,
       NationID::RomanRepublic,
       "roman_legion_organizer",
       "Quintus Fabius Maximus",
       "Rome's delaying strategist who preserves armies through discipline and "
       "staying power.",
       "A campaign and scenario commander; never produced from a barracks.",
       "Heavy spear commander who anchors a disciplined battle line.",
       "Boosts nearby troop endurance and slows collapse in grind fights.",
       "Lower offensive pressure and weaker pursuit potential.",
       "Fabian Endurance keeps nearby cohorts in line for prolonged fighting.",
       "health_regen",
       "Nearby allied infantry regenerate health while inside the aura.",
       "Rally of Patience stabilizes wavering lines during attritional combat.",
       "If killed or wounded, nearby allies lose confidence and the aura shuts "
       "off.",
       "Unique infantry renderer with long crimson cloak, broad scutum guard, "
       "and reinforced cuirass with silver neck guard.",
       0,
       13.0F,
       7.0F,
       24.0F,
       11.0F,
       42.0F,
       28.0F,
       15.0F,
       24.0F,
       15.0F,
       60.0F,
       SpawnType::Spearman},
      {TroopType::RomanVeteranConsul,
       NationID::RomanRepublic,
       "roman_veteran_consul",
       "Publius Cornelius Scipio",
       "Aggressive Roman consul focused on decisive strikes and tactical "
       "initiative.",
       "A campaign and scenario commander; never produced from a barracks.",
       "Sword-and-shield infantry commander who leads close behind the main "
       "assault line.",
       "Improves nearby offensive output and enables sharper counter-pushes.",
       "More vulnerable if isolated away from supporting infantry.",
       "Consular Assault boosts nearby legion attack tempo.",
       "attack_boost",
       "Nearby allied troops gain bonus melee and ranged damage in aura range.",
       "Consular Rally rapidly restores morale to wavering assault cohorts.",
       "Death of the consul causes a severe morale shock to nearby Romans.",
       "Unique infantry renderer with gilded consul helmet, decorated cuirass, "
       "ornate sword, and a black-crimson command cloak.",
       0,
       14.0F,
       6.0F,
       0.20F,
       12.0F,
       48.0F,
       34.0F,
       16.0F,
       26.0F,
       15.0F,
       55.0F,
       SpawnType::Knight},
      {TroopType::RomanFieldCommander,
       NationID::RomanRepublic,
       "roman_field_commander",
       "Marcus Claudius Marcellus",
       "Roman field commander known for fast shock actions and relentless "
       "pressure.",
       "A campaign and scenario commander; never produced from a barracks.",
       "Lightly armored bow commander who supports aggressive vanguard action.",
       "Raises maneuver speed for nearby troops, enabling faster local "
       "redeployment.",
       "Lower staying power than other Roman commanders.",
       "Vanguard Tempo accelerates nearby infantry movement and line "
       "repositioning.",
       "speed_boost",
       "Nearby allied infantry move faster while in aura range.",
       "Vanguard Rally snaps wavering attackers back into coherent motion.",
       "If Marcellus falls, nearby attackers suffer immediate morale drop.",
       "Unique infantry renderer with wolf-crest helmet, red commander sash, "
       "lighter armor set, and a predatory war bow.",
       0,
       11.0F,
       5.0F,
       0.18F,
       10.0F,
       38.0F,
       24.0F,
       13.0F,
       28.0F,
       12.0F,
       50.0F,
       SpawnType::Archer},
      {TroopType::CarthageMercenaryBroker,
       NationID::Carthage,
       "carthage_mercenary_broker",
       "Hanno the Great",
       "Carthaginian political commander who leverages resources and manpower "
       "flows.",
       "A campaign and scenario commander; never produced from a barracks.",
       "Bronze-spear commander coordinating disciplined mercenary infantry.",
       "Accelerates nearby barracks production through logistical oversight.",
       "Mediocre direct combat impact and risky if exposed.",
       "Contract Logistics boosts nearby production tempo and reserve buildup.",
       "production_haste",
       "Nearby allied barracks train units faster while in aura range.",
       "Treasury Rally restores wavering troops and keeps formations from "
       "breaking.",
       "Death triggers contract panic, reducing confidence in nearby troops.",
       "Unique infantry renderer with merchant-general armor mix, purple-black "
       "mantle, Iberian shield motifs, and bronze command spear.",
       0,
       13.0F,
       6.0F,
       0.35F,
       12.0F,
       44.0F,
       30.0F,
       16.0F,
       32.0F,
       15.0F,
       65.0F,
       SpawnType::Spearman},
      {TroopType::CarthageCavalryPatron,
       NationID::Carthage,
       "carthage_cavalry_patron",
       "Hasdrubal Barca",
       "Mobile Carthaginian field commander built around decisive flanking "
       "momentum.",
       "A campaign and scenario commander; never produced from a barracks.",
       "Fast bow commander built around flanking pressure and withdrawal.",
       "Increases nearby unit speed for coordinated flanks and withdrawals.",
       "Less resilient in prolonged frontal attrition.",
       "Barcid Maneuver grants nearby troops superior movement control.",
       "speed_boost",
       "Nearby allied troops gain movement speed while within aura range.",
       "Flank Rally rapidly restores wavering units preparing to maneuver.",
       "Loss of Hasdrubal sharply drops morale among nearby mobile forces.",
       "Unique infantry renderer with Numidian-influenced helmet plume, "
       "patterned cloak, recurved bow, and dark bronze armor.",
       0,
       15.0F,
       5.0F,
       0.20F,
       13.0F,
       36.0F,
       24.0F,
       14.0F,
       24.0F,
       12.0F,
       50.0F,
       SpawnType::Archer},
      {TroopType::CarthageElephantMaster,
       NationID::Carthage,
       "carthage_elephant_master",
       "Hannibal Barca",
       "Carthage's premier battlefield commander from the Hannibal campaign.",
       "A campaign and scenario commander; never produced from a barracks.",
       "Elite sword commander with an iconic standard and sacred-band armor.",
       "Delivers a strong nearby attack bonus and elite crisis rally response.",
       "High-value target; losing him causes severe local collapse.",
       "Hannibalic Offensive amplifies nearby attack power before decisive "
       "engagements.",
       "attack_boost",
       "Nearby allied troops gain substantial attack damage in aura range.",
       "Supreme Rally restores routing or wavering units to fighting order.",
       "If Hannibal is killed or wounded, nearby allied morale takes a heavy "
       "shock and aura ends.",
       "Unique infantry renderer with black-plumed helmet, lion pelt shoulder, "
       "ornate Iberian falcata, and sacred-band bronze armor accents.",
       0,
       12.0F,
       8.0F,
       0.28F,
       10.0F,
       50.0F,
       36.0F,
       17.0F,
       34.0F,
       18.0F,
       55.0F,
       SpawnType::Knight},
  };
  return definitions;
}

auto commander_definition(TroopType troop_type) -> const CommanderDefinition* {
  for (const auto& definition : all_commander_definitions()) {
    if (definition.troop_type == troop_type) {
      return &definition;
    }
  }
  return nullptr;
}

void configure_commander_component(Engine::Core::Entity& entity, TroopType troop_type) {
  auto const* definition = commander_definition(troop_type);
  if (definition == nullptr) {
    return;
  }
  auto* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr) {
    commander = entity.add_component<Engine::Core::CommanderComponent>();
  }
  if (commander == nullptr) {
    return;
  }
  commander->commander_id = definition->id;
  commander->display_name = definition->display_name;
  commander->strategic_identity = definition->strategic_identity;
  commander->passive_aura = definition->passive_aura;
  commander->bonus_type = definition->bonus_type;
  commander->bonus_summary = definition->bonus_summary;
  commander->rally_ability = definition->rally_ability;
  commander->death_consequence = definition->death_consequence;
  commander->bodyguard_count = 0;
  commander->aura_radius = definition->aura_radius;
  commander->aura_morale_bonus = definition->aura_morale_bonus;
  commander->aura_bonus_value = definition->aura_bonus_value;
  commander->rally_range = definition->rally_range;
  commander->rally_cooldown = definition->rally_cooldown;
  commander->rally_morale_restore = definition->rally_morale_restore;
  commander->rally_requires_manual_trigger = true;
  commander->death_shock_radius = definition->death_shock_radius;
  commander->death_morale_shock = definition->death_morale_shock;
  commander->aura_ability_duration = definition->aura_ability_duration;
  commander->aura_ability_cooldown = definition->aura_ability_cooldown;
  commander->aura_affinity_spawn_type = definition->aura_affinity_spawn_type;
}

auto commander_definitions_for_nation(Game::Systems::NationID nation_id)
    -> std::vector<const CommanderDefinition*> {
  std::vector<const CommanderDefinition*> result;
  for (const auto& definition : all_commander_definitions()) {
    if (definition.nation_id == nation_id) {
      result.push_back(&definition);
    }
  }
  return result;
}

} // namespace Game::Units
