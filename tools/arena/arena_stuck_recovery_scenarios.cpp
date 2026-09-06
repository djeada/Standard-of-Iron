#include "arena_stuck_recovery_scenarios.h"

#include <QVector3D>

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/systems/nation_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr float k_stall_budget_seconds = 12.0F;

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
           QVector3D spacing = QVector3D(0.0F, 0.0F, 3.2F)) -> ArenaScenarioGroup {
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

auto move_direct(float time,
                 const QString& group_name,
                 QVector3D destination) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("move_%1").arg(group_name);
  step.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  step.command = Command::Move;
  step.group = group_name;
  step.destination = destination;
  return step;
}

auto move_to(float time,
             const QString& group_name,
             QVector3D destination) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("move_%1").arg(group_name);
  step.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  step.command = Command::FormationMove;
  step.group = group_name;
  step.destination = destination;
  return step;
}

void expect_no_permanent_stall(ArenaScenarioDefinition& scenario,
                               const QString& group_name) {
  scenario.expectations.push_back(
      expectation(Expect::AllGroupsRespondWithin, group_name, 2.5F));
  scenario.expectations.push_back(expectation(Expect::NoRootTeleport, group_name));
  scenario.expectations.push_back(expectation(Expect::GroupIsRendered, group_name));
  scenario.expectations.push_back(
      expectation(Expect::NoPermanentStall, group_name, k_stall_budget_seconds));
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

auto gate_column_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_gate_column"),
      QStringLiteral("Stuck: A Block Through Its Own Gate"),
      QStringLiteral("Nine troops abreast are sent through a gateway a fraction of "
                     "their frontage wide. The line cannot fit as a line, so it "
                     "has to become a column and walk through; standing outside "
                     "its own gate until the scenario ends is the bug."),
      70.0F,
      {46.0F, 52.0F, 0.0F});
  scenario.terrain_grid_extent = 140;
  scenario.arena_floor_half_extent = 40.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}};

  scenario.groups.push_back(building(QStringLiteral("west_wall"),
                                     Spawn::WallSegment,
                                     1,
                                     6,
                                     QVector3D(-9.0F, 0.0F, 0.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("east_wall"),
                                     Spawn::WallSegment,
                                     1,
                                     6,
                                     QVector3D(9.0F, 0.0F, 0.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("gate"),
                                     Spawn::WallGate,
                                     1,
                                     1,
                                     QVector3D(0.0F, 0.0F, 0.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));

  const QString column = QStringLiteral("column");
  QVector3D const destination(0.0F, 0.0F, 24.0F);
  auto marchers = group(column,
                        Troop::Spearman,
                        1,
                        9,
                        QVector3D(0.0F, 0.0F, -24.0F),
                        10,
                        QVector3D(3.2F, 0.0F, 0.0F));
  marchers.facing_degrees = 0.0F;
  scenario.groups.push_back(std::move(marchers));
  scenario.steps = {move_to(0.8F, column, destination)};

  expect_no_permanent_stall(scenario, column);
  expect_arrival(scenario, column, destination, 8.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto town_streets_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_town_streets"),
      QStringLiteral("Stuck: Narrow Streets Of A Built-Up Town"),
      QStringLiteral("Two columns cross a town whose houses leave streets barely "
                     "wider than a file. Every troop either comes out the far "
                     "side or is visibly stood down; none may be left facing a "
                     "wall it has stopped walking into."),
      80.0F,
      {58.0F, 50.0F, 24.0F});
  scenario.terrain_grid_extent = 170;
  scenario.arena_floor_half_extent = 48.0F;

  scenario.groups.push_back(building(QStringLiteral("row_a"),
                                     Spawn::Home,
                                     1,
                                     5,
                                     QVector3D(-16.0F, 0.0F, 7.0F),
                                     QVector3D(8.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_b"),
                                     Spawn::Home,
                                     1,
                                     5,
                                     QVector3D(-16.0F, 0.0F, -7.0F),
                                     QVector3D(8.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("row_c"),
                                     Spawn::Barracks,
                                     1,
                                     2,
                                     QVector3D(-4.0F, 0.0F, 18.0F),
                                     QVector3D(16.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("market"),
                                     Spawn::Marketplace,
                                     1,
                                     1,
                                     QVector3D(4.0F, 0.0F, 0.0F),
                                     QVector3D(0.0F, 0.0F, 0.0F)));
  scenario.resource_patches = {
      patch_of("supply_cart",
               2,
               QVector3D(-9.0F, 0.0F, 0.0F),
               QVector3D(0.0F, 0.0F, 3.0F),
               1.0F),
      patch_of("weapon_rack",
               3,
               QVector3D(12.0F, 0.0F, 2.0F),
               QVector3D(2.5F, 0.0F, 0.0F),
               1.0F),
  };

  const QString westbound = QStringLiteral("westbound");
  const QString eastbound = QStringLiteral("eastbound");
  QVector3D const west(-30.0F, 0.0F, 0.0F);
  QVector3D const east(30.0F, 0.0F, 0.0F);
  scenario.groups.push_back(group(eastbound, Troop::Spearman, 1, 4, west, 10));
  scenario.groups.push_back(group(westbound, Troop::Swordsman, 1, 4, east, 10));
  scenario.steps = {move_to(0.8F, eastbound, east), move_to(0.8F, westbound, west)};

  expect_no_permanent_stall(scenario, eastbound);
  expect_no_permanent_stall(scenario, westbound);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto forest_march_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_forest_march"),
      QStringLiteral("Stuck: A March Through Close Woods"),
      QStringLiteral("A column marches through a stand of pines packed tightly "
                     "enough that the gaps between trunks are the only route. "
                     "Nobody may end the march standing against a tree."),
      70.0F,
      {50.0F, 50.0F, 20.0F});
  scenario.terrain_grid_extent = 150;
  scenario.arena_floor_half_extent = 44.0F;

  for (int row = 0; row < 6; ++row) {
    scenario.resource_patches.push_back(
        patch_of("pine_tree",
                 9,
                 QVector3D(-10.0F + (static_cast<float>(row) * 4.0F),
                           0.0F,
                           -16.0F + (static_cast<float>(row % 2) * 2.0F)),
                 QVector3D(0.0F, 0.0F, 4.0F),
                 1.0F));
  }

  const QString column = QStringLiteral("column");
  QVector3D const destination(26.0F, 0.0F, 0.0F);
  scenario.groups.push_back(
      group(column, Troop::Spearman, 1, 3, QVector3D(-26.0F, 0.0F, 0.0F), 10));
  scenario.steps = {move_to(0.8F, column, destination)};

  expect_no_permanent_stall(scenario, column);
  expect_arrival(scenario, column, destination, 9.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto hill_pinch_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_hill_pinch"),
      QStringLiteral("Stuck: An Army Through A Pinch Between Hills"),
      QStringLiteral("Sixteen troops are sent through a gap between two hills far "
                     "narrower than their frontage, with the whole block arriving "
                     "at it at once. The traffic jam is expected; being left in "
                     "it when the scenario ends is not."),
      110.0F,
      {56.0F, 55.0F, 0.0F});
  scenario.terrain_grid_extent = 180;
  scenario.arena_floor_half_extent = 52.0F;
  scenario.terrain_height_scale_override = 0.5F;

  auto hill = [](float center_z) {
    Game::Map::TerrainFeature feature;
    feature.type = Game::Map::TerrainType::Hill;
    feature.center_x = 0.0F;
    feature.center_z = center_z;
    feature.radius = 16.0F;
    feature.height = 8.0F;
    return feature;
  };
  scenario.terrain_features.push_back(hill(20.0F));
  scenario.terrain_features.push_back(hill(-20.0F));

  const QString army = QStringLiteral("army");
  QVector3D const destination(30.0F, 0.0F, 0.0F);
  scenario.groups.push_back(
      group(army, Troop::Spearman, 1, 16, QVector3D(-30.0F, 0.0F, 0.0F), 10));
  scenario.steps = {move_to(0.8F, army, destination)};

  expect_no_permanent_stall(scenario, army);
  scenario.expectations.push_back(expectation(Expect::StallRecoveryObserved, army));
  expect_arrival(scenario, army, destination, 12.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto boulder_field_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_boulder_field"),
      QStringLiteral("Stuck: Crossing A Field Of Boulders"),
      QStringLiteral("Troops cross a scatter of boulders and ore that leaves only "
                     "winding gaps. The route is long and awkward; standing "
                     "against a rock for the rest of the run is not a route."),
      70.0F,
      {48.0F, 50.0F, 18.0F});
  scenario.terrain_grid_extent = 150;
  scenario.arena_floor_half_extent = 44.0F;

  for (int row = 0; row < 5; ++row) {
    scenario.resource_patches.push_back(
        patch_of("boulder",
                 7,
                 QVector3D(-8.0F + (static_cast<float>(row) * 4.5F),
                           0.0F,
                           -12.0F + (static_cast<float>(row % 2) * 2.5F)),
                 QVector3D(0.0F, 0.0F, 4.0F),
                 1.3F));
  }
  scenario.resource_patches.push_back(patch_of(
      "iron_ore", 3, QVector3D(2.0F, 0.0F, 6.0F), QVector3D(3.0F, 0.0F, 0.0F), 1.0F));

  const QString column = QStringLiteral("column");
  QVector3D const destination(26.0F, 0.0F, 0.0F);
  scenario.groups.push_back(
      group(column, Troop::Swordsman, 1, 4, QVector3D(-26.0F, 0.0F, 0.0F), 10));
  scenario.steps = {move_to(0.8F, column, destination)};

  expect_no_permanent_stall(scenario, column);
  expect_arrival(scenario, column, destination, 9.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto ruin_passage_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_ruin_passage"),
      QStringLiteral("Stuck: A Half-Blocked Passage Through Ruins"),
      QStringLiteral("A corridor between two wall lines is part-filled with fallen "
                     "stonework, leaving a passage that shifts from side to side. "
                     "Troops thread it or are stood down; none may be abandoned "
                     "mid-corridor still walking into a ruin."),
      75.0F,
      {46.0F, 50.0F, 12.0F});
  scenario.terrain_grid_extent = 150;
  scenario.arena_floor_half_extent = 42.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}};

  scenario.groups.push_back(building(QStringLiteral("north_wall"),
                                     Spawn::WallSegment,
                                     1,
                                     17,
                                     QVector3D(0.0F, 0.0F, 10.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("south_wall"),
                                     Spawn::WallSegment,
                                     1,
                                     17,
                                     QVector3D(0.0F, 0.0F, -10.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.resource_patches = {
      patch_of(
          "ruins", 2, QVector3D(-8.0F, 0.0F, 5.0F), QVector3D(4.0F, 0.0F, 0.0F), 1.0F),
      patch_of(
          "ruins", 2, QVector3D(2.0F, 0.0F, -5.0F), QVector3D(4.0F, 0.0F, 0.0F), 1.0F),
      patch_of(
          "ruins", 1, QVector3D(10.0F, 0.0F, 4.5F), QVector3D(0.0F, 0.0F, 0.0F), 1.0F),
  };

  const QString column = QStringLiteral("column");
  QVector3D const destination(24.0F, 0.0F, 0.0F);
  scenario.groups.push_back(
      group(column, Troop::Spearman, 1, 3, QVector3D(-24.0F, 0.0F, 0.0F), 8));
  scenario.steps = {move_to(0.8F, column, destination)};

  expect_no_permanent_stall(scenario, column);
  expect_arrival(scenario, column, destination, 9.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto crossing_congestion_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_crossing_congestion"),
      QStringLiteral("Stuck: Four Formations Through One Junction"),
      QStringLiteral("Two friendly columns cross at right angles through a gap in "
                     "a wall line while an enemy block holds the middle. Somebody "
                     "gives way; nobody deadlocks."),
      95.0F,
      {56.0F, 55.0F, 20.0F});
  scenario.terrain_grid_extent = 170;
  scenario.arena_floor_half_extent = 48.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};

  scenario.groups.push_back(building(QStringLiteral("west_wall"),
                                     Spawn::WallSegment,
                                     1,
                                     6,
                                     QVector3D(-11.0F, 0.0F, 10.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("east_wall"),
                                     Spawn::WallSegment,
                                     1,
                                     6,
                                     QVector3D(11.0F, 0.0F, 10.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));

  const QString eastbound = QStringLiteral("eastbound");
  const QString northbound = QStringLiteral("northbound");
  const QString holders = QStringLiteral("holders");
  QVector3D const east(28.0F, 0.0F, 0.0F);
  QVector3D const north(0.0F, 0.0F, 26.0F);

  scenario.groups.push_back(
      group(eastbound, Troop::Spearman, 1, 4, QVector3D(-28.0F, 0.0F, 0.0F), 8));
  auto crossing = group(northbound,
                        Troop::Swordsman,
                        1,
                        4,
                        QVector3D(0.0F, 0.0F, -22.0F),
                        8,
                        QVector3D(3.2F, 0.0F, 0.0F));
  crossing.facing_degrees = 0.0F;
  scenario.groups.push_back(std::move(crossing));

  auto blockers =
      group(holders, Troop::Swordsman, 2, 1, QVector3D(1.0F, 0.0F, 2.0F), 8);
  blockers.attacks_disabled = true;
  scenario.groups.push_back(std::move(blockers));

  scenario.steps = {move_to(0.8F, eastbound, east), move_to(0.8F, northbound, north)};

  expect_no_permanent_stall(scenario, eastbound);
  expect_no_permanent_stall(scenario, northbound);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto sealed_objective_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_sealed_objective"),
      QStringLiteral("Stuck: An Objective Ringed By Rock"),
      QStringLiteral("Troops are sent to a patch of ground walled off by an "
                     "unbroken ring of boulders. The destination is perfectly "
                     "good ground -- it is simply not connected to anything -- so "
                     "the order resolves to the near side of the rocks and ends "
                     "there. Nobody gets in, and nobody spends the match pressed "
                     "against the stones still trying."),
      60.0F,
      {46.0F, 50.0F, 16.0F});
  scenario.terrain_grid_extent = 150;
  scenario.arena_floor_half_extent = 42.0F;

  QVector3D const unreachable(14.0F, 0.0F, 0.0F);
  constexpr float k_ring_radius = 7.0F;
  constexpr int k_ring_stones = 96;
  for (int index = 0; index < k_ring_stones; ++index) {
    float const angle = (2.0F * std::numbers::pi_v<float> * static_cast<float>(index)) /
                        static_cast<float>(k_ring_stones);
    scenario.resource_patches.push_back(
        patch_of("boulder",
                 1,
                 QVector3D(unreachable.x() + (std::cos(angle) * k_ring_radius),
                           0.0F,
                           unreachable.z() + (std::sin(angle) * k_ring_radius)),
                 QVector3D(0.0F, 0.0F, 0.0F),
                 1.6F));
  }

  const QString hopefuls = QStringLiteral("hopefuls");
  scenario.groups.push_back(
      group(hopefuls, Troop::Spearman, 1, 1, QVector3D(-22.0F, 0.0F, 0.0F), 8));
  scenario.steps = {move_direct(0.8F, hopefuls, unreachable)};

  expect_no_permanent_stall(scenario, hopefuls);
  auto held = expectation(Expect::GroupHeldOutsideDestination, hopefuls, 0.0F, 5.0F);
  held.position = unreachable;
  scenario.expectations.push_back(std::move(held));
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto interrupted_march_scenario() -> ArenaScenarioDefinition {
  auto scenario = definition(
      QStringLiteral("stuck_interrupted_march"),
      QStringLiteral("Stuck: An Order Changed Mid-March"),
      QStringLiteral("A column is sent into a dead-end pocket, then pulled back "
                     "out again while it is still wedged in the mouth of it. The "
                     "second order has to take, and the first has to stop being "
                     "something anyone is still trying to carry out."),
      75.0F,
      {46.0F, 50.0F, 16.0F});
  scenario.terrain_grid_extent = 150;
  scenario.arena_floor_half_extent = 42.0F;
  scenario.owner_teams = {{.owner_id = 1, .team_id = 1}};

  scenario.groups.push_back(building(QStringLiteral("pocket_north"),
                                     Spawn::WallSegment,
                                     1,
                                     8,
                                     QVector3D(11.0F, 0.0F, 6.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("pocket_south"),
                                     Spawn::WallSegment,
                                     1,
                                     8,
                                     QVector3D(11.0F, 0.0F, -6.0F),
                                     QVector3D(2.0F, 0.0F, 0.0F)));
  scenario.groups.push_back(building(QStringLiteral("pocket_end"),
                                     Spawn::WallSegment,
                                     1,
                                     7,
                                     QVector3D(20.0F, 0.0F, 0.0F),
                                     QVector3D(0.0F, 0.0F, 2.0F)));

  const QString column = QStringLiteral("column");
  QVector3D const staging(-24.0F, 0.0F, 0.0F);
  scenario.groups.push_back(group(column, Troop::Spearman, 1, 4, staging, 10));
  scenario.steps = {move_to(0.8F, column, QVector3D(28.0F, 0.0F, 0.0F)),
                    move_to(30.0F, column, staging)};

  expect_no_permanent_stall(scenario, column);
  expect_arrival(scenario, column, staging, 10.0F);
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

} // namespace

auto build_stuck_recovery_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(gate_column_scenario());
  result.push_back(town_streets_scenario());
  result.push_back(forest_march_scenario());
  result.push_back(hill_pinch_scenario());
  result.push_back(boulder_field_scenario());
  result.push_back(ruin_passage_scenario());
  result.push_back(crossing_congestion_scenario());
  result.push_back(sealed_objective_scenario());
  result.push_back(interrupted_march_scenario());
  return result;
}

} // namespace Arena::Scenarios
