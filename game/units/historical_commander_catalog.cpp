// Historical cameo commanders (issue #1522): the named consuls and Barcid
// officers of the Second Punic War. They are fielded by missions, maps and
// arena scenarios through their ids, borrow the body and signature move of a
// playable commander, and carry their own look, aura numbers and a few
// one-line barks. They are never playable: see CommanderDefinition::playable.

#include <QtGlobal>

#include <algorithm>
#include <utility>

#include "../core/component_commander.h"
#include "../core/component_core.h"
#include "../core/entity.h"
#include "commander_catalog.h"

namespace Game::Units {
namespace {

using Game::Systems::NationID;

enum class CameoWeapon : std::uint8_t {
  Spear,
  Sword,
  Bow,
};

auto weapon_of(TroopType troop_type) -> CameoWeapon {
  switch (troop_type) {
  case TroopType::RomanLegionOrganizer:
  case TroopType::CarthageSpearCommander:
    return CameoWeapon::Spear;
  case TroopType::RomanFieldCommander:
  case TroopType::CarthageBowCommander:
    return CameoWeapon::Bow;
  default:
    return CameoWeapon::Sword;
  }
}

struct CameoAura {
  const char* bonus_type;
  SpawnType affinity;
  float aura_radius;
  float aura_morale_bonus;
  float aura_bonus_value;
  float rally_range;
  float rally_cooldown;
  float rally_morale_restore;
  float death_shock_radius;
  float death_morale_shock;
  float aura_ability_duration;
  float aura_ability_cooldown;
};

struct CameoText {
  const char* display_name;
  const char* strategic_identity;
  const char* battlefield_role;
  const char* strengths;
  const char* weaknesses;
  const char* passive_aura;
  const char* bonus_summary;
};

struct CameoSpec {
  TroopType body;
  NationID nation;
  const char* id;
  const char* renderer_id;
  CameoText text;
  CameoAura aura;
  const char* visual_requirements;
  const char* rally_bark;
  const char* charge_bark;
  const char* fall_back_bark;
};

auto make_cameo(const CameoSpec& spec) -> CommanderDefinition {
  CommanderDefinition definition;
  if (const auto* body = commander_definition(spec.body)) {
    // Signature move and AI doctrine follow the borrowed body, so an owner
    // fielding a cameo fights and plans exactly like the playable original.
    definition = *body;
  }
  definition.troop_type = spec.body;
  definition.nation_id = spec.nation;
  definition.id = spec.id;
  definition.display_name = spec.text.display_name;
  definition.strategic_identity = spec.text.strategic_identity;
  definition.recruitment_effect = QT_TRANSLATE_NOOP(
      "Commanders",
      "A historical commander fielded by missions and scenarios; not playable and "
      "never produced from a barracks.");
  definition.battlefield_role = spec.text.battlefield_role;
  definition.strengths = spec.text.strengths;
  definition.weaknesses = spec.text.weaknesses;
  definition.passive_aura = spec.text.passive_aura;
  definition.bonus_type = spec.aura.bonus_type;
  definition.bonus_summary = spec.text.bonus_summary;
  definition.rally_ability = QT_TRANSLATE_NOOP(
      "Commanders", "Rallies wavering troops nearby back into fighting order.");
  definition.death_consequence = QT_TRANSLATE_NOOP(
      "Commanders", "If he falls, nearby allies lose heart and his aura ends.");
  definition.visual_requirements = spec.visual_requirements;
  definition.bodyguard_count = 0;
  definition.aura_radius = spec.aura.aura_radius;
  definition.aura_morale_bonus = spec.aura.aura_morale_bonus;
  definition.aura_bonus_value = spec.aura.aura_bonus_value;
  definition.rally_range = spec.aura.rally_range;
  definition.rally_cooldown = spec.aura.rally_cooldown;
  definition.rally_morale_restore = spec.aura.rally_morale_restore;
  definition.death_shock_radius = spec.aura.death_shock_radius;
  definition.death_morale_shock = spec.aura.death_morale_shock;
  definition.aura_ability_duration = spec.aura.aura_ability_duration;
  definition.aura_ability_cooldown = spec.aura.aura_ability_cooldown;
  definition.aura_affinity_spawn_type = spec.aura.affinity;
  definition.playable = false;
  definition.renderer_id = spec.renderer_id;
  definition.barks.rally = {spec.rally_bark};
  definition.barks.charge = {spec.charge_bark};
  definition.barks.fall_back = {spec.fall_back_bark};
  return definition;
}

auto build_historical_roster() -> std::vector<CommanderDefinition> {
  const CameoSpec specs[] = {
      {TroopType::RomanVeteranConsul,
       NationID::RomanRepublic,
       "roman_sempronius_longus",
       "troops/roman/commanders/sempronius_longus",
       {QT_TRANSLATE_NOOP("Commanders", "Tiberius Sempronius Longus"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Consul of 218 BC, eager for battle, who sent his army "
                          "across the freezing Trebia before breakfast."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Consular sword commander who leads the legions from the "
                          "front of the attack."),
        QT_TRANSLATE_NOOP("Commanders", "Drives a fresh assault hard and fast."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Commits early and walks into prepared ground."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Consular Impetus sharpens the attack of nearby legions."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied swordsmen gain the most attack in aura "
                          "range.")},
       {"attack_boost",
        SpawnType::Swordsman,
        13.0F,
        6.0F,
        0.20F,
        11.0F,
        46.0F,
        30.0F,
        15.0F,
        28.0F,
        14.0F,
        58.0F},
       "Roman base helmet with twin tall white side feathers and a red crest knob, "
       "vermilion consular cloak with gold trim, anatomical cuirass, gladius and "
       "scutum.",
       QT_TRANSLATE_NOOP("Commanders", "Hold the line, Romans!"),
       QT_TRANSLATE_NOOP("Commanders", "Across, and at them!"),
       QT_TRANSLATE_NOOP("Commanders", "Back to the river bank!")},
      {TroopType::RomanLegionOrganizer,
       NationID::RomanRepublic,
       "roman_gaius_flaminius",
       "troops/roman/commanders/gaius_flaminius",
       {QT_TRANSLATE_NOOP("Commanders", "Gaius Flaminius"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Popular consul of 217 BC who marched his column into "
                          "Hannibal's ambush at Lake Trasimene and died there."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Spear commander who keeps a marching column moving at "
                          "speed."),
        QT_TRANSLATE_NOOP("Commanders", "Fast on the march and quick to engage."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Scouts nothing; easily caught deployed in "
                          "column."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Forced March hurries nearby troops along the road."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied spearmen move fastest in aura range.")},
       {"speed_boost",
        SpawnType::Spearman,
        12.0F,
        5.0F,
        0.18F,
        10.0F,
        44.0F,
        26.0F,
        14.0F,
        30.0F,
        14.0F,
        55.0F},
       "Roman base helmet with three tall upright black feathers, saffron cloak "
       "with dark bronze trim, mail shirt and a plain hasta.",
       QT_TRANSLATE_NOOP("Commanders", "Close up, form on me!"),
       QT_TRANSLATE_NOOP("Commanders", "Forward, and no halting!"),
       QT_TRANSLATE_NOOP("Commanders", "Back to the road!")},
      {TroopType::RomanVeteranConsul,
       NationID::RomanRepublic,
       "roman_terentius_varro",
       "troops/roman/commanders/terentius_varro",
       {QT_TRANSLATE_NOOP("Commanders", "Gaius Terentius Varro"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Consul of 216 BC who led the largest army Rome had ever "
                          "fielded into the encirclement at Cannae, and survived it."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Sword commander who throws the full weight of the line "
                          "forward."),
        QT_TRANSLATE_NOOP("Commanders", "Numbers, nerve and a crushing first push."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Rash; blind to the flanks once the line is moving."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Weight of Numbers drives nearby legions harder into the "
                          "enemy centre."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied swordsmen gain strong attack in aura "
                          "range.")},
       {"attack_boost",
        SpawnType::Swordsman,
        14.0F,
        5.0F,
        0.22F,
        12.0F,
        48.0F,
        28.0F,
        16.0F,
        26.0F,
        15.0F,
        60.0F},
       "Gilded Roman helmet with a tall purple crest arching forward over the "
       "brow, wide purple cloak with gold trim, heavy cuirass and an ornate sword.",
       QT_TRANSLATE_NOOP("Commanders", "Stand, Rome is watching!"),
       QT_TRANSLATE_NOOP("Commanders", "Push! Push the centre!"),
       QT_TRANSLATE_NOOP("Commanders", "To Venusia, fall back!")},
      {TroopType::RomanLegionOrganizer,
       NationID::RomanRepublic,
       "roman_aemilius_paullus",
       "troops/roman/commanders/aemilius_paullus",
       {QT_TRANSLATE_NOOP("Commanders", "Lucius Aemilius Paullus"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Cautious consul of 216 BC who advised against battle at "
                          "Cannae and died on the field."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Spear commander who steadies the line and refuses to "
                          "break."),
        QT_TRANSLATE_NOOP("Commanders", "Calm under pressure; keeps a wing standing."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Slow to move and bound by a colleague's decisions."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Aristocratic Composure helps nearby troops recover in a "
                          "long fight."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied spearmen regenerate health fastest in aura "
                          "range.")},
       {"health_regen",
        SpawnType::Spearman,
        13.0F,
        7.0F,
        22.0F,
        11.0F,
        42.0F,
        30.0F,
        15.0F,
        32.0F,
        15.0F,
        60.0F},
       "Silvered Roman helmet with a long white horsehair tail falling down the "
       "back, brick-brown cloak with silver trim, anatomical cuirass and a hasta.",
       QT_TRANSLATE_NOOP("Commanders", "Steady! Keep your ranks!"),
       QT_TRANSLATE_NOOP("Commanders", "With me, together!"),
       QT_TRANSLATE_NOOP("Commanders", "Give ground, slowly!")},
      {TroopType::RomanVeteranConsul,
       NationID::RomanRepublic,
       "roman_scipio_consul_218",
       "troops/roman/commanders/scipio_consul_218",
       {QT_TRANSLATE_NOOP("Commanders", "Publius Cornelius Scipio (consul 218 BC)"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Consul of 218 BC and father of Scipio Africanus; wounded "
                          "at the Ticinus and carried from the field by his son."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Sword commander who screens the army's advance with fast "
                          "mounted troops."),
        QT_TRANSLATE_NOOP("Commanders", "Quick to probe, quick to pull a screen back."),
        QT_TRANSLATE_NOOP("Commanders", "Fights from the front and is easily cut off."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Consular Screen quickens nearby riders and skirmishers."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied horsemen move fastest in aura range.")},
       {"speed_boost",
        SpawnType::MountedSwordsman,
        13.0F,
        6.0F,
        0.18F,
        12.0F,
        44.0F,
        28.0F,
        15.0F,
        30.0F,
        14.0F,
        55.0F},
       "Bronze Roman helmet with a stiff black fan crest edged in white, steel-blue "
       "cloak with white trim, heavy cuirass, gladius and scutum.",
       QT_TRANSLATE_NOOP("Commanders", "Rally to the consul!"),
       QT_TRANSLATE_NOOP("Commanders", "Ride them down!"),
       QT_TRANSLATE_NOOP("Commanders", "Back across the Ticinus!")},
      {TroopType::CarthageSwordCommander,
       NationID::Carthage,
       "carthage_mago_barca",
       "troops/carthage/commanders/mago_barca",
       {QT_TRANSLATE_NOOP("Commanders", "Mago Barca"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Hannibal's youngest brother, who sprang the ambush from the "
                          "stream bed at the Trebia and fought in the centre at "
                          "Cannae."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Sword commander who leads a hidden detachment into the "
                          "enemy's rear."),
        QT_TRANSLATE_NOOP("Commanders", "Strikes hard from concealment."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Small command; vulnerable once the ambush is spent."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Ambush Strike lends nearby troops a sharper first blow."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied swordsmen gain strong attack in aura "
                          "range.")},
       {"attack_boost",
        SpawnType::Swordsman,
        12.0F,
        6.0F,
        0.22F,
        10.0F,
        46.0F,
        30.0F,
        15.0F,
        28.0F,
        14.0F,
        55.0F},
       "Carthaginian helmet with a single tall crimson plume, dark green cloak with "
       "gold trim, linen corselet, Punic sword and round shield.",
       QT_TRANSLATE_NOOP("Commanders", "To me, sons of Carthage!"),
       QT_TRANSLATE_NOOP("Commanders", "Out of the reeds, now!"),
       QT_TRANSLATE_NOOP("Commanders", "Back to the stream bed!")},
      {TroopType::CarthageSpearCommander,
       NationID::Carthage,
       "carthage_maharbal",
       "troops/carthage/commanders/maharbal",
       {QT_TRANSLATE_NOOP("Commanders", "Maharbal"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Hannibal's cavalry commander, who rounded up the survivors "
                          "of Trasimene and urged a march on Rome after Cannae."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Spear-armed cavalry commander who hunts broken troops."),
        QT_TRANSLATE_NOOP("Commanders", "Relentless in pursuit."),
        QT_TRANSLATE_NOOP("Commanders", "Light protection; poor at holding ground."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Cavalry Pursuit speeds nearby riders after a breaking "
                          "enemy."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied horsemen move fastest in aura range.")},
       {"speed_boost",
        SpawnType::MountedSwordsman,
        14.0F,
        5.0F,
        0.20F,
        13.0F,
        40.0F,
        26.0F,
        14.0F,
        26.0F,
        12.0F,
        50.0F},
       "Carthaginian helmet with a low white horsehair crest from brow to nape, "
       "teal cloak with cream trim, light armour and a cavalry spear.",
       QT_TRANSLATE_NOOP("Commanders", "Horsemen, to me!"),
       QT_TRANSLATE_NOOP("Commanders", "After them, no quarter!"),
       QT_TRANSLATE_NOOP("Commanders", "Wheel away, reform!")},
      {TroopType::CarthageBowCommander,
       NationID::Carthage,
       "carthage_hanno_bomilcar",
       "troops/carthage/commanders/hanno_bomilcar",
       {QT_TRANSLATE_NOOP("Commanders", "Hanno, son of Bomilcar"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Hannibal's nephew, who crossed the Rhone upstream and "
                          "signalled his flank attack with smoke."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Mobile commander who takes a detachment round the enemy's "
                          "flank."),
        QT_TRANSLATE_NOOP("Commanders", "Fast, independent flanking marches."),
        QT_TRANSLATE_NOOP("Commanders", "Thin in a frontal fight."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Flanking March hurries nearby troops round the enemy."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied archers move fastest in aura range.")},
       {"speed_boost",
        SpawnType::Archer,
        13.0F,
        5.0F,
        0.19F,
        12.0F,
        38.0F,
        24.0F,
        13.0F,
        24.0F,
        12.0F,
        50.0F},
       "Carthaginian helmet with a forward-curling Phrygian peak and a short blue "
       "plume, indigo cloak with silver trim, light armour and a recurved bow.",
       QT_TRANSLATE_NOOP("Commanders", "Hold here, wait for the smoke!"),
       QT_TRANSLATE_NOOP("Commanders", "Now, into their flank!"),
       QT_TRANSLATE_NOOP("Commanders", "Back to the ford!")},
      {TroopType::CarthageSwordCommander,
       NationID::Carthage,
       "carthage_hasdrubal_cavalry",
       "troops/carthage/commanders/hasdrubal_cavalry",
       {QT_TRANSLATE_NOOP("Commanders", "Hasdrubal (cavalry commander)"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Commander of the Celtic and Iberian heavy horse on "
                          "Hannibal's left at Cannae; not Hannibal's brother Hasdrubal "
                          "Barca."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Heavy cavalry commander who breaks a wing and rides into "
                          "the rear."),
        QT_TRANSLATE_NOOP("Commanders", "Shock and discipline in the charge."),
        QT_TRANSLATE_NOOP("Commanders", "Costly to replace if he is cut down."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Heavy Horse Charge adds weight to nearby attacks."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied horsemen gain the most attack in aura "
                          "range.")},
       {"attack_boost",
        SpawnType::MountedSwordsman,
        12.0F,
        6.0F,
        0.24F,
        11.0F,
        48.0F,
        32.0F,
        15.0F,
        28.0F,
        14.0F,
        58.0F},
       "Dark iron Carthaginian helmet with a face guard and two curved white side "
       "plumes, black cloak with bronze trim, gilded scale, falcata and cavalry "
       "shield.",
       QT_TRANSLATE_NOOP("Commanders", "Close the ranks, horsemen!"),
       QT_TRANSLATE_NOOP("Commanders", "Charge! Break their wing!"),
       QT_TRANSLATE_NOOP("Commanders", "Rein in and reform!")},
      {TroopType::RomanLegionOrganizer,
       NationID::RomanRepublic,
       "numidian_masinissa",
       "troops/numidian/commanders/masinissa",
       {QT_TRANSLATE_NOOP("Commanders", "Masinissa"),
        QT_TRANSLATE_NOOP("Commanders",
                          "Numidian prince who fought for Carthage in Spain, changed "
                          "sides, and led Rome's allied horse at Zama."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Light cavalry commander who harries with javelins and "
                          "never stands still."),
        QT_TRANSLATE_NOOP("Commanders", "The fastest horse in Africa."),
        QT_TRANSLATE_NOOP("Commanders", "Unarmoured; no use in a static fight."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Numidian Horse keeps nearby riders moving and wheeling."),
        QT_TRANSLATE_NOOP("Commanders",
                          "Nearby allied horsemen move fastest in aura range.")},
       {"speed_boost",
        SpawnType::MountedSwordsman,
        15.0F,
        5.0F,
        0.20F,
        13.0F,
        36.0F,
        24.0F,
        14.0F,
        24.0F,
        12.0F,
        50.0F},
       "Bareheaded with dark curled hair under a white royal diadem studded with "
       "gold, full black beard, short white cloak with crimson trim over a white "
       "tunic, no armour, and a light javelin.",
       QT_TRANSLATE_NOOP("Commanders", "Riders, to me!"),
       QT_TRANSLATE_NOOP("Commanders", "Throw, and wheel!"),
       QT_TRANSLATE_NOOP("Commanders", "Scatter and come again!")},
  };

  std::vector<CommanderDefinition> roster;
  roster.reserve(std::size(specs));
  for (const auto& spec : specs) {
    roster.push_back(make_cameo(spec));
  }
  return roster;
}

auto empty_lines() -> const std::vector<std::string>& {
  static const std::vector<std::string> k_empty;
  return k_empty;
}

} // namespace

auto CommanderBarks::lines(CommanderBarkKind kind) const
    -> const std::vector<std::string>& {
  switch (kind) {
  case CommanderBarkKind::Rally:
    return rally;
  case CommanderBarkKind::Charge:
    return charge;
  case CommanderBarkKind::FallBack:
    return fall_back;
  }
  return empty_lines();
}

auto historical_commander_definitions() -> const std::vector<CommanderDefinition>& {
  static const std::vector<CommanderDefinition> roster = build_historical_roster();
  return roster;
}

auto historical_commander_definition(std::string_view commander_id)
    -> const CommanderDefinition* {
  if (commander_id.empty()) {
    return nullptr;
  }
  for (const auto& definition : historical_commander_definitions()) {
    if (definition.id == commander_id) {
      return &definition;
    }
  }
  return nullptr;
}

auto is_historical_commander_id(std::string_view commander_id) -> bool {
  return historical_commander_definition(commander_id) != nullptr;
}

auto find_commander_definition(std::string_view commander_id)
    -> const CommanderDefinition* {
  for (const auto& definition : all_commander_definitions()) {
    if (definition.id == commander_id) {
      return &definition;
    }
  }
  return historical_commander_definition(commander_id);
}

auto historical_commander_troop_for_nation(const CommanderDefinition& definition,
                                           NationID nation_id) -> TroopType {
  const bool carthage = nation_id == NationID::Carthage;
  switch (weapon_of(definition.troop_type)) {
  case CameoWeapon::Spear:
    return carthage ? TroopType::CarthageSpearCommander
                    : TroopType::RomanLegionOrganizer;
  case CameoWeapon::Bow:
    return carthage ? TroopType::CarthageBowCommander : TroopType::RomanFieldCommander;
  case CameoWeapon::Sword:
    return carthage ? TroopType::CarthageSwordCommander : TroopType::RomanVeteranConsul;
  }
  return definition.troop_type;
}

auto apply_historical_commander(Engine::Core::Entity& entity,
                                std::string_view commander_id) -> bool {
  const auto* definition = historical_commander_definition(commander_id);
  if (definition == nullptr ||
      entity.get_component<Engine::Core::CommanderComponent>() == nullptr) {
    return false;
  }
  configure_commander_component(entity, *definition);
  if (!definition->renderer_id.empty()) {
    if (auto* renderable = entity.get_component<Engine::Core::RenderableComponent>()) {
      renderable->renderer_id = definition->renderer_id;
    }
  }
  return true;
}

auto commander_bark_lines(std::string_view commander_id,
                          CommanderBarkKind kind) -> const std::vector<std::string>& {
  if (const auto* definition = find_commander_definition(commander_id)) {
    return definition->barks.lines(kind);
  }
  return empty_lines();
}

} // namespace Game::Units
