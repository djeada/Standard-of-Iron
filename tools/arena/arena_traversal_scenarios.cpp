#include "arena_traversal_scenarios.h"

#include <QVector3D>

#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/map/terrain.h"
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

constexpr int k_marchers = 24;

constexpr int k_gate_marchers = 12;
constexpr float k_march_from = -20.0F;
constexpr float k_march_to = 20.0F;
constexpr float k_order_at = 1.0F;

auto definition(const char* id,
                const char* label,
                const char* description,
                float duration,
                ArenaCameraView camera) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = QString::fromLatin1(id);
  result.label = QString::fromLatin1(label);
  result.description = QString::fromLatin1(description);
  result.duration_seconds = duration;
  result.camera = camera;
  result.terrain_grid_extent = 52;
  result.arena_floor_half_extent = 26.0F;
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.suppress_terrain_scatter = true;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_boundary_mountains = true;
  return result;
}

auto marchers(int individuals = k_marchers) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = QStringLiteral("marchers");
  result.troop_type = Troop::Spearman;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = 1;
  result.individuals_per_unit = individuals;
  result.origin = QVector3D(k_march_from, 0.0F, 0.0F);
  result.spacing = QVector3D(0.0F, 0.0F, 4.0F);
  result.facing_degrees = 90.0F;
  return result;
}

auto building_row(const char* name,
                  Spawn type,
                  int count,
                  QVector3D origin,
                  QVector3D spacing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = QString::fromLatin1(name);
  result.spawn_type = type;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = 0.0F;
  return result;
}

void seal_the_flanks(ArenaScenarioDefinition& scenario,
                     Game::Map::TerrainType type,
                     float offset) {
  constexpr float k_ridge_radius = 8.0F;
  constexpr float k_ridge_step = 8.0F;
  constexpr float k_ridge_reach = 24.0F;
  for (float side : {-1.0F, 1.0F}) {
    for (float x = -k_ridge_reach; x <= k_ridge_reach; x += k_ridge_step) {
      Game::Map::TerrainFeature ridge;
      ridge.type = type;
      ridge.center_x = x;
      ridge.center_z = side * offset;
      ridge.radius = k_ridge_radius;
      ridge.height = type == Game::Map::TerrainType::Mountain ? 14.0F : 7.0F;
      scenario.terrain_features.push_back(ridge);
    }
  }
}

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing,
           float scale) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type), count, origin, spacing, scale, true};
}

auto march_order() -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("march");
  step.trigger = {Trigger::AtTime, k_order_at, {}, {}, 0.0F};
  step.command = Command::FormationMove;
  step.group = QStringLiteral("marchers");
  step.destination = QVector3D(k_march_to, 0.0F, 0.0F);
  return step;
}

auto expectation(Expect kind, float threshold = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = QStringLiteral("marchers");
  result.threshold = threshold;
  return result;
}

void expect_a_clean_crossing(ArenaScenarioDefinition& scenario) {
  scenario.expectations.push_back(expectation(Expect::AllGroupsRespondWithin, 2.5F));
  scenario.expectations.push_back(expectation(Expect::MovementAnimationObserved));
  scenario.expectations.push_back(expectation(Expect::NoRootTeleport));
  scenario.expectations.push_back(expectation(Expect::GroupIsRendered));
  scenario.expectations.push_back(expectation(Expect::UnitsClearOfBuildings));
  scenario.expectations.push_back(
      expectation(Expect::SoldiersStayOnWalkableGround, 24.0F));

  auto arrived = expectation(Expect::GroupReachedDestination);
  arrived.distance = 6.0F;
  arrived.position = QVector3D(k_march_to, 0.0F, 0.0F);
  scenario.expectations.push_back(std::move(arrived));
  scenario.expectations.push_back(expectation(Expect::FrameBudget, 33.34F));
}

void expect_narrow_order(ArenaScenarioDefinition& scenario,
                         float minimum_files,
                         float mode_changes) {
  scenario.expectations.push_back(expectation(Expect::NarrowLayoutEngaged));
  if (minimum_files > 0.0F) {

    scenario.expectations.push_back(
        expectation(Expect::NarrowLayoutKeepsFiles, minimum_files));
  }
  scenario.expectations.push_back(
      expectation(Expect::NarrowLayoutModeSettles, mode_changes));
  scenario.expectations.push_back(expectation(Expect::NarrowLayoutRestores, 0.6F));
}

void expect_normal_order(ArenaScenarioDefinition& scenario) {
  scenario.expectations.push_back(expectation(Expect::NarrowLayoutStaysWide));
  scenario.expectations.push_back(expectation(Expect::NarrowLayoutModeSettles, 1.0F));
  scenario.expectations.push_back(expectation(Expect::FormationOrderPreserved, 1.0F));
}

auto street_scenario(const char* id,
                     const char* label,
                     const char* description,
                     float row_offset,
                     float minimum_files,
                     bool narrow_expected) -> ArenaScenarioDefinition {
  auto scenario = definition(id, label, description, 46.0F, {50.0F, 54.0F, 18.0F});
  seal_the_flanks(scenario, Game::Map::TerrainType::Mountain, 13.0F);
  scenario.groups = {
      marchers(),
      building_row("north_row",
                   Spawn::Home,
                   5,
                   QVector3D(-3.5F, 0.0F, row_offset),
                   QVector3D(7.0F, 0.0F, 0.0F)),
      building_row("south_row",
                   Spawn::Home,
                   5,
                   QVector3D(-3.5F, 0.0F, -row_offset),
                   QVector3D(7.0F, 0.0F, 0.0F)),
  };
  scenario.steps = {march_order()};
  expect_a_clean_crossing(scenario);
  if (narrow_expected) {
    expect_narrow_order(scenario, minimum_files, 4.0F);
  } else {
    expect_normal_order(scenario);
  }
  return scenario;
}

} // namespace

auto build_traversal_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  result.push_back(street_scenario(
      k_traversal_city_street_id,
      "Traversal: A Street Between Two Rows Of Houses",
      "A block seven metres wide takes a street with a bit under six metres of "
      "clear road. It closes ranks to fit and opens again on the far side: a "
      "street is not a reason to give up a file, let alone all of them.",
      6.5F,
      5.0F,
      true));

  result.push_back(street_scenario(
      k_traversal_city_alley_id,
      "Traversal: A Back Alley Between Houses",
      "The same block takes an alley with under two metres of clear ground. "
      "Here it has to give up files, and what it takes instead must still be a "
      "formation: ranks in order, nobody standing on anybody.",
      5.3F,
      2.0F,
      true));

  result.push_back(street_scenario(
      k_traversal_wide_street_id,
      "Traversal: A Boulevard That Fits The Block",
      "A street twelve metres across, comfortably wider than the block. "
      "Nothing about the formation may change: not its files, not its spacing, "
      "not its order.",
      10.0F,
      0.0F,
      false));

  {
    auto scenario = definition(k_traversal_wall_gate_id,
                               "Traversal: A Walled Gate",
                               "A wall with one gate in it. Half a block files "
                               "through a one metre opening and re-forms on the "
                               "far side; single file is allowed here and only "
                               "here.",
                               76.0F,
                               {56.0F, 52.0F, 16.0F});
    seal_the_flanks(scenario, Game::Map::TerrainType::Mountain, 13.0F);
    scenario.groups = {
        marchers(k_gate_marchers),
        building_row("north_wall",
                     Spawn::WallSegment,
                     6,
                     QVector3D(0.0F, 0.0F, 7.0F),
                     QVector3D(0.0F, 0.0F, 2.0F)),
        building_row("south_wall",
                     Spawn::WallSegment,
                     6,
                     QVector3D(0.0F, 0.0F, -7.0F),
                     QVector3D(0.0F, 0.0F, 2.0F)),
        building_row(
            "gate", Spawn::WallGate, 1, QVector3D(0.0F, 0.0F, 0.0F), QVector3D()),
    };
    scenario.steps = {march_order()};
    expect_a_clean_crossing(scenario);
    expect_narrow_order(scenario, 1.0F, 4.0F);
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = definition(k_traversal_forest_path_id,
                               "Traversal: A Path Through The Pines",
                               "Two stands of pine leave a path narrower than "
                               "the block. It gives up the files the trunks "
                               "take and files through on the rest; how many "
                               "that is depends on where the trees fell.",
                               52.0F,
                               {54.0F, 50.0F, 20.0F});
    scenario.suppress_terrain_scatter = false;
    seal_the_flanks(scenario, Game::Map::TerrainType::Mountain, 13.0F);
    scenario.groups = {marchers()};
    scenario.resource_patches = {
        patch("pine_tree", 11, {-15.0F, 0.0F, 3.4F}, {3.0F, 0.0F, 0.0F}, 1.0F),
        patch("pine_tree", 11, {-15.0F, 0.0F, -3.4F}, {3.0F, 0.0F, 0.0F}, 1.0F),
        patch("pine_tree", 6, {-13.0F, 0.0F, 7.0F}, {5.0F, 0.0F, 0.0F}, 1.0F),
        patch("pine_tree", 6, {-13.0F, 0.0F, -7.0F}, {5.0F, 0.0F, 0.0F}, 1.0F),
    };
    scenario.steps = {march_order()};
    expect_a_clean_crossing(scenario);
    expect_narrow_order(scenario, 0.0F, 4.0F);
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = definition(k_traversal_hill_gap_id,
                               "Traversal: The Saddle Between Two Hills",
                               "A lane between two hills, narrower than the "
                               "block but wide enough to hold most of its "
                               "files. It closes ranks rather than filing off.",
                               52.0F,
                               {60.0F, 56.0F, 0.0F});
    seal_the_flanks(scenario, Game::Map::TerrainType::Hill, 12.0F);
    scenario.groups = {marchers()};
    scenario.steps = {march_order()};
    expect_a_clean_crossing(scenario);
    expect_narrow_order(scenario, 0.0F, 4.0F);
    scenario.expectations.push_back(expectation(Expect::FormationOrderPreserved, 1.5F));
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = definition(k_traversal_rock_cluster_id,
                               "Traversal: Between Two Boulder Fields",
                               "Dense stone on both sides leaves a two metre "
                               "passage. The block gives up the files it has "
                               "to and no more.",
                               52.0F,
                               {52.0F, 50.0F, 22.0F});
    scenario.suppress_terrain_scatter = false;
    seal_the_flanks(scenario, Game::Map::TerrainType::Mountain, 13.0F);
    scenario.groups = {marchers()};
    scenario.resource_patches = {
        patch("boulder", 13, {-15.0F, 0.0F, 2.2F}, {2.5F, 0.0F, 0.0F}, 1.2F),
        patch("boulder", 13, {-15.0F, 0.0F, -2.2F}, {2.5F, 0.0F, 0.0F}, 1.2F),
        patch("boulder", 6, {-13.0F, 0.0F, 5.6F}, {5.0F, 0.0F, 0.0F}, 1.0F),
        patch("boulder", 6, {-13.0F, 0.0F, -5.6F}, {5.0F, 0.0F, 0.0F}, 1.0F),
    };
    scenario.steps = {march_order()};
    expect_a_clean_crossing(scenario);
    expect_narrow_order(scenario, 0.0F, 4.0F);
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = definition(k_traversal_ruins_field_id,
                               "Traversal: An Irregular Field Of Ruins",
                               "Broken walls, carts and racks with no straight "
                               "line through them. The block narrows for the "
                               "pinches and widens between them without "
                               "flickering between the two.",
                               56.0F,
                               {52.0F, 50.0F, 24.0F});
    scenario.suppress_terrain_scatter = false;
    seal_the_flanks(scenario, Game::Map::TerrainType::Mountain, 13.0F);
    scenario.groups = {marchers()};
    scenario.resource_patches = {
        patch("ruins", 3, {-14.0F, 0.0F, 3.6F}, {9.0F, 0.0F, 0.0F}, 1.2F),
        patch("ruins", 3, {-9.0F, 0.0F, -3.6F}, {9.0F, 0.0F, 0.0F}, 1.2F),
        patch("supply_cart", 2, {2.0F, 0.0F, 2.8F}, {8.0F, 0.0F, 0.0F}, 1.0F),
        patch("weapon_rack", 2, {6.0F, 0.0F, -2.8F}, {8.0F, 0.0F, 0.0F}, 1.0F),
        patch("dead_tree", 3, {-4.0F, 0.0F, 6.0F}, {8.0F, 0.0F, 0.0F}, 1.0F),
    };
    scenario.steps = {march_order()};
    expect_a_clean_crossing(scenario);
    expect_narrow_order(scenario, 0.0F, 6.0F);
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = definition(k_traversal_open_ground_id,
                               "Traversal: Open Ground",
                               "Nothing in the way at all. The narrow-order "
                               "rule must never fire here, and the block must "
                               "cross at its own frontage and spacing.",
                               36.0F,
                               {50.0F, 52.0F, 18.0F});
    scenario.groups = {marchers()};
    scenario.steps = {march_order()};
    expect_a_clean_crossing(scenario);
    expect_normal_order(scenario);
    result.push_back(std::move(scenario));
  }

  return result;
}

} // namespace Arena::Scenarios
