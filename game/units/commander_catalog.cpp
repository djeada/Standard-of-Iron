#include "commander_catalog.h"

namespace Game::Units {

auto all_commander_definitions() -> const std::vector<CommanderDefinition> & {
  using Game::Systems::NationID;
  static const std::vector<CommanderDefinition> definitions = {
      {TroopType::RomanLegionOrganizer,
       NationID::RomanRepublic,
       "roman_legion_organizer",
       "Legion Organizer",
       "Disciplined reformer who turns Rome toward dense, reliable infantry.",
       "Long barracks project that consumes manpower otherwise used for a full "
       "line unit.",
       "Stays behind the front with an elite standard-bearing guard.",
       "Improves nearby legion cohesion and keeps formations steady.",
       "Slow, infantry-focused, and vulnerable if isolated from the line.",
       "Order of the Eagle: nearby infantry recover discipline faster.",
       "Rally the Standards reforms wavering troops around the commander.",
       "If killed or wounded, nearby Romans suffer a discipline shock and the "
       "aura ends.",
       "Tall eagle standard, red cloak, bronze cuirass, rectangular scutum "
       "bodyguards, gold HUD wreath icon, and double-width selection marker.",
       8,
       13.0F,
       7.0F,
       11.0F,
       42.0F,
       28.0F,
       15.0F,
       24.0F},
      {TroopType::RomanVeteranConsul,
       NationID::RomanRepublic,
       "roman_veteran_consul",
       "Veteran Consul",
       "Cautious senior commander built around morale endurance.",
       "Expensive veteran staff competes with elite infantry recruitment.",
       "Measured command unit that anchors reserves and stabilizes crises.",
       "Excellent rally power and resilient bodyguard.",
       "High cost, modest speed, and limited offensive pressure.",
       "Consular Resolve: nearby troops resist wavering.",
       "Consular Rally restores morale to shaken units in a broad radius.",
       "Loss removes the morale anchor and shocks nearby units.",
       "White-plumed Attic helmet, purple-trimmed cloak, consul portrait, "
       "laurel UI icon, veteran bodyguards with polished shields.",
       10,
       14.0F,
       6.0F,
       12.0F,
       48.0F,
       34.0F,
       16.0F,
       26.0F},
      {TroopType::RomanFieldCommander,
       NationID::RomanRepublic,
       "roman_field_commander",
       "Aggressive Field Commander",
       "Forward officer who favors attacks, pursuit, and decisive movement.",
       "Fastest Roman commander but still a long, high-value barracks build.",
       "Moves near the first line with a compact guard and clear red banner.",
       "Boosts nearby attack confidence and can save a wavering push.",
       "Riskier positioning and weaker defensive aura.",
       "Forward Momentum: nearby troops gain morale while advancing.",
       "Battlefield Rally snaps wavering attackers back into formation.",
       "A forward death causes a sharp morale shock among nearby attackers.",
       "Crested helmet, red horsehair plume, short command banner, dark red "
       "portrait backing, compact praetorian guard.",
       6,
       11.0F,
       5.0F,
       10.0F,
       38.0F,
       24.0F,
       13.0F,
       28.0F},
      {TroopType::CarthageMercenaryBroker,
       NationID::Carthage,
       "carthage_mercenary_broker",
       "Mercenary Broker",
       "Wealthy fixer who makes Carthage flexible but loyalty-sensitive.",
       "Very costly commander representing contracts, interpreters, and pay.",
       "Protected civilian-military staff unit kept behind mixed troops.",
       "Strong rally for diverse nearby units and efficient crisis recovery.",
       "Low direct combat value and severe morale shock if lost.",
       "Silver Contracts: nearby troops recover morale while paid and ordered.",
       "Paid Rally steadies wavering mercenaries with visible coin-standard "
       "signals.",
       "Death or capture implies broken contracts, causing heavy local shock.",
       "Purple sash, coin standard, mixed Libyan-Iberian bodyguard, bronze "
       "merchant portrait icon, purple-gold selection marker.",
       7,
       13.0F,
       6.0F,
       12.0F,
       44.0F,
       30.0F,
       16.0F,
       32.0F},
      {TroopType::CarthageCavalryPatron,
       NationID::Carthage,
       "carthage_cavalry_patron",
       "Cavalry Patron",
       "Aristocratic sponsor built around mobile Numidian and citizen cavalry.",
       "Long build and high cost that delays normal mounted production.",
       "Mobile commander with light mounted guards and a tall horsehair pennant.",
       "Best aura radius for mobile support and rapid rally timing.",
       "Less useful around static infantry and fragile in prolonged melee.",
       "Numidian Patronage: nearby mobile troops keep morale while maneuvering.",
       "Cavalry Rally quickly reforms wavering units near the commander.",
       "If wounded, the mobility aura shuts down and nearby cavalry lose heart.",
       "Blue-purple cloak, mounted silhouette, crescent horse standard, "
       "Numidian guard, horse-head HUD icon.",
       6,
       15.0F,
       5.0F,
       13.0F,
       36.0F,
       24.0F,
       14.0F,
       24.0F},
      {TroopType::CarthageElephantMaster,
       NationID::Carthage,
       "carthage_elephant_master",
       "Elephant Master",
       "Dangerous specialist who coordinates elephants and shock troops.",
       "Major investment with high risk, high battlefield presence, and long "
       "training.",
       "Slow command unit surrounded by mahouts, handlers, and heavy guards.",
       "Powerful local morale support for shock assaults.",
       "Slow, obvious, and disastrous to lose near committed troops.",
       "Tower Command: nearby troops hold morale around elephant-led assaults.",
       "Handler Rally reforms wavering troops with drums and elephant standards.",
       "A wound panics nearby confidence and disables elephant command aura.",
       "Elephant-head standard, dark purple armor cloth, handler bodyguards, "
       "large tusk HUD icon, oversized gold-purple selection marker.",
       9,
       12.0F,
       8.0F,
       10.0F,
       50.0F,
       36.0F,
       17.0F,
       34.0F},
  };
  return definitions;
}

auto commander_definition(TroopType troop_type) -> const CommanderDefinition * {
  for (const auto &definition : all_commander_definitions()) {
    if (definition.troop_type == troop_type) {
      return &definition;
    }
  }
  return nullptr;
}

auto commander_definitions_for_nation(Game::Systems::NationID nation_id)
    -> std::vector<const CommanderDefinition *> {
  std::vector<const CommanderDefinition *> result;
  for (const auto &definition : all_commander_definitions()) {
    if (definition.nation_id == nation_id) {
      result.push_back(&definition);
    }
  }
  return result;
}

} // namespace Game::Units
