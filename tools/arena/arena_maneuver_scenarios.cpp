#include "arena_maneuver_scenarios.h"

#include <QString>
#include <QStringList>
#include <QVector3D>

#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/formation/army_formation_types.h"
#include "game/systems/nation_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Intent = Game::Formation::ArmyFormationIntent;
using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr float k_response_seconds = 1.0F;

constexpr float k_stall_budget_seconds = 8.0F;

constexpr float k_walkable_ground_budget = 24.0F;

auto definition(QString id,
                QString label,
                QString description,
                float duration,
                ArenaCameraView camera) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = std::move(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = camera;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_boundary_mountains = true;
  result.suppress_terrain_scatter = false;
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  return result;
}

auto group(QString name,
           Troop troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals,
           QVector3D spacing = QVector3D(0.0F, 0.0F, 3.6F)) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = owner == 1 ? 90.0F : 270.0F;
  return result;
}

auto building(QString name,
              Spawn type,
              int owner,
              int count,
              QVector3D origin,
              QVector3D spacing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = 0.0F;
  return result;
}

auto expectation(Expect kind,
                 QString source = {},
                 float threshold = 0.0F,
                 float distance = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.threshold = threshold;
  result.distance = distance;
  return result;
}

auto patch_of(const char* prop_type,
              int count,
              QVector3D origin,
              QVector3D spacing,
              float scale) -> ArenaScenarioResourcePatch {
  ArenaScenarioResourcePatch patch;
  patch.prop_type = QString::fromLatin1(prop_type);
  patch.count = count;
  patch.origin = origin;
  patch.spacing = spacing;
  patch.scale = scale;
  patch.exact = true;
  return patch;
}

auto move_to(float time,
             const QString& group_name,
             QVector3D destination,
             const QString& order_name) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("%1_%2").arg(order_name, group_name);
  step.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  step.command = Command::FormationMove;
  step.group = group_name;
  step.destination = destination;
  return step;
}

auto deploy_army(float time,
                 const QStringList& groups,
                 QVector3D anchor,
                 float facing_degrees,
                 Intent intent,
                 const QString& order_name) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = order_name;
  step.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  step.command = Command::FormArmy;
  step.group = groups.first();
  step.formation.groups = groups;
  step.formation.anchor = anchor;
  step.formation.facing_degrees = facing_degrees;
  step.formation.intent = intent;
  return step;
}

void order_army(ArenaScenarioDefinition& scenario,
                float time,
                const QStringList& groups,
                QVector3D destination,
                const QString& order_name,
                float lateral_spread = 6.0F) {

  int const count = groups.size();
  for (int index = 0; index < count; ++index) {
    float const offset =
        (static_cast<float>(index) - (count - 1) * 0.5F) * lateral_spread;
    scenario.steps.push_back(move_to(time,
                                     groups.at(index),
                                     destination + QVector3D(0.0F, 0.0F, offset),
                                     order_name));
  }
}

void expect_a_responsive_group(ArenaScenarioDefinition& scenario,
                               const QString& group_name) {
  scenario.expectations.push_back(
      expectation(Expect::AllGroupsRespondWithin, group_name, k_response_seconds));
  scenario.expectations.push_back(expectation(Expect::NoRootTeleport, group_name));
  scenario.expectations.push_back(expectation(Expect::GroupIsRendered, group_name));
  scenario.expectations.push_back(
      expectation(Expect::MovementAnimationObserved, group_name));
  scenario.expectations.push_back(
      expectation(Expect::NoPermanentStall, group_name, k_stall_budget_seconds));
  scenario.expectations.push_back(
      expectation(Expect::UnitsClearOfBuildings, group_name));
  scenario.expectations.push_back(expectation(
      Expect::SoldiersStayOnWalkableGround, group_name, k_walkable_ground_budget));
}

void expect_arrival(ArenaScenarioDefinition& scenario,
                    const QString& group_name,
                    QVector3D destination,
                    float tolerance) {
  auto arrived = expectation(Expect::GroupReachedDestination, group_name);
  arrived.distance = tolerance;
  arrived.position = destination;
  scenario.expectations.push_back(std::move(arrived));
}

void build_staggered_town(ArenaScenarioDefinition& scenario) {
  constexpr float k_house_step = 10.0F;
  scenario.groups.push_back(building(QStringLiteral("row_a"),
                                     Spawn::Home,
                                     1,
                                     5,
                                     QVector3D(-25.0F, 0.0F, 12.0F),
                                     QVector3D(k_house_step, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_b_west"),
                                     Spawn::Home,
                                     1,
                                     2,
                                     QVector3D(-20.0F, 0.0F, 4.0F),
                                     QVector3D(k_house_step, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_b_east"),
                                     Spawn::Home,
                                     1,
                                     1,
                                     QVector3D(10.0F, 0.0F, 4.0F),
                                     QVector3D(k_house_step, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_c_west"),
                                     Spawn::Home,
                                     1,
                                     2,
                                     QVector3D(-25.0F, 0.0F, -4.0F),
                                     QVector3D(k_house_step, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_c_east"),
                                     Spawn::Home,
                                     1,
                                     1,
                                     QVector3D(5.0F, 0.0F, -4.0F),
                                     QVector3D(k_house_step, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_d"),
                                     Spawn::Home,
                                     1,
                                     5,
                                     QVector3D(-20.0F, 0.0F, -12.0F),
                                     QVector3D(k_house_step, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("market"),
                                     Spawn::Marketplace,
                                     1,
                                     1,
                                     QVector3D(26.0F, 0.0F, 6.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("north_barracks"),
                                     Spawn::Barracks,
                                     1,
                                     1,
                                     QVector3D(-6.0F, 0.0F, 26.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("south_barracks"),
                                     Spawn::Barracks,
                                     1,
                                     1,
                                     QVector3D(14.0F, 0.0F, -26.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.resource_patches = {
      patch_of("supply_cart",
               2,
               QVector3D(-30.0F, 0.0F, 8.0F),
               QVector3D(0.0F, 0.0F, -16.0F),
               1.0F),
      patch_of("weapon_rack",
               3,
               QVector3D(3.0F, 0.0F, -8.0F),
               QVector3D(3.0F, 0.0F, 0.0F),
               1.0F),
      patch_of("supply_cart",
               2,
               QVector3D(20.0F, 0.0F, 0.0F),
               QVector3D(0.0F, 0.0F, 8.0F),
               1.0F),
      patch_of("boulder",
               4,
               QVector3D(-34.0F, 0.0F, -20.0F),
               QVector3D(6.0F, 0.0F, 2.0F),
               1.1F),
      patch_of("boulder",
               3,
               QVector3D(30.0F, 0.0F, 20.0F),
               QVector3D(5.0F, 0.0F, -3.0F),
               1.2F),
      patch_of("ruins",
               2,
               QVector3D(-38.0F, 0.0F, 22.0F),
               QVector3D(7.0F, 0.0F, 0.0F),
               1.0F),
      patch_of("pine_tree",
               4,
               QVector3D(36.0F, 0.0F, -12.0F),
               QVector3D(3.0F, 0.0F, -3.0F),
               1.0F),
  };
}

auto add_roman_army(ArenaScenarioDefinition& scenario, QVector3D west) -> QStringList {

  const QString swords = QStringLiteral("swords");
  const QString spears = QStringLiteral("spears");
  const QString archers = QStringLiteral("archers");
  const QString horse = QStringLiteral("horse");
  QVector3D const across(0.0F, 0.0F, 4.2F);
  scenario.groups.push_back(group(
      swords, Troop::Swordsman, 1, 5, west + QVector3D(4.0F, 0.0F, 0.0F), 12, across));
  scenario.groups.push_back(group(
      spears, Troop::Spearman, 1, 4, west + QVector3D(-2.0F, 0.0F, 0.0F), 12, across));
  scenario.groups.push_back(group(
      archers, Troop::Archer, 1, 3, west + QVector3D(-8.0F, 0.0F, 0.0F), 10, across));
  scenario.groups.push_back(group(horse,
                                  Troop::MountedKnight,
                                  1,
                                  2,
                                  west + QVector3D(-14.0F, 0.0F, 0.0F),
                                  6,
                                  across));
  return {swords, spears, archers, horse};
}

auto town_relay_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("maneuver_town_relay"),
      QStringLiteral("Manoeuvre: An Army Relayed Through A Town"),
      QStringLiteral(
          "Fourteen units of four troop types are driven back and forth through a "
          "town whose staggered houses leave no straight street. Seven orders in "
          "under two minutes, three of them reversing a march in progress and one "
          "a full deploy; every one has to be acknowledged at once, nobody may pop, "
          "stand in a wall or wedge, and the army must end where it was last sent."),
      112.0F,
      {70.0F, 52.0F, 20.0F});
  scenario.terrain_grid_extent = 190;
  scenario.arena_floor_half_extent = 60.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}};
  build_staggered_town(scenario);

  scenario.groups.push_back(building(QStringLiteral("north_palisade"),
                                     Spawn::WallSegment,
                                     1,
                                     44,
                                     QVector3D(-43.0F, 0.0F, 18.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("south_palisade"),
                                     Spawn::WallSegment,
                                     1,
                                     44,
                                     QVector3D(-43.0F, 0.0F, -18.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));

  QVector3D const west(-44.0F, 0.0F, 0.0F);
  QVector3D const east(44.0F, 0.0F, 0.0F);
  QVector3D const north_east(40.0F, 0.0F, 12.0F);
  QVector3D const south(-40.0F, 0.0F, -12.0F);
  QVector3D const plaza(-1.0F, 0.0F, 0.0F);
  auto const army = add_roman_army(scenario, west);

  order_army(scenario, 1.0F, army, east, QStringLiteral("east"));
  order_army(scenario, 13.0F, army, west, QStringLiteral("about_face"));
  order_army(scenario, 25.0F, army, north_east, QStringLiteral("north_east"));
  order_army(scenario, 40.0F, army, south, QStringLiteral("south"), 8.0F);
  scenario.steps.push_back(deploy_army(56.0F,
                                       army,
                                       QVector3D(-54.0F, 0.0F, 0.0F),
                                       90.0F,
                                       Intent::Line,
                                       QStringLiteral("deploy_west_line")));
  order_army(scenario,
             72.0F,
             army,
             east + QVector3D(-4.0F, 0.0F, -4.0F),
             QStringLiteral("east_again"));
  order_army(scenario, 90.0F, army, plaza, QStringLiteral("plaza"), 2.5F);

  for (auto const& name : army) {
    expect_a_responsive_group(scenario, name);
  }

  for (int index = 0; index < army.size(); ++index) {
    float const offset = (static_cast<float>(index) - (army.size() - 1) * 0.5F) * 2.5F;
    expect_arrival(
        scenario, army.at(index), plaza + QVector3D(0.0F, 0.0F, offset), 9.0F);
  }
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto obstacle_slalom_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("maneuver_obstacle_slalom"),
      QStringLiteral("Manoeuvre: An Army Slaloms Through Clutter"),
      QStringLiteral(
          "The same army over open ground strewn with boulder fields, ruins, tree "
          "clusters and a few farmsteads, ordered from corner to corner along a "
          "zigzag with two mid-march reversals. No leg is straight; every group has "
          "to keep re-steering around the next obstacle without stalling, popping "
          "or losing a soldier in the scenery."),
      96.0F,
      {72.0F, 54.0F, 25.0F});
  scenario.terrain_grid_extent = 190;
  scenario.arena_floor_half_extent = 60.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}};

  scenario.groups.push_back(building(QStringLiteral("farm_a"),
                                     Spawn::Farm,
                                     1,
                                     1,
                                     QVector3D(-12.0F, 0.0F, 14.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("homestead"),
                                     Spawn::Home,
                                     1,
                                     3,
                                     QVector3D(8.0F, 0.0F, -6.0F),
                                     QVector3D(9.0F, 0.0F, 5.0F)));
  scenario.groups.push_back(building(QStringLiteral("farm_b"),
                                     Spawn::Farm,
                                     1,
                                     1,
                                     QVector3D(24.0F, 0.0F, 22.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.resource_patches = {
      patch_of("boulder",
               5,
               QVector3D(-30.0F, 0.0F, -12.0F),
               QVector3D(4.0F, 0.0F, 3.0F),
               1.3F),
      patch_of("boulder",
               4,
               QVector3D(-8.0F, 0.0F, -24.0F),
               QVector3D(5.0F, 0.0F, -2.0F),
               1.2F),
      patch_of("boulder",
               5,
               QVector3D(14.0F, 0.0F, 10.0F),
               QVector3D(3.5F, 0.0F, 3.5F),
               1.1F),
      patch_of("boulder",
               4,
               QVector3D(30.0F, 0.0F, -20.0F),
               QVector3D(4.0F, 0.0F, 4.0F),
               1.3F),
      patch_of("ruins",
               3,
               QVector3D(-26.0F, 0.0F, 24.0F),
               QVector3D(8.0F, 0.0F, -3.0F),
               1.0F),
      patch_of(
          "ruins", 2, QVector3D(36.0F, 0.0F, 4.0F), QVector3D(0.0F, 0.0F, 9.0F), 1.0F),
      patch_of("pine_tree",
               6,
               QVector3D(-4.0F, 0.0F, 30.0F),
               QVector3D(4.0F, 0.0F, -2.0F),
               1.0F),
      patch_of("pine_tree",
               5,
               QVector3D(-38.0F, 0.0F, 4.0F),
               QVector3D(2.0F, 0.0F, 5.0F),
               1.0F),
      patch_of("pine_tree",
               5,
               QVector3D(18.0F, 0.0F, -30.0F),
               QVector3D(4.0F, 0.0F, 2.0F),
               1.0F),
      patch_of("supply_cart",
               3,
               QVector3D(0.0F, 0.0F, 4.0F),
               QVector3D(4.0F, 0.0F, -4.0F),
               1.0F),
  };

  QVector3D const start(-44.0F, 0.0F, -30.0F);
  auto const army = add_roman_army(scenario, start);

  order_army(scenario,
             1.0F,
             army,
             QVector3D(40.0F, 0.0F, 30.0F),
             QStringLiteral("north_east"));
  order_army(scenario,
             14.0F,
             army,
             QVector3D(-40.0F, 0.0F, 30.0F),
             QStringLiteral("north_west"));
  order_army(scenario,
             27.0F,
             army,
             QVector3D(40.0F, 0.0F, -30.0F),
             QStringLiteral("south_east"));
  order_army(
      scenario, 42.0F, army, QVector3D(-10.0F, 0.0F, 34.0F), QStringLiteral("north"));
  order_army(
      scenario, 56.0F, army, QVector3D(38.0F, 0.0F, 6.0F), QStringLiteral("east"));
  order_army(
      scenario, 68.0F, army, QVector3D(-42.0F, 0.0F, 0.0F), QStringLiteral("west"));
  order_army(scenario,
             80.0F,
             army,
             QVector3D(-4.0F, 0.0F, -4.0F),
             QStringLiteral("centre"),
             5.0F);

  for (auto const& name : army) {
    expect_a_responsive_group(scenario, name);
  }
  for (int index = 0; index < army.size(); ++index) {
    float const offset = (static_cast<float>(index) - (army.size() - 1) * 0.5F) * 5.0F;
    expect_arrival(
        scenario, army.at(index), QVector3D(-4.0F, 0.0F, -4.0F + offset), 9.0F);
  }
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto countermarch_streets_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("maneuver_countermarch_streets"),
      QStringLiteral("Manoeuvre: Two Armies Counter-March Through Streets"),
      QStringLiteral(
          "Two friendly armies of six units each are sent through the same street "
          "grid from opposite ends, reversed before they meet, sent on again into "
          "each other, then crossed at right angles through the one junction, with "
          "an enemy block standing in the middle of it. Head-on traffic has to pass; "
          "neither army may deadlock, pile up or leave a unit behind."),
      104.0F,
      {70.0F, 54.0F, 20.0F});
  scenario.terrain_grid_extent = 190;
  scenario.arena_floor_half_extent = 60.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};

  scenario.groups.push_back(building(QStringLiteral("north_row_west"),
                                     Spawn::Home,
                                     1,
                                     3,
                                     QVector3D(-30.0F, 0.0F, 7.0F),
                                     QVector3D(10.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("north_row_east"),
                                     Spawn::Home,
                                     1,
                                     3,
                                     QVector3D(10.0F, 0.0F, 7.0F),
                                     QVector3D(10.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("south_row_west"),
                                     Spawn::Home,
                                     1,
                                     3,
                                     QVector3D(-30.0F, 0.0F, -7.0F),
                                     QVector3D(10.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("south_row_east"),
                                     Spawn::Home,
                                     1,
                                     3,
                                     QVector3D(10.0F, 0.0F, -7.0F),
                                     QVector3D(10.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("outer_north"),
                                     Spawn::Home,
                                     1,
                                     5,
                                     QVector3D(-25.0F, 0.0F, 17.0F),
                                     QVector3D(11.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("outer_south"),
                                     Spawn::Home,
                                     1,
                                     5,
                                     QVector3D(-19.0F, 0.0F, -17.0F),
                                     QVector3D(11.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("market"),
                                     Spawn::Marketplace,
                                     1,
                                     1,
                                     QVector3D(0.0F, 0.0F, 26.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.resource_patches = {
      patch_of("weapon_rack",
               2,
               QVector3D(-14.0F, 0.0F, 2.5F),
               QVector3D(3.0F, 0.0F, 0.0F),
               1.0F),
      patch_of("supply_cart",
               1,
               QVector3D(16.0F, 0.0F, -2.5F),
               QVector3D(0.0F, 0.0F, 0.0F),
               1.0F),
      patch_of("boulder",
               3,
               QVector3D(-40.0F, 0.0F, 24.0F),
               QVector3D(5.0F, 0.0F, 3.0F),
               1.2F),
      patch_of("boulder",
               3,
               QVector3D(36.0F, 0.0F, -26.0F),
               QVector3D(5.0F, 0.0F, 3.0F),
               1.2F),
  };

  const QString west_swords = QStringLiteral("west_swords");
  const QString west_spears = QStringLiteral("west_spears");
  const QString east_swords = QStringLiteral("east_swords");
  const QString east_archers = QStringLiteral("east_archers");
  const QString holders = QStringLiteral("holders");
  QVector3D const west_home(-44.0F, 0.0F, 0.0F);
  QVector3D const east_home(44.0F, 0.0F, 0.0F);
  scenario.groups.push_back(group(west_swords,
                                  Troop::Swordsman,
                                  1,
                                  3,
                                  west_home + QVector3D(0.0F, 0.0F, -5.0F),
                                  12));
  scenario.groups.push_back(group(
      west_spears, Troop::Spearman, 1, 3, west_home + QVector3D(0.0F, 0.0F, 6.0F), 12));
  auto east_a = group(east_swords,
                      Troop::Swordsman,
                      1,
                      3,
                      east_home + QVector3D(0.0F, 0.0F, -5.0F),
                      12);
  east_a.facing_degrees = 270.0F;
  scenario.groups.push_back(std::move(east_a));
  auto east_b = group(
      east_archers, Troop::Archer, 1, 3, east_home + QVector3D(0.0F, 0.0F, 6.0F), 10);
  east_b.facing_degrees = 270.0F;
  scenario.groups.push_back(std::move(east_b));
  auto blockers =
      group(holders, Troop::Swordsman, 2, 1, QVector3D(0.0F, 0.0F, 1.0F), 10);
  blockers.attacks_disabled = true;
  scenario.groups.push_back(std::move(blockers));

  QStringList const west_army{west_swords, west_spears};
  QStringList const east_army{east_swords, east_archers};

  order_army(
      scenario, 1.0F, west_army, east_home, QStringLiteral("west_to_east"), 5.0F);
  order_army(
      scenario, 1.0F, east_army, west_home, QStringLiteral("east_to_west"), 5.0F);
  order_army(scenario, 15.0F, west_army, west_home, QStringLiteral("west_back"), 5.0F);
  order_army(scenario, 15.0F, east_army, east_home, QStringLiteral("east_back"), 5.0F);
  order_army(scenario,
             28.0F,
             west_army,
             east_home,
             QStringLiteral("west_to_east_again"),
             5.0F);
  order_army(scenario,
             28.0F,
             east_army,
             west_home,
             QStringLiteral("east_to_west_again"),
             5.0F);
  order_army(scenario,
             50.0F,
             west_army,
             QVector3D(0.0F, 0.0F, 34.0F),
             QStringLiteral("west_to_north"),
             5.0F);
  order_army(scenario,
             50.0F,
             east_army,
             QVector3D(0.0F, 0.0F, -34.0F),
             QStringLiteral("east_to_south"),
             5.0F);
  order_army(scenario,
             70.0F,
             west_army,
             QVector3D(0.0F, 0.0F, -30.0F),
             QStringLiteral("west_to_south"),
             5.0F);
  order_army(scenario,
             70.0F,
             east_army,
             QVector3D(0.0F, 0.0F, 30.0F),
             QStringLiteral("east_to_north"),
             5.0F);

  for (auto const& name : {west_swords, west_spears, east_swords, east_archers}) {
    expect_a_responsive_group(scenario, name);
  }
  expect_arrival(scenario, west_swords, QVector3D(0.0F, 0.0F, -32.5F), 9.0F);
  expect_arrival(scenario, west_spears, QVector3D(0.0F, 0.0F, -27.5F), 9.0F);
  expect_arrival(scenario, east_swords, QVector3D(0.0F, 0.0F, 27.5F), 9.0F);
  expect_arrival(scenario, east_archers, QVector3D(0.0F, 0.0F, 32.5F), 9.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

} // namespace

auto build_maneuver_definitions() -> std::vector<ArenaScenarioDefinition> {
  return {
      town_relay_scenario(),
      obstacle_slalom_scenario(),
      countermarch_streets_scenario(),
  };
}

} // namespace Arena::Scenarios
