#include "commander_catalog.h"

#include <QtGlobal>

#include "../core/component_commander.h"
#include "../core/entity.h"

namespace Game::Units {

auto all_commander_definitions() -> const std::vector<CommanderDefinition>& {
  using Game::Systems::NationID;
  static const std::vector<CommanderDefinition> definitions = {
      {TroopType::RomanLegionOrganizer,
       NationID::RomanRepublic,
       "roman_legion_organizer",
       QT_TRANSLATE_NOOP("Commanders", "Quintus Fabius Maximus"),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Rome's delaying strategist who preserves armies through discipline and "
           "staying power."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "A campaign and scenario commander; never produced from a barracks."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Heavy spear commander who anchors a disciplined battle line."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Boosts nearby troop endurance and slows collapse in grind fights."),
       QT_TRANSLATE_NOOP("Commanders",
                         "Lower offensive pressure and weaker pursuit potential."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Fabian Endurance keeps nearby cohorts in line for prolonged fighting."),
       "health_regen",
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Nearby allied spearmen regenerate health fastest inside the aura."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Rally of Patience stabilizes wavering lines during attritional combat."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "If killed or wounded, nearby allies lose confidence and the aura shuts "
           "off."),
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
       SpawnType::Spearman,
       {CommanderSignatureMove::BracingThrust,
        QT_TRANSLATE_NOOP("Commanders", "Bracing Thrust"),
        8.5F,
        1.35F,
        0.55F,
        0.85F,
        1},
       {"defensive", "garrison", 0.25F, 0.90F, 0.20F}},
      {TroopType::RomanVeteranConsul,
       NationID::RomanRepublic,
       "roman_veteran_consul",
       QT_TRANSLATE_NOOP("Commanders", "Publius Cornelius Scipio"),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Aggressive Roman consul focused on decisive strikes and tactical "
           "initiative."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "A campaign and scenario commander; never produced from a barracks."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Sword-and-shield infantry commander who leads close behind the main "
           "assault line."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Improves nearby offensive output and enables sharper counter-pushes."),
       QT_TRANSLATE_NOOP("Commanders",
                         "More vulnerable if isolated away from supporting infantry."),
       QT_TRANSLATE_NOOP("Commanders",
                         "Consular Assault boosts nearby legion attack tempo."),
       "attack_boost",
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Nearby allied swordsmen gain the most bonus damage in aura range."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Consular Rally rapidly restores morale to wavering assault cohorts."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Death of the consul causes a severe morale shock to nearby Romans."),
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
       SpawnType::Knight,
       {CommanderSignatureMove::ConsularRiposte,
        QT_TRANSLATE_NOOP("Commanders", "Consular Riposte"),
        7.0F,
        1.75F,
        0.0F,
        0.35F,
        1},
       {"aggressive", "field", 0.80F, 0.40F, 0.30F}},
      {TroopType::RomanFieldCommander,
       NationID::RomanRepublic,
       "roman_field_commander",
       QT_TRANSLATE_NOOP("Commanders", "Marcus Claudius Marcellus"),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Roman field commander known for fast shock actions and relentless "
           "pressure."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "A campaign and scenario commander; never produced from a barracks."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Lightly armored bow commander who supports aggressive vanguard action."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Raises maneuver speed for nearby troops, enabling faster local "
           "redeployment."),
       QT_TRANSLATE_NOOP("Commanders",
                         "Lower staying power than other Roman commanders."),
       QT_TRANSLATE_NOOP("Commanders",
                         "Vanguard Tempo accelerates nearby infantry movement and line "
                         "repositioning."),
       "speed_boost",
       QT_TRANSLATE_NOOP("Commanders",
                         "Nearby allied archers move fastest while in aura range."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Vanguard Rally snaps wavering attackers back into coherent motion."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "If Marcellus falls, nearby attackers suffer immediate morale drop."),
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
       SpawnType::Archer,
       {CommanderSignatureMove::PointBlankVolley,
        QT_TRANSLATE_NOOP("Commanders", "Point-blank Volley"),
        6.0F,
        1.5F,
        0.0F,
        0.5F,
        1},
       {"rusher", "field", 0.90F, 0.25F, 0.45F}},
      {TroopType::CarthageSpearCommander,
       NationID::Carthage,
       "carthage_spear_commander",
       QT_TRANSLATE_NOOP("Commanders", "Hanno the Great"),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Carthaginian phalanx commander who holds ground with hired spear "
           "levies."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "A campaign and scenario commander; never produced from a barracks."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Bronze-spear commander coordinating disciplined mercenary infantry."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Keeps a braced spear line standing far longer than it should."),
       QT_TRANSLATE_NOOP("Commanders",
                         "Mediocre direct combat impact and risky if exposed."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Contract Discipline knits nearby spear levies into an unbroken hedge."),
       "health_regen",
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Nearby allied spearmen recover health fastest inside the aura."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Treasury Rally restores wavering troops and keeps formations from "
           "breaking."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Death triggers contract panic, reducing confidence in nearby troops."),
       "Unique infantry renderer with merchant-general armor mix, purple-black "
       "mantle, Iberian shield motifs, and bronze command spear.",
       0,
       13.0F,
       6.0F,
       22.0F,
       12.0F,
       44.0F,
       30.0F,
       16.0F,
       32.0F,
       15.0F,
       65.0F,
       SpawnType::Spearman,
       {CommanderSignatureMove::PhalanxSweep,
        QT_TRANSLATE_NOOP("Commanders", "Phalanx Sweep"),
        9.5F,
        1.15F,
        0.75F,
        0.6F,
        4},
       {"economic", "garrison", 0.35F, 0.75F, 0.20F}},
      {TroopType::CarthageBowCommander,
       NationID::Carthage,
       "carthage_bow_commander",
       QT_TRANSLATE_NOOP("Commanders", "Hasdrubal Barca"),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Mobile Carthaginian field commander built around decisive flanking "
           "momentum."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "A campaign and scenario commander; never produced from a barracks."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Fast bow commander built around flanking pressure and withdrawal."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Increases nearby unit speed for coordinated flanks and withdrawals."),
       QT_TRANSLATE_NOOP("Commanders",
                         "Less resilient in prolonged frontal attrition."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Barcid Maneuver grants nearby troops superior movement control."),
       "speed_boost",
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Nearby allied archers gain the most movement speed in aura range."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Flank Rally rapidly restores wavering units preparing to maneuver."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Loss of Hasdrubal sharply drops morale among nearby mobile forces."),
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
       SpawnType::Archer,
       {CommanderSignatureMove::HuntingShot,
        QT_TRANSLATE_NOOP("Commanders", "Hunting Shot"),
        5.5F,
        1.9F,
        0.0F,
        0.0F,
        1},
       {"harasser", "field", 0.55F, 0.30F, 0.95F}},
      {TroopType::CarthageSwordCommander,
       NationID::Carthage,
       "carthage_sword_commander",
       QT_TRANSLATE_NOOP("Commanders", "Hannibal Barca"),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Carthage's premier battlefield commander from the Hannibal campaign."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "A campaign and scenario commander; never produced from a barracks."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Elite sword commander with an iconic standard and sacred-band armor."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Delivers a strong nearby attack bonus and elite crisis rally response."),
       QT_TRANSLATE_NOOP("Commanders",
                         "High-value target; losing him causes severe local collapse."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Hannibalic Offensive amplifies nearby attack power before decisive "
           "engagements."),
       "attack_boost",
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Nearby allied swordsmen gain substantial attack damage in aura range."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "Supreme Rally restores routing or wavering units to fighting order."),
       QT_TRANSLATE_NOOP(
           "Commanders",
           "If Hannibal is killed or wounded, nearby allied morale takes a heavy "
           "shock and aura ends."),
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
       SpawnType::Knight,
       {CommanderSignatureMove::EncirclingCut,
        QT_TRANSLATE_NOOP("Commanders", "Encircling Cut"),
        6.5F,
        1.45F,
        0.25F,
        0.45F,
        2},
       {"aggressive", "field", 0.85F, 0.50F, 0.60F}},
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
  commander->signature_move = static_cast<std::uint8_t>(definition->signature.move);
  commander->signature_name = definition->signature.display_name;
  commander->signature_cooldown = definition->signature.cooldown_seconds;

  commander->signature_cooldown_remaining =
      definition->signature.cooldown_seconds * 0.5F;
  commander->signature_damage_multiplier = definition->signature.damage_multiplier;
  commander->signature_bonus_reach = definition->signature.bonus_reach;
  commander->signature_stagger_seconds = definition->signature.stagger_seconds;
  commander->signature_max_targets = definition->signature.max_targets;
  commander->signature_strike_active = false;
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
