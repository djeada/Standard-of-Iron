#include "arena_rpg_friendly_scenarios.h"

#include <optional>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

} // namespace

auto build_rpg_friendly_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_pass_ranks_id),
        QStringLiteral("RPG Pass Ranks"),
        QStringLiteral(
            "Behind-head commander walks the length of a friendly line and then "
            "back through it. Only the individual bodies that stand between the "
            "lens and the commander may drop out: a rank the player walks past "
            "has to stay on screen instead of the whole unit blinking away as "
            "soon as its anchor enters the gap."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {-6.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 90.0F;

    auto line = group(QStringLiteral("marching_line"),
                      Troop::Spearman,
                      1,
                      3,
                      {-3.0F, 0.0F, 1.2F},
                      8,
                      {3.4F, 0.0F, 0.0F});
    line.facing_degrees = 180.0F;
    s.groups = {commander, line};

    auto move = [](float time, QVector3D axes, std::optional<float> yaw = {}) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      step.rpg_view_yaw_degrees = yaw;
      return step;
    };
    s.steps = {

        move(0.30F, {0.0F, 0.0F, 1.0F}, 90.0F),

        move(4.20F, {0.0F, 0.0F, 1.0F}, 0.0F),

        move(6.40F, {0.0F, 0.0F, 1.0F}, 270.0F),
        move(8.40F, {0.0F, 0.0F, 0.0F}),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("marching_line")});
    s.expectations.push_back(expectation(Expect::RpgFormationSurvivesLensGap,
                                         QStringLiteral("marching_line"),
                                         {},
                                         0.5F));
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("marching_line")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  auto friendly_crossing_commander = [](QVector3D origin) {
    auto commander = group(
        QStringLiteral("rpg_commander"), Troop::RomanVeteranConsul, 1, 1, origin, 1);
    commander.facing_degrees = 0.0F;
    return commander;
  };

  auto walk_north = [](float time, std::optional<float> yaw = {}) {
    auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
    step.destination = {0.0F, 0.0F, 1.0F};
    step.value = 0;
    step.rpg_view_yaw_degrees = yaw;
    return step;
  };

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_friendly_ranks_id),
        QStringLiteral("RPG Friendly Ranks Crossing"),
        QStringLiteral(
            "Behind-head commander walks the length of his own three-deep infantry "
            "block on open ground. Friendly bodies are traffic: they may deflect "
            "him and slow him locally, but the block must not behave like a wall. "
            "The travel contract is the whole scenario -- eleven metres of the "
            "fourteen a clear walk would cover in the same window."),
        11.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto rank_a = group(QStringLiteral("rank_a"),
                        Troop::Spearman,
                        1,
                        3,
                        {-3.0F, 0.0F, -1.0F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    rank_a.facing_degrees = 180.0F;
    auto rank_b = group(QStringLiteral("rank_b"),
                        Troop::Swordsman,
                        1,
                        3,
                        {-3.0F, 0.0F, 1.6F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    rank_b.facing_degrees = 180.0F;
    auto rank_c = group(QStringLiteral("rank_c"),
                        Troop::Spearman,
                        1,
                        3,
                        {-3.0F, 0.0F, 4.2F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    rank_c.facing_degrees = 180.0F;
    s.groups = {
        friendly_crossing_commander({0.0F, 0.0F, -7.0F}), rank_a, rank_b, rank_c};

    s.steps = {walk_north(0.30F, 0.0F), stop_moving(10.40F)};
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    auto travel = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 11.0F, 0.40F);
    travel.end_seconds = 10.40F;
    s.expectations.push_back(travel);
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::UnitsStayOnWalkableGround));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_friendly_workers_id),
        QStringLiteral("RPG Friendly Workers Crossing"),
        QStringLiteral(
            "The same crossing through builders and civilians instead of soldiers. "
            "Worker bodies stand still far more often than soldiers do, and a body "
            "standing still is exactly the case that used to anchor the pair and "
            "refuse the commander the ground."),
        11.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto builders = group(QStringLiteral("work_party"),
                          Troop::Builder,
                          1,
                          5,
                          {-3.2F, 0.0F, -0.8F},
                          1,
                          {1.6F, 0.0F, 0.0F});
    auto civilians = group(QStringLiteral("townsfolk"),
                           Troop::Civilian,
                           1,
                           5,
                           {-3.2F, 0.0F, 1.4F},
                           1,
                           {1.6F, 0.0F, 0.0F});
    auto second_row = group(QStringLiteral("townsfolk_rear"),
                            Troop::Civilian,
                            1,
                            5,
                            {-2.4F, 0.0F, 3.6F},
                            1,
                            {1.6F, 0.0F, 0.0F});
    s.groups = {friendly_crossing_commander({0.0F, 0.0F, -7.0F}),
                builders,
                civilians,
                second_row};

    s.steps = {walk_north(0.30F, 0.0F), stop_moving(10.40F)};
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    auto travel = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 11.0F, 0.40F);
    travel.end_seconds = 10.40F;
    s.expectations.push_back(travel);
    s.expectations.push_back(expectation(Expect::UnitsStayOnWalkableGround));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_friendly_livestock_id),
        QStringLiteral("RPG Friendly Livestock Crossing"),
        QStringLiteral(
            "Mixed humans and animals on the same ground. Wildlife is a dynamic "
            "body like any other and follows the same rule: an animal in the way "
            "is something to flow around, not a blocker, unless a gameplay rule "
            "says otherwise."),
        11.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    s.wildlife = Game::Wildlife::default_settings();
    s.wildlife.enabled = true;
    s.wildlife.seed = 20260906U;
    s.wildlife.wolves.enabled = false;
    s.wildlife.wolves.group_count = 0;
    s.wildlife.birds.enabled = false;
    s.wildlife.birds.group_count = 0;
    s.wildlife.sheep.enabled = true;
    s.wildlife.sheep.group_count = 1;
    s.wildlife.sheep.group_size_min = 6;
    s.wildlife.sheep.group_size_max = 6;
    s.wildlife.sheep.roam_radius = 2.5F;
    s.wildlife.sheep.spawn_areas = {{0.0F, 1.5F, 2.5F}};

    auto escort = group(QStringLiteral("escort"),
                        Troop::Swordsman,
                        1,
                        3,
                        {-3.0F, 0.0F, -1.2F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    escort.facing_degrees = 180.0F;
    s.groups = {friendly_crossing_commander({0.0F, 0.0F, -7.0F}), escort};

    s.steps = {walk_north(0.30F, 0.0F), stop_moving(10.40F)};
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    auto travel = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 11.0F, 0.40F);
    travel.end_seconds = 10.40F;
    s.expectations.push_back(travel);
    s.expectations.push_back(expectation(Expect::UnitsStayOnWalkableGround));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_friendly_stream_id),
        QStringLiteral("RPG Friendly Stream Crossing"),
        QStringLiteral(
            "Two friendly columns march across the commander's path in opposite "
            "directions while he walks straight through the seam between them. "
            "Moving bodies share the correction with him; the crossing has to stay "
            "possible in both directions at once."),
        12.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto eastbound = group(QStringLiteral("eastbound"),
                           Troop::Swordsman,
                           1,
                           3,
                           {-9.0F, 0.0F, -1.0F},
                           8,
                           {3.0F, 0.0F, 0.0F});
    eastbound.facing_degrees = 90.0F;
    auto westbound = group(QStringLiteral("westbound"),
                           Troop::Spearman,
                           1,
                           3,
                           {9.0F, 0.0F, 2.4F},
                           8,
                           {-3.0F, 0.0F, 0.0F});
    westbound.facing_degrees = 270.0F;
    s.groups = {friendly_crossing_commander({0.0F, 0.0F, -7.0F}), eastbound, westbound};

    auto march = [](float time, QString name, QVector3D destination) {
      auto step = at(time, Command::Move, std::move(name));
      step.destination = destination;
      return step;
    };
    s.steps = {
        walk_north(0.30F, 0.0F),
        march(0.30F, QStringLiteral("eastbound"), {9.0F, 0.0F, -1.0F}),
        march(0.30F, QStringLiteral("westbound"), {-9.0F, 0.0F, 2.4F}),
        stop_moving(11.40F),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    auto travel = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 11.0F, 0.40F);
    travel.end_seconds = 11.40F;
    s.expectations.push_back(travel);
    s.expectations.push_back(expectation(Expect::UnitsStayOnWalkableGround));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_friendly_gauntlet_id),
        QStringLiteral("RPG Friendly Gauntlet"),
        QStringLiteral(
            "Friendly bodies packed into the gap between two houses. Both kinds of "
            "blocker are present at once and they must not be confused: the houses "
            "stop the commander exactly as they do under an RTS order, and the "
            "soldiers between them do not."),
        12.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto west_home = building(QStringLiteral("gauntlet_west"),
                              Game::Units::SpawnType::Home,
                              Nation::RomanRepublic,
                              1,
                              1,
                              {-5.0F, 0.0F, 0.0F});
    west_home.health_override = west_home.max_health_override = 4000;
    auto east_home = building(QStringLiteral("gauntlet_east"),
                              Game::Units::SpawnType::Home,
                              Nation::RomanRepublic,
                              1,
                              1,
                              {5.0F, 0.0F, 0.0F});
    east_home.health_override = east_home.max_health_override = 4000;
    auto plug = group(QStringLiteral("gauntlet_plug"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.2F, 0.0F, 0.0F},
                      8,
                      {2.4F, 0.0F, 0.0F});
    plug.facing_degrees = 180.0F;
    s.groups = {
        friendly_crossing_commander({0.0F, 0.0F, -7.0F}), west_home, east_home, plug};

    s.steps = {walk_north(0.30F, 0.0F), stop_moving(11.40F)};
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    auto travel = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 10.0F, 0.40F);
    travel.end_seconds = 11.40F;
    s.expectations.push_back(travel);
    s.expectations.push_back(expectation(Expect::UnitsStayOnWalkableGround));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rts_friendly_ranks_id),
        QStringLiteral("RTS Friendly Ranks Crossing"),
        QStringLiteral(
            "The rpg_friendly_ranks layout under an ordinary move order. This is "
            "the control: the same commander, the same start, the same friendly "
            "block, the same destination, steered by the route follower instead of "
            "by a player. If one of the two passes and the other does not, the "
            "modes are not sharing a traversal model."),
        11.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto rank_a = group(QStringLiteral("rank_a"),
                        Troop::Spearman,
                        1,
                        3,
                        {-3.0F, 0.0F, -1.0F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    rank_a.facing_degrees = 180.0F;
    auto rank_b = group(QStringLiteral("rank_b"),
                        Troop::Swordsman,
                        1,
                        3,
                        {-3.0F, 0.0F, 1.6F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    rank_b.facing_degrees = 180.0F;
    auto rank_c = group(QStringLiteral("rank_c"),
                        Troop::Spearman,
                        1,
                        3,
                        {-3.0F, 0.0F, 4.2F},
                        8,
                        {3.0F, 0.0F, 0.0F});
    rank_c.facing_degrees = 180.0F;
    s.groups = {
        friendly_crossing_commander({0.0F, 0.0F, -7.0F}), rank_a, rank_b, rank_c};

    auto order = at(0.30F, Command::Move, QStringLiteral("rpg_commander"));
    order.destination = {0.0F, 0.0F, 7.0F};
    s.steps = {order};

    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("rpg_commander"),
                               {},
                               0.0F,
                               0.0F,
                               1.5F);
    reached.position = order.destination;
    s.expectations.push_back(reached);
    s.expectations.push_back(expectation(Expect::UnitsStayOnWalkableGround));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
