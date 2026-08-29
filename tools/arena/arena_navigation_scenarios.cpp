

#include "arena_navigation_scenarios.h"

#include <QVector3D>

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

  result.suppress_terrain_scatter = false;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_boundary_mountains = true;
  return result;
}

auto group(QString name,
           Troop troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals,
           QVector3D spacing) -> ArenaScenarioGroup {
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

auto expectation(Expect kind,
                 QString source = {},
                 float threshold = 0.0F,
                 float distance = 0.0F,
                 float start_seconds = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.threshold = threshold;
  result.distance = distance;
  result.start_seconds = start_seconds;
  return result;
}

auto patch_of(const char* prop_type,
              int count,
              QVector3D origin,
              float scale) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type),
          count,
          origin,
          QVector3D(3.0F, 0.0F, 0.0F),
          scale};
}

auto building(QString name,
              Spawn type,
              Nation nation,
              int owner,
              int count,
              QVector3D origin,
              QVector3D spacing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = nation;
  result.owner_id = owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = 0.0F;
  return result;
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

void expect_a_clean_march(ArenaScenarioDefinition& scenario,
                          const QString& group_name,
                          QVector3D destination,
                          float arrival_tolerance) {
  scenario.expectations.push_back(
      expectation(Expect::AllGroupsRespondWithin, group_name, 2.5F));
  scenario.expectations.push_back(
      expectation(Expect::MovementAnimationObserved, group_name));
  scenario.expectations.push_back(
      expectation(Expect::MovementIsContinuous, group_name, 2.5F));
  scenario.expectations.push_back(expectation(Expect::NoRootTeleport, group_name));
  scenario.expectations.push_back(
      expectation(Expect::UnitsClearOfBuildings, group_name));
  scenario.expectations.push_back(expectation(Expect::GroupIsRendered, group_name));

  auto arrived = expectation(Expect::GroupReachedDestination, group_name);
  arrived.distance = arrival_tolerance;
  arrived.position = destination;
  scenario.expectations.push_back(std::move(arrived));
}

void carve_hill_gap(ArenaScenarioDefinition& scenario, float gap) {
  constexpr float k_hill_radius = 15.0F;
  float const offset = (gap * 0.5F) + k_hill_radius;

  auto hill = [&](float center_z) {
    Game::Map::TerrainFeature feature;
    feature.type = Game::Map::TerrainType::Hill;
    feature.center_x = 0.0F;
    feature.center_z = center_z;
    feature.radius = k_hill_radius;
    feature.height = 7.0F;

    feature.entrances.push_back(QVector3D(
        0.0F, 0.0F, center_z + (center_z > 0.0F ? k_hill_radius : -k_hill_radius)));
    return feature;
  };
  scenario.terrain_features.push_back(hill(offset));
  scenario.terrain_features.push_back(hill(-offset));
}

struct HillTraversalShape {
  const char* slug;
  const char* prose;
  Game::Map::HillShape shape;
  float thickness;
  QVector3D crown;
};

auto hill_traversal_scenario(const HillTraversalShape& profile,
                             int entrance_count) -> ArenaScenarioDefinition {
  constexpr float k_hill_radius = 10.0F;
  constexpr float k_hill_height = 7.0F;
  constexpr float k_shape_extent = 24.0F;
  constexpr float k_climb_order = 0.6F;
  constexpr float k_settle_seconds = 2.0F;
  constexpr float k_leg_reversal_tolerance = 0.6F;
  constexpr float k_slope_band_ceiling = 5.0F;
  constexpr float k_soldier_ground_transient_samples = 24.0F;
  constexpr float k_climb_window_start = 1.0F;
  constexpr float k_climb_window_end = 52.0F;
  constexpr float k_descend_order = 54.0F;
  constexpr float k_descend_window_start = 56.0F;
  constexpr float k_duration = 104.0F;

  QVector3D const staging(-22.0F, 0.0F, 0.0F);

  QString const entrances_prose =
      entrance_count == 0
          ? QStringLiteral("no authored entrance, so the generated one is the only "
                           "way up")
          : (entrance_count == 1 ? QStringLiteral("a single authored entrance")
                                 : QStringLiteral("two authored entrances"));

  auto scenario = definition(
      QStringLiteral("nav_hill_%1_%2_entrances")
          .arg(QString::fromLatin1(profile.slug))
          .arg(entrance_count),
      QStringLiteral("Navigation: Onto And Off A %1 Hill (%2)")
          .arg(QString::fromLatin1(profile.prose))
          .arg(entrance_count),
      QStringLiteral("A troop climbs a %1 hill with %2, then walks back off it. "
                     "It may only ever stand on ground it is allowed to stand on, "
                     "and neither leg may reverse: no sliding back down the slope "
                     "and no being dragged back up it.")
          .arg(QString::fromLatin1(profile.prose))
          .arg(entrances_prose),
      k_duration,
      {44.0F, 52.0F, 25.0F});
  scenario.terrain_grid_extent = 120;
  scenario.arena_floor_half_extent = 40.0F;
  scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  scenario.terrain_height_scale_override = 0.5F;
  scenario.suppress_terrain_scatter = true;

  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.radius = k_hill_radius;
  hill.height = k_hill_height;
  hill.shape = profile.shape;
  if (profile.shape != Game::Map::HillShape::Blob) {
    hill.width = k_shape_extent;
    hill.depth = k_shape_extent;
    hill.thickness = profile.thickness;
  }
  if (entrance_count >= 1) {
    hill.entrances.push_back(QVector3D(-12.0F, 0.0F, 0.0F));
  }
  if (entrance_count >= 2) {
    hill.entrances.push_back(QVector3D(12.0F, 0.0F, 0.0F));
  }
  scenario.terrain_features.push_back(hill);

  const QString climbers = QStringLiteral("climbers");
  scenario.groups = {
      group(climbers, Troop::Spearman, 1, 1, staging, 8, QVector3D(0.0F, 0.0F, 3.2F))};
  scenario.steps = {move_to(k_climb_order, climbers, profile.crown),
                    move_to(k_descend_order, climbers, staging)};

  scenario.expectations.push_back(
      expectation(Expect::AllGroupsRespondWithin, climbers, 2.5F));
  scenario.expectations.push_back(
      expectation(Expect::MovementAnimationObserved, climbers));
  scenario.expectations.push_back(expectation(Expect::NoRootTeleport, climbers));
  scenario.expectations.push_back(expectation(Expect::GroupIsRendered, climbers));
  scenario.expectations.push_back(
      expectation(Expect::UnitsStayOnWalkableGround, climbers));
  scenario.expectations.push_back(expectation(Expect::SoldiersStayOnWalkableGround,
                                              climbers,
                                              k_soldier_ground_transient_samples,
                                              0.0F,
                                              k_settle_seconds));

  auto climbed = expectation(Expect::ElevationGainObserved, climbers, 4.5F);
  scenario.expectations.push_back(std::move(climbed));

  auto climb_leg = expectation(Expect::ElevationClimbIsMonotonic,
                               climbers,
                               k_leg_reversal_tolerance,
                               k_slope_band_ceiling,
                               k_climb_window_start);
  climb_leg.end_seconds = k_climb_window_end;
  scenario.expectations.push_back(std::move(climb_leg));

  auto descent_leg = expectation(Expect::ElevationDescentIsMonotonic,
                                 climbers,
                                 k_leg_reversal_tolerance,
                                 k_slope_band_ceiling,
                                 k_descend_window_start);
  scenario.expectations.push_back(std::move(descent_leg));

  auto returned = expectation(Expect::GroupReachedDestination, climbers);
  returned.distance = 5.0F;
  returned.position = staging;
  scenario.expectations.push_back(std::move(returned));

  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

auto hill_gap_scenario(const char* id,
                       const char* label,
                       const char* description,
                       int units,
                       int individuals,
                       float gap,
                       float duration,
                       float order_check_from) -> ArenaScenarioDefinition {
  auto scenario = definition(QString::fromLatin1(id),
                             QString::fromLatin1(label),
                             QString::fromLatin1(description),
                             duration,
                             {52.0F, 55.0F, 0.0F});
  scenario.terrain_grid_extent = 160;
  scenario.arena_floor_half_extent = 46.0F;
  scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  carve_hill_gap(scenario, gap);

  QVector3D const destination(26.0F, 0.0F, 0.0F);
  scenario.groups = {group(QStringLiteral("column"),
                           Troop::Spearman,
                           1,
                           units,
                           QVector3D(-26.0F, 0.0F, 0.0F),
                           individuals,
                           QVector3D(0.0F, 0.0F, 3.2F))};
  scenario.steps = {move_to(0.6F, QStringLiteral("column"), destination)};

  expect_a_clean_march(scenario, QStringLiteral("column"), destination, 6.0F);

  scenario.expectations.push_back(expectation(Expect::FormationOrderPreserved,
                                              QStringLiteral("column"),
                                              1.5F,
                                              0.0F,
                                              order_check_from));
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
  return scenario;
}

} // namespace

auto build_navigation_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  result.push_back(hill_gap_scenario(
      "nav_hill_gap_single",
      "Navigation: One Troop Between Two Hills",
      "A single spearman troop crosses a lane between two hills. Nothing about "
      "the ground should change its shape.",
      1,
      12,
      14.0F,
      48.0F,
      0.0F));
  result.push_back(hill_gap_scenario(
      "nav_hill_gap_line",
      "Navigation: Nine Troops Between Two Hills",
      "Nine troops abreast take a lane narrower than the line they stand in. "
      "They close ranks to fit and open again on the far side; they never form "
      "a column.",
      9,
      12,
      12.0F,
      58.0F,
      50.0F));
  result.push_back(hill_gap_scenario(
      "nav_hill_gap_army",
      "Navigation: An Army Between Two Hills",
      "Twenty-five troops through a lane far narrower than their frontage. The "
      "block squeezes as far as it goes and walks through overlapping rather "
      "than re-forming.",
      25,
      12,
      10.0F,
      95.0F,
      82.0F));

  result.push_back(hill_gap_scenario(
      "nav_hill_gap_wide",
      "Navigation: A Line Through A Lane That Fits It",
      "Nine troops take a lane wider than the line they stand in. Nothing about "
      "the block may change: not its files, not its order, not its spacing.",
      9,
      12,
      34.0F,
      52.0F,
      0.0F));

  {
    const HillTraversalShape shapes[] = {
        {"blob",
         "round",
         Game::Map::HillShape::Blob,
         0.0F,
         QVector3D(2.0F, 0.0F, 0.0F)},
        {"corridor",
         "ridge",
         Game::Map::HillShape::Corridor,
         9.0F,
         QVector3D(0.0F, 0.0F, 0.0F)},
        {"arc",
         "crescent",
         Game::Map::HillShape::Arc,
         9.0F,
         QVector3D(7.0F, 0.0F, 0.0F)},
        {"elbow",
         "elbow",
         Game::Map::HillShape::Elbow,
         9.0F,
         QVector3D(-2.0F, 0.0F, -6.0F)},
        {"ring",
         "ring",
         Game::Map::HillShape::Ring,
         7.0F,
         QVector3D(5.0F, 0.0F, -5.0F)},
    };
    for (const auto& shape : shapes) {
      for (int entrances = 0; entrances <= 2; ++entrances) {
        result.push_back(hill_traversal_scenario(shape, entrances));
      }
    }
  }

  {
    auto scenario = definition(
        QStringLiteral("nav_ruin_melee"),
        QStringLiteral("Navigation: A Fight Beside A Ruin"),
        QStringLiteral(
            "A patrol and a wolf pack close on each other across a ruin. Both "
            "sides have to go around it, and the melee has to settle on open "
            "ground rather than on top of the stonework."),
        30.0F,
        {34.0F, 50.0F, 20.0F});
    scenario.terrain_grid_extent = 128;
    scenario.arena_floor_half_extent = 30.0F;
    scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    scenario.resource_patches = {
        patch_of("ruins", 1, QVector3D(0.0F, 0.0F, 0.0F), 1.2F),
        patch_of("ruins", 1, QVector3D(3.0F, 0.0F, 4.0F), 0.9F),
        patch_of("weapon_rack", 1, QVector3D(-4.0F, 0.0F, -3.5F), 1.0F),
    };
    scenario.groups = {
        group(QStringLiteral("patrol"),
              Troop::Spearman,
              1,
              2,
              QVector3D(-14.0F, 0.0F, 0.0F),
              8,
              QVector3D(0.0F, 0.0F, 3.4F)),
        group(QStringLiteral("raiders"),
              Troop::Swordsman,
              2,
              2,
              QVector3D(14.0F, 0.0F, 0.0F),
              8,
              QVector3D(0.0F, 0.0F, 3.4F)),
    };
    scenario.steps = {
        move_to(0.6F, QStringLiteral("patrol"), QVector3D(9.0F, 0.0F, 0.0F)),
        move_to(0.6F, QStringLiteral("raiders"), QVector3D(-9.0F, 0.0F, 0.0F))};
    for (auto const& name : {QStringLiteral("patrol"), QStringLiteral("raiders")}) {
      scenario.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, 2.5F));
      scenario.expectations.push_back(expectation(Expect::NoRootTeleport, name));
      scenario.expectations.push_back(
          expectation(Expect::MovementIsContinuous, name, 3.0F));
      scenario.expectations.push_back(expectation(Expect::GroupIsRendered, name));
    }
    scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = definition(
        QStringLiteral("nav_town_crossing"),
        QStringLiteral("Navigation: Crossing A Built-Up Town"),
        QStringLiteral("Troops cross a street lined with barracks, homes and a market, "
                       "past weapon racks, carts and ruins. Nothing may be walked "
                       "through, and nobody may be left standing at the start."),
        45.0F,
        {56.0F, 52.0F, 24.0F});
    scenario.terrain_grid_extent = 160;
    scenario.arena_floor_half_extent = 44.0F;
    scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

    scenario.groups = {
        group(QStringLiteral("marchers"),
              Troop::Spearman,
              1,
              6,
              QVector3D(-30.0F, 0.0F, 0.0F),
              10,
              QVector3D(0.0F, 0.0F, 3.4F)),
        building(QStringLiteral("north_row"),
                 Spawn::Barracks,
                 Nation::RomanRepublic,
                 1,
                 3,
                 QVector3D(-12.0F, 0.0F, 12.0F),
                 QVector3D(14.0F, 0.0F, 0.0F)),
        building(QStringLiteral("south_row"),
                 Spawn::Home,
                 Nation::RomanRepublic,
                 1,
                 4,
                 QVector3D(-14.0F, 0.0F, -12.0F),
                 QVector3D(10.0F, 0.0F, 0.0F)),
        building(QStringLiteral("market"),
                 Spawn::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 QVector3D(6.0F, 0.0F, -1.0F),
                 QVector3D(8.0F, 0.0F, 0.0F)),
    };
    scenario.resource_patches = {
        patch_of("ruins", 2, QVector3D(-6.0F, 0.0F, 5.0F), 1.0F),
        patch_of("weapon_rack", 3, QVector3D(-2.0F, 0.0F, -6.0F), 1.0F),
        patch_of("supply_cart", 2, QVector3D(10.0F, 0.0F, 7.0F), 1.0F),
        patch_of("pine_tree", 4, QVector3D(16.0F, 0.0F, -7.0F), 1.0F),
    };

    QVector3D const destination(30.0F, 0.0F, 0.0F);
    scenario.steps = {move_to(0.8F, QStringLiteral("marchers"), destination)};
    expect_a_clean_march(scenario, QStringLiteral("marchers"), destination, 8.0F);
    scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F));
    result.push_back(std::move(scenario));
  }

  return result;
}

} // namespace Arena::Scenarios
