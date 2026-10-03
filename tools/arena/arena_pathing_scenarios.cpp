#include "arena_pathing_scenarios.h"

#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

} // namespace

auto build_pathing_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_path_bridge_crossing_id),
        QStringLiteral("Pathfinding: Bridge Crossing"),
        QStringLiteral("Two infantry files funnel onto a production bridge deck, "
                       "cross the river, and reform on the far bank."),
        15.0F,
        {32.0F, 54.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 5.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 5.0F}, 4.5F, 0.45F});
    s.groups = {group(QStringLiteral("crossers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -11.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("crossers"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("crossers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("crossers")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("crossers")));
    s.expectations.push_back(expectation(Expect::BridgeCenterlineAligned,
                                         QStringLiteral("crossers"),
                                         {},
                                         0.0F,
                                         0.0F,
                                         0.50F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("crossers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_uphill_advance_id),
        QStringLiteral("Pathfinding: Uphill Advance"),
        QStringLiteral("Spearmen climb a smooth four-metre rise and settle on its "
                       "crown with grounded, continuous locomotion."),
        11.0F,
        {27.0F, 48.0F, 16.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, -1.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 2.0F}, 10.0F, 4.0F});
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-1.5F, 0.0F, -10.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 2.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(expectation(
        Expect::ElevationGainObserved, QStringLiteral("climbers"), {}, 2.5F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               2.5F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_wall_detour_id),
        QStringLiteral("Pathfinding: Wall Detour"),
        QStringLiteral("Infantry ordered through an intact palisade must remain "
                       "blocked by its footprint and route around the visible end."),
        15.0F,
        {32.0F, 55.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("detour_troops"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -9.0F},
              6,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("blocking_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 9,
                 {-8.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("detour_troops"));
    move.destination = {0.0F, 0.0F, 9.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("detour_troops")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("detour_troops")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("blocking_wall")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("detour_troops"),
                               {},
                               0.0F,
                               0.0F,
                               3.5F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_wall_breach_id),
        QStringLiteral("Pathfinding: Wall Breach"),
        QStringLiteral("Swordsmen destroy a designated weak wall section, then "
                       "path through the opened gap without crossing intact "
                       "neighbouring segments."),

        26.0F,
        {30.0F, 53.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto breach = building(QStringLiteral("breach_wall"),
                           Game::Units::SpawnType::WallSegment,
                           Nation::Carthage,
                           2,
                           1,
                           {0.0F, 0.0F, 0.0F});
    breach.health_override = breach.max_health_override = 55;
    s.groups = {
        group(QStringLiteral("breachers"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -8.0F},
              8,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("left_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {-10.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
        breach,
        building(QStringLiteral("right_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {4.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto attack = at(0.4F,
                     Command::Attack,
                     QStringLiteral("breachers"),
                     QStringLiteral("breach_wall"));
    auto pass = when_destroyed(QStringLiteral("breach_wall"),
                               Command::FormationMove,
                               QStringLiteral("breachers"),
                               {});
    pass.destination = {0.0F, 0.0F, 8.0F};
    s.steps = {attack, pass};
    add_visual_stability(s, {QStringLiteral("breachers")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("breachers")));
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("breachers")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("breach_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("left_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("right_wall")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("breachers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = pass.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_narrow_gap_column_id),
        QStringLiteral("Pathfinding: Narrow Gap Column"),
        QStringLiteral("A dozen swordsmen ordered through the single opening in a "
                       "long palisade file through it and reform beyond."),
        26.0F,
        {34.0F, 58.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("column"),
              Troop::Swordsman,
              1,
              2,
              {0.0F, 0.0F, -10.0F},
              12,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("west_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 6,
                 {-7.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("east_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 6,
                 {7.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("column"));
    move.destination = {0.0F, 0.0F, 10.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("column")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("west_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("east_wall")));
    auto through = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("column"),
                               {},
                               0.0F,
                               0.0F,
                               6.0F);
    through.position = move.destination;
    s.expectations.push_back(through);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_building_alley_id),
        QStringLiteral("Pathfinding: Building Alley"),
        QStringLiteral("Infantry thread the alley between two barracks rather than "
                       "walking the long way round the block."),
        22.0F,
        {30.0F, 54.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("threaders"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -11.0F},
              6,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("west_block"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {-5.0F, 0.0F, 0.0F}),
        building(QStringLiteral("east_block"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {5.0F, 0.0F, 0.0F}),
        building(QStringLiteral("west_wing"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 5,
                 {-18.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("east_wing"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 5,
                 {10.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("threaders"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("threaders")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("threaders")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("west_block")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("east_block")));
    auto through = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("threaders"),
                               {},
                               0.0F,
                               0.0F,
                               5.0F);
    through.position = move.destination;
    s.expectations.push_back(through);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_diagonal_wall_seal_id),
        QStringLiteral("Pathfinding: Diagonal Wall Seals"),
        QStringLiteral("A palisade set on the diagonal holds: nobody slips between "
                       "two segments that only touch at their corners."),
        20.0F,
        {32.0F, 56.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    s.groups = {group(QStringLiteral("probers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-9.0F, 0.0F, -7.0F},
                      6,
                      {2.4F, 0.0F, 0.0F})};

    for (int index = 0; index < 25; ++index) {
      const float offset = static_cast<float>(index) * 2.0F;
      s.groups.push_back(building(QStringLiteral("diagonal_wall_%1").arg(index),
                                  Game::Units::SpawnType::WallSegment,
                                  Nation::Carthage,
                                  2,
                                  1,
                                  {-24.0F + offset, 0.0F, 24.0F - offset}));
    }
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("probers"));
    move.destination = {10.0F, 0.0F, 10.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("probers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("probers")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("diagonal_wall_12")));
    auto held = expectation(Expect::GroupHeldOutsideDestination,
                            QStringLiteral("probers"),
                            {},
                            0.0F,
                            0.0F,
                            6.0F);
    held.position = move.destination;
    s.expectations.push_back(held);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_bridge_column_id),
        QStringLiteral("Pathfinding: Bridge Column"),
        QStringLiteral("A wide line of infantry funnels onto one narrow bridge, "
                       "crosses on the deck, and reforms on the far bank."),
        26.0F,
        {34.0F, 58.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 6.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 3.5F, 0.45F});
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-12.0F, 0.0F, -12.0F},
                      12,
                      {2.4F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("column"));
    move.destination = {0.0F, 0.0F, 12.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("column")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("column")));
    auto crossed = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("column"),
                               {},
                               0.0F,
                               0.0F,
                               6.0F);
    crossed.position = move.destination;
    s.expectations.push_back(crossed);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_hill_entrance_column_id),
        QStringLiteral("Pathfinding: Hill Entrance Column"),
        QStringLiteral("Spearmen ordered onto a crown they cannot scale directly "
                       "find the cut ramp and climb it in column."),
        24.0F,
        {32.0F, 56.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 0.0F}, 9.0F, 4.0F});
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-6.0F, 0.0F, -14.0F},
                      8,
                      {2.4F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 0.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(
        expectation(Expect::ElevationGainObserved, QStringLiteral("climbers")));
    auto crowned = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               6.0F);
    crowned.position = move.destination;
    s.expectations.push_back(crowned);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_road_junction_showcase_id),
        QStringLiteral("Roads: Junction Showcase"),
        QStringLiteral("A crossroads, a T-junction, a Y-branch, a sharp bend, and two "
                       "closely spaced side turnings in one view, so junction geometry "
                       "can be judged for stacking, seams, and notches."),
        16.0F,
        {34.0F, 55.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    const auto road =
        [](QVector3D start, QVector3D end, float width, const char* style) {
          return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
        };

    s.roads.push_back(road({-16.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 4.0F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, 0.0F}, {16.0F, 0.0F, 0.0F}, 4.0F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 0.0F}, 4.0F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 14.0F}, 4.0F, "default"));

    s.roads.push_back(road({9.0F, 0.0F, 0.0F}, {13.0F, 0.0F, 9.0F}, 3.2F, "default"));
    s.roads.push_back(road({9.0F, 0.0F, 0.0F}, {15.0F, 0.0F, -7.0F}, 3.2F, "default"));

    s.roads.push_back(road({-16.0F, 0.0F, 10.0F}, {-8.0F, 0.0F, 10.0F}, 3.6F, "stone"));
    s.roads.push_back(road({-8.0F, 0.0F, 10.0F}, {-6.0F, 0.0F, 16.0F}, 3.6F, "stone"));
    s.roads.push_back(road({-12.0F, 0.0F, 10.0F}, {-12.0F, 0.0F, 5.0F}, 2.8F, "rough"));
    s.roads.push_back(road({-9.5F, 0.0F, 10.0F}, {-9.5F, 0.0F, 5.5F}, 2.8F, "rough"));
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -11.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("column"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("column")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("column"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_road_slope_showcase_id),
        QStringLiteral("Roads: Slope Showcase"),
        QStringLiteral(
            "One road climbs a rise head-on while a second traverses it "
            "across the fall line and the two cross on the flank, so "
            "terrain-following and slope junctions can be reviewed together."),
        11.0F,
        {34.0F, 50.0F, 22.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 0.0F}, 12.0F, 5.0F});
    const auto road =
        [](QVector3D start, QVector3D end, float width, const char* style) {
          return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
        };
    s.roads.push_back(road({0.0F, 0.0F, -16.0F}, {0.0F, 0.0F, -6.0F}, 4.0F, "stone"));
    s.roads.push_back(road({0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 4.0F, "stone"));
    s.roads.push_back(road({0.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 16.0F}, 4.0F, "stone"));
    s.roads.push_back(road({-16.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 6.0F}, 3.6F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, 6.0F}, {16.0F, 0.0F, 6.0F}, 3.6F, "default"));
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-1.5F, 0.0F, -13.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 0.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(expectation(
        Expect::ElevationGainObserved, QStringLiteral("climbers"), {}, 3.0F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_road_bridge_approach_id),
        QStringLiteral("Roads: Bridge Approach"),
        QStringLiteral(
            "A road runs onto a bridge deck from both banks while the river "
            "keeps flowing underneath, which is the case where the deck used "
            "to sit on filled ground and the approaches stopped short."),
        17.0F,
        {30.0F, 46.0F, 16.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 6.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 6.0F, 0.7F});
    s.roads.push_back(Game::Map::RoadSegment{
        {0.0F, 0.0F, -18.0F}, {0.0F, 0.0F, -6.0F}, 4.0F, QStringLiteral("default")});
    s.roads.push_back(Game::Map::RoadSegment{
        {0.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 18.0F}, 4.0F, QStringLiteral("default")});
    s.groups = {group(QStringLiteral("crossers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -12.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("crossers"));
    move.destination = {0.0F, 0.0F, 12.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("crossers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("crossers")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("crossers")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("crossers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
