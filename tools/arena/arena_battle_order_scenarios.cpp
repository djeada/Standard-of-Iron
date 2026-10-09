#include "arena_battle_order_scenarios.h"

#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Intent = Game::Formation::ArmyFormationIntent;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr int k_rome = 1;
constexpr int k_carthage = 2;

auto group(QString name,
           Troop troop,
           Nation nation,
           int owner,
           int count,
           QVector3D origin,
           float pitch,
           float facing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = nation;
  result.owner_id = owner;
  result.count = count;
  result.origin = origin;
  result.spacing = {pitch, 0.0F, 0.0F};
  result.facing_degrees = facing;
  return result;
}

auto at(float time, Command command, QString source) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  return result;
}

auto attack_move(float time, QString source, QString target) -> ArenaScenarioStep {
  auto result = at(time, Command::AttackMove, std::move(source));
  result.name += QStringLiteral("_attack");
  result.target_group = std::move(target);
  return result;
}

auto form(float time,
          QStringList groups,
          Intent intent,
          QVector3D anchor,
          float facing) -> ArenaScenarioStep {
  auto result = at(time, Command::FormArmy, groups.value(0));
  result.name += QStringLiteral("_form");
  result.formation.groups = std::move(groups);
  result.formation.intent = intent;
  result.formation.anchor = anchor;
  result.formation.facing_degrees = facing;
  return result;
}

auto manoeuvre(QString group, QString metric, float threshold) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = Expect::BattleOrderManoeuvreObserved;
  result.group = std::move(group);
  result.counter_key = std::move(metric);
  result.threshold = threshold;
  return result;
}

auto rendered(QString group) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = Expect::GroupIsRendered;
  result.group = std::move(group);
  return result;
}

auto battlefield(QString id,
                 QString label,
                 QString description,
                 float duration) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition s;
  s.id = std::move(id);
  s.label = std::move(label);
  s.description = std::move(description);
  s.duration_seconds = duration;
  // Straight down on the field, high enough to hold both armies.
  s.camera = {175.0F, 78.0F, 0.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, 8.0F);
  s.ground_type = QStringLiteral("grass_dry");
  s.suppress_boundary_mountains = true;
  s.suppress_procedural_props = true;
  s.suppress_terrain_scatter = true;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.select_spawned_units = false;
  s.terrain_grid_extent = 361;
  s.arena_floor_half_extent = 85.0F;
  s.environment.start_time = 14.5F;
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.owner_teams = {{.owner_id = k_rome, .team_id = 1},
                   {.owner_id = k_carthage, .team_id = 2}};
  return s;
}

// Cannae, 216 BC. The consuls deepen their maniples and drive at the centre;
// Hannibal's crescent of Gauls and Iberians bows out to meet them, gives
// ground under the weight, and the Libyan wings that held back wheel in on the
// Roman flanks while the cavalry, having beaten the Roman horse, closes the
// rear.
auto cannae_battle_orders() -> ArenaScenarioDefinition {
  auto s = battlefield(QStringLiteral("battle_order_cannae"),
                       QStringLiteral("Battle Order: Cannae"),
                       QStringLiteral("A deep Roman triplex acies drives into Hannibal's "
                                      "convex crescent. The Gallic and Iberian centre "
                                      "gives ground until the line turns concave; the "
                                      "Libyan wings, which held, wheel in on the Roman "
                                      "flanks and the cavalry closes the rear."),
                       120.0F);
  s.groups = {
      group(QStringLiteral("legion_swords"),
            Troop::Swordsman,
            Nation::RomanRepublic,
            k_rome,
            12,
            {0.0F, 0.0F, 58.0F},
            6.0F,
            180.0F),
      group(QStringLiteral("triarii"),
            Troop::Spearman,
            Nation::RomanRepublic,
            k_rome,
            4,
            {0.0F, 0.0F, 66.0F},
            7.0F,
            180.0F),
      group(QStringLiteral("velites"),
            Troop::Velites,
            Nation::RomanRepublic,
            k_rome,
            4,
            {0.0F, 0.0F, 50.0F},
            9.0F,
            180.0F),
      group(QStringLiteral("equites"),
            Troop::MountedSwordsman,
            Nation::RomanRepublic,
            k_rome,
            4,
            {0.0F, 0.0F, 72.0F},
            8.0F,
            180.0F),
      // Allied contingents first: the owner's nation is the last group's.
      group(QStringLiteral("gallic_centre"),
            Troop::Swordsman,
            Nation::Gauls,
            k_carthage,
            4,
            {-14.0F, 0.0F, -24.0F},
            7.0F,
            0.0F),
      group(QStringLiteral("iberian_centre"),
            Troop::Swordsman,
            Nation::Iberians,
            k_carthage,
            4,
            {14.0F, 0.0F, -24.0F},
            7.0F,
            0.0F),
      group(QStringLiteral("gallic_horse"),
            Troop::MountedSwordsman,
            Nation::Gauls,
            k_carthage,
            3,
            {-50.0F, 0.0F, -30.0F},
            7.0F,
            0.0F),
      group(QStringLiteral("libyans"),
            Troop::Spearman,
            Nation::Carthage,
            k_carthage,
            6,
            {0.0F, 0.0F, -34.0F},
            12.0F,
            0.0F),
      group(QStringLiteral("balearic_slingers"),
            Troop::Slinger,
            Nation::Carthage,
            k_carthage,
            3,
            {0.0F, 0.0F, -16.0F},
            10.0F,
            0.0F),
      group(QStringLiteral("numidians"),
            Troop::HorseSpearman,
            Nation::Carthage,
            k_carthage,
            3,
            {50.0F, 0.0F, -30.0F},
            7.0F,
            0.0F),
  };

  QStringList const legion{QStringLiteral("legion_swords"),
                           QStringLiteral("triarii"),
                           QStringLiteral("velites"),
                           QStringLiteral("equites")};
  QStringList const host{QStringLiteral("gallic_centre"),
                         QStringLiteral("iberian_centre"),
                         QStringLiteral("libyans"),
                         QStringLiteral("balearic_slingers"),
                         QStringLiteral("gallic_horse"),
                         QStringLiteral("numidians")};

  auto crescent = form(0.3F, host, Intent::ConvexCrescent, {0.0F, 0.0F, -22.0F}, 0.0F);
  s.steps.push_back(std::move(crescent));
  // The consuls narrowed the maniples' frontage and deepened them.
  auto deploy = form(0.3F, legion, Intent::TriplexAcies, {0.0F, 0.0F, 50.0F}, 180.0F);
  deploy.formation.options.depth_scale = 1.8F;
  s.steps.push_back(std::move(deploy));

  auto advance = form(16.0F, legion, Intent::TriplexAcies, {0.0F, 0.0F, 16.0F}, 180.0F);
  advance.formation.options.depth_scale = 1.8F;
  advance.formation.options.movement_policy =
      Game::Formation::MovementPolicy::MaintainFormation;
  s.steps.push_back(std::move(advance));

  s.steps.push_back(attack_move(16.0F, QStringLiteral("gallic_horse"), QStringLiteral("equites")));
  s.steps.push_back(attack_move(16.0F, QStringLiteral("numidians"), QStringLiteral("equites")));
  s.steps.push_back(
      attack_move(32.0F, QStringLiteral("legion_swords"), QStringLiteral("gallic_centre")));
  s.steps.push_back(
      attack_move(34.0F, QStringLiteral("triarii"), QStringLiteral("iberian_centre")));

  for (auto const& rider : {QStringLiteral("gallic_horse"), QStringLiteral("numidians")}) {
    ArenaScenarioStep close;
    close.name = rider + QStringLiteral("_closes_the_rear");
    close.trigger = {Trigger::GroupDestroyed, 0.0F, QStringLiteral("equites"), {}, 0.0F};
    close.command = Command::AttackMove;
    close.group = rider;
    close.target_group = QStringLiteral("triarii");
    s.steps.push_back(std::move(close));
  }

  s.battle_sides = {{k_rome, QStringLiteral("Rome"), {0.0F, 0.0F, 50.0F}, 30.0F},
                    {k_carthage, QStringLiteral("Carthage"), {0.0F, 0.0F, -22.0F}, 30.0F}};
  s.expectations = {
      rendered(QStringLiteral("legion_swords")),
      rendered(QStringLiteral("gallic_centre")),
      manoeuvre(QStringLiteral("gallic_centre"), QStringLiteral("centre_yield"), 0.5F),
      manoeuvre(QStringLiteral("libyans"), QStringLiteral("wing_wheel"), 0.3F),
  };
  return s;
}

// Zama, 202 BC. Scipio stands his maniples one behind another so the lanes
// run straight through the army; Hannibal's elephants charge, are funnelled
// down the lanes and run out the back, and the legion then advances.
auto zama_battle_orders() -> ArenaScenarioDefinition {
  auto s = battlefield(QStringLiteral("battle_order_zama"),
                       QStringLiteral("Battle Order: Zama"),
                       QStringLiteral("Hannibal's war elephants charge from their "
                                      "screen; the Roman triplex acies opens straight "
                                      "lanes as they come, the velites fall back, and "
                                      "the beasts run down the lanes and out behind "
                                      "the triarii."),
                       80.0F);
  s.groups = {
      group(QStringLiteral("legion_swords"),
            Troop::Swordsman,
            Nation::RomanRepublic,
            k_rome,
            9,
            {0.0F, 0.0F, 62.0F},
            6.0F,
            180.0F),
      group(QStringLiteral("triarii"),
            Troop::Spearman,
            Nation::RomanRepublic,
            k_rome,
            3,
            {0.0F, 0.0F, 70.0F},
            7.0F,
            180.0F),
      group(QStringLiteral("velites"),
            Troop::Velites,
            Nation::RomanRepublic,
            k_rome,
            4,
            {0.0F, 0.0F, 54.0F},
            9.0F,
            180.0F),
      group(QStringLiteral("equites"),
            Troop::MountedSwordsman,
            Nation::RomanRepublic,
            k_rome,
            2,
            {0.0F, 0.0F, 76.0F},
            8.0F,
            180.0F),
      group(QStringLiteral("elephants"),
            Troop::Elephant,
            Nation::Carthage,
            k_carthage,
            8,
            {0.0F, 0.0F, -38.0F},
            8.0F,
            0.0F),
      group(QStringLiteral("mercenaries"),
            Troop::Swordsman,
            Nation::Carthage,
            k_carthage,
            6,
            {0.0F, 0.0F, -50.0F},
            7.0F,
            0.0F),
      group(QStringLiteral("veterans"),
            Troop::Spearman,
            Nation::Carthage,
            k_carthage,
            4,
            {0.0F, 0.0F, -58.0F},
            8.0F,
            0.0F),
      group(QStringLiteral("punic_horse"),
            Troop::HorseSpearman,
            Nation::Carthage,
            k_carthage,
            2,
            {0.0F, 0.0F, -64.0F},
            8.0F,
            0.0F),
  };
  QStringList const legion{QStringLiteral("legion_swords"),
                           QStringLiteral("triarii"),
                           QStringLiteral("velites"),
                           QStringLiteral("equites")};
  QStringList const host{QStringLiteral("elephants"),
                         QStringLiteral("mercenaries"),
                         QStringLiteral("veterans"),
                         QStringLiteral("punic_horse")};
  s.steps.push_back(form(0.3F, host, Intent::ElephantScreen, {0.0F, 0.0F, -50.0F}, 0.0F));
  s.steps.push_back(form(0.3F, legion, Intent::TriplexAcies, {0.0F, 0.0F, 52.0F}, 180.0F));
  s.steps.push_back(
      attack_move(14.0F, QStringLiteral("elephants"), QStringLiteral("legion_swords")));
  s.steps.push_back(
      attack_move(52.0F, QStringLiteral("legion_swords"), QStringLiteral("mercenaries")));

  s.battle_sides = {{k_rome, QStringLiteral("Rome"), {0.0F, 0.0F, 52.0F}, 30.0F},
                    {k_carthage, QStringLiteral("Carthage"), {0.0F, 0.0F, -50.0F}, 30.0F}};
  s.expectations = {
      rendered(QStringLiteral("legion_swords")),
      rendered(QStringLiteral("elephants")),
      manoeuvre(QStringLiteral("legion_swords"), QStringLiteral("lane_shift"), 0.99F),
      manoeuvre(QStringLiteral("elephants"), QStringLiteral("elephant_lane_runs"), 3.0F),
  };
  return s;
}

} // namespace

auto build_battle_order_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(cannae_battle_orders());
  result.push_back(zama_battle_orders());
  return result;
}

} // namespace Arena::Scenarios
