#include "arena_gate_scenarios.h"

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

auto build_gate_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {

    auto gate_line = [](ArenaScenarioDefinition& scenario, int gate_owner) {
      scenario.groups.push_back(building(QStringLiteral("west_wall"),
                                         Game::Units::SpawnType::WallSegment,
                                         Nation::RomanRepublic,
                                         gate_owner,
                                         4,
                                         {-7.0F, 0.0F, 0.0F},
                                         {2.0F, 0.0F, 0.0F}));
      scenario.groups.push_back(building(QStringLiteral("east_wall"),
                                         Game::Units::SpawnType::WallSegment,
                                         Nation::RomanRepublic,
                                         gate_owner,
                                         4,
                                         {7.0F, 0.0F, 0.0F},
                                         {2.0F, 0.0F, 0.0F}));
    };

    {
      auto s = definition(
          QString::fromLatin1(k_gate_friendly_passage_id),
          QStringLiteral("Gate: Friendly Passage"),
          QStringLiteral("The owner's infantry approach their own gate, it swings "
                         "open ahead of them, and they march through the wall line."),
          14.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      s.groups.push_back(group(
          QStringLiteral("garrison"), Troop::Swordsman, 1, 1, {-0.5F, 0.0F, -9.0F}, 6));
      auto move = at(0.5F, Command::FormationMove, QStringLiteral("garrison"));
      move.destination = {-0.5F, 0.0F, 9.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("garrison")});
      s.expectations.push_back(
          expectation(Expect::MovementAnimationObserved, QStringLiteral("garrison")));
      s.expectations.push_back(
          expectation(Expect::GateOpenedObserved, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("gate")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("garrison"),
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
          QString::fromLatin1(k_gate_allied_access_id),
          QStringLiteral("Gate: Allied Access"),
          QStringLiteral(
              "A Carthaginian column sharing the wall owner's team is "
              "admitted through the gate on the same terms as its garrison."),
          14.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1},
                       {.owner_id = 2, .team_id = 2},
                       {.owner_id = 3, .team_id = 1}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      auto allies = group(
          QStringLiteral("allies"), Troop::Spearman, 3, 1, {-0.5F, 0.0F, -9.0F}, 6);
      allies.facing_degrees = 0.0F;
      s.groups.push_back(allies);
      auto move = at(0.5F, Command::FormationMove, QStringLiteral("allies"));
      move.destination = {-0.5F, 0.0F, 9.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("allies")});
      s.expectations.push_back(
          expectation(Expect::GateOpenedObserved, QStringLiteral("gate")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("allies"),
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
          QString::fromLatin1(k_gate_enemy_blocked_id),
          QStringLiteral("Gate: Enemy Blocked"),
          QStringLiteral("Hostile infantry walk up to a shut gate, fail to trigger "
                         "it, and are held on their side of the wall."),
          12.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      auto gate = building(QStringLiteral("gate"),
                           Game::Units::SpawnType::WallGate,
                           Nation::RomanRepublic,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F});
      gate.health_override = gate.max_health_override = 6000;
      s.groups.push_back(gate);
      s.groups.push_back(group(
          QStringLiteral("raiders"), Troop::Spearman, 2, 1, {-0.5F, 0.0F, -9.0F}, 6));
      auto move = at(0.5F, Command::FormationMove, QStringLiteral("raiders"));
      move.destination = {-0.5F, 0.0F, 9.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("raiders")});
      s.expectations.push_back(
          expectation(Expect::GateRemainedClosed, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("gate")));
      auto held = expectation(Expect::GroupHeldOutsideDestination,
                              QStringLiteral("raiders"),
                              {},
                              0.0F,
                              0.0F,
                              4.0F);
      held.position = move.destination;
      s.expectations.push_back(held);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_gate_destroyed_breach_id),
          QStringLiteral("Gate: Destroyed Breach"),
          QStringLiteral("Attackers break a gate that will not open for them and "
                         "pour through the breach it leaves in the wall."),

          30.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      auto gate = building(QStringLiteral("gate"),
                           Game::Units::SpawnType::WallGate,
                           Nation::RomanRepublic,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F});
      gate.health_override = gate.max_health_override = 60;
      s.groups.push_back(gate);
      s.groups.push_back(group(QStringLiteral("breachers"),
                               Troop::Swordsman,
                               2,
                               1,
                               {-0.5F, 0.0F, -8.0F},
                               8));
      auto attack = at(
          0.4F, Command::Attack, QStringLiteral("breachers"), QStringLiteral("gate"));
      auto pass = when_destroyed(QStringLiteral("gate"),
                                 Command::FormationMove,
                                 QStringLiteral("breachers"),
                                 {});
      pass.destination = {-0.5F, 0.0F, 8.0F};
      s.steps = {attack, pass};
      add_visual_stability(s, {QStringLiteral("breachers")});
      s.expectations.push_back(
          expectation(Expect::AttackAnimationObserved, QStringLiteral("breachers")));
      s.expectations.push_back(
          expectation(Expect::GroupDestroyed, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("west_wall")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("east_wall")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("breachers"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 3.5F);
      reached.position = pass.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_siege_ram_gate_breach_id),
          QStringLiteral("Siege: Ram Breaches Gate"),
          QStringLiteral("A battering ram is pushed up to a barred gate and splinters "
                         "it open at full strength; the infantry behind it march "
                         "through the breach."),

          60.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      s.groups.push_back(
          group(QStringLiteral("ram"), Troop::Ram, 2, 1, {0.0F, 0.0F, -9.0F}, 1));
      s.groups.push_back(group(QStringLiteral("followers"),
                               Troop::Swordsman,
                               2,
                               1,
                               {-0.5F, 0.0F, -14.0F},
                               8));
      auto attack =
          at(0.4F, Command::Attack, QStringLiteral("ram"), QStringLiteral("gate"));
      auto pass = when_destroyed(QStringLiteral("gate"),
                                 Command::FormationMove,
                                 QStringLiteral("followers"),
                                 {});
      pass.destination = {-0.5F, 0.0F, 8.0F};
      s.steps = {attack, pass};
      s.expectations.push_back(
          expectation(Expect::GroupDestroyed, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("west_wall")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("east_wall")));
      s.expectations.push_back(expectation(Expect::GroupExists, QStringLiteral("ram")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("followers"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 3.5F);
      reached.position = pass.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_siege_tower_wall_assault_id),
          QStringLiteral("Siege: Tower Assaults Wall"),
          QStringLiteral("A siege tower rolls against an enemy wall with a company "
                         "walking behind it; when the ramp drops the company climbs "
                         "the tower and crosses onto the wall-top walkway."),

          40.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(-4.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      s.groups.push_back(group(
          QStringLiteral("tower"), Troop::SiegeTower, 2, 1, {-4.0F, 0.0F, -9.0F}, 1));
      s.groups.push_back(group(
          QStringLiteral("escort"), Troop::Swordsman, 2, 1, {-4.0F, 0.0F, -14.0F}, 10));
      s.steps = {at(0.4F, Command::Move, QStringLiteral("tower"))};
      s.steps.back().destination = {-4.0F, 0.0F, -3.0F};
      auto follow = at(0.4F, Command::Move, QStringLiteral("escort"));
      follow.destination = {-4.0F, 0.0F, -8.0F};
      s.steps.push_back(follow);
      s.expectations.push_back(
          expectation(Expect::SiegeTowerDocked, QStringLiteral("tower")));
      s.expectations.push_back(
          expectation(Expect::WallWalkerObserved, QStringLiteral("tower")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("west_wall")));
      result.push_back(std::move(s));
    }

    auto walled_town = [](ArenaScenarioDefinition& scenario) {
      scenario.groups.push_back(building(QStringLiteral("town_wall"),
                                         Game::Units::SpawnType::WallSegment,
                                         Nation::RomanRepublic,
                                         1,
                                         11,
                                         {0.0F, 0.0F, 0.0F},
                                         {2.0F, 0.0F, 0.0F}));
      scenario.groups.push_back(building(QStringLiteral("town_barracks"),
                                         Game::Units::SpawnType::Barracks,
                                         Nation::RomanRepublic,
                                         1,
                                         1,
                                         {2.0F, 0.0F, 11.0F}));
    };

    {
      auto s = definition(
          QString::fromLatin1(k_wall_walk_garrison_id),
          QStringLiteral("Wall Walk: Garrison Mounts the Balcony"),
          QStringLiteral("A town's infantry is ordered onto its palisade: it walks to "
                         "the nearest stair, climbs to the balcony on the town face "
                         "and files along it, then comes back down to the street."),
          38.0F,
          {15.0F, 34.0F, 20.0F});
      s.camera_focus = QVector3D(1.0F, 0.0F, 1.5F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      walled_town(s);
      s.groups.push_back(group(
          QStringLiteral("garrison"), Troop::Swordsman, 1, 1, {-3.0F, 0.0F, 7.0F}, 10));
      auto climb = at(0.5F, Command::Move, QStringLiteral("garrison"));
      climb.destination = {3.0F, 0.0F, 0.45F};
      auto descend = at(26.0F, Command::Move, QStringLiteral("garrison"));
      descend.destination = {-4.0F, 0.0F, 7.0F};
      s.steps = {climb, descend};
      s.expectations.push_back(
          expectation(Expect::WallWalkerObserved, QStringLiteral("garrison")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("town_wall")));
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_wall_walk_ladder_climb_id),
          QStringLiteral("Wall Walk: Ladder Climb"),
          QStringLiteral("A builder's ladder leans on the town face of the palisade "
                         "between two stairs. A company ordered onto that stretch "
                         "climbs it hand over hand, one man after another, and later "
                         "comes back down the same way."),
          36.0F,
          {14.0F, 30.0F, 16.0F});
      s.camera_focus = QVector3D(6.0F, 1.0F, 1.5F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      walled_town(s);
      s.groups.push_back(building(QStringLiteral("ladder"),
                                  Game::Units::SpawnType::WallLadder,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {6.0F, 0.0F, 1.175F},
                                  {0.0F, 0.0F, 0.0F},
                                  180.0F));
      s.groups.push_back(group(
          QStringLiteral("climbers"), Troop::Swordsman, 1, 1, {6.0F, 0.0F, 6.5F}, 8));
      auto climb = at(0.5F, Command::Move, QStringLiteral("climbers"));
      climb.destination = {6.0F, 0.0F, 0.45F};
      auto descend = at(24.0F, Command::Move, QStringLiteral("climbers"));
      descend.destination = {6.0F, 0.0F, 6.5F};
      s.steps = {climb, descend};
      s.expectations.push_back(
          expectation(Expect::WallWalkerObserved, QStringLiteral("climbers")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("ladder")));
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_siege_tower_balcony_assault_id),
          QStringLiteral("Siege: Tower Storms the Balcony"),
          QStringLiteral("A siege tower is pushed against a town palisade, drops its "
                         "bridge onto the stakes and its company files over the "
                         "crest onto the defenders' balcony, where the garrison that "
                         "climbed up to meet it is waiting."),
          50.0F,
          {26.0F, 34.0F, -20.0F});
      s.camera_focus = QVector3D(-2.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      walled_town(s);
      s.groups.push_back(group(
          QStringLiteral("garrison"), Troop::Swordsman, 1, 1, {4.0F, 0.0F, 7.0F}, 8));
      s.groups.push_back(group(
          QStringLiteral("tower"), Troop::SiegeTower, 2, 1, {-4.0F, 0.0F, -12.0F}, 1));
      s.groups.push_back(group(
          QStringLiteral("escort"), Troop::Swordsman, 2, 1, {-4.0F, 0.0F, -17.0F}, 10));
      auto man = at(0.5F, Command::Move, QStringLiteral("garrison"));
      man.destination = {2.0F, 0.0F, 0.45F};
      auto push = at(0.5F, Command::Move, QStringLiteral("tower"));
      push.destination = {-4.0F, 0.0F, -3.0F};
      auto follow = at(0.5F, Command::Move, QStringLiteral("escort"));
      follow.destination = {-4.0F, 0.0F, -8.0F};
      s.steps = {man, push, follow};
      s.expectations.push_back(
          expectation(Expect::SiegeTowerDocked, QStringLiteral("tower")));
      s.expectations.push_back(
          expectation(Expect::WallWalkerObserved, QStringLiteral("tower")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("town_wall")));
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_gate_consecutive_transit_id),
          QStringLiteral("Gate: Consecutive Transit"),
          QStringLiteral("Three files cross the same gate back to back; it must "
                         "stay open under them and never shut on a body."),
          20.0F,
          {30.0F, 54.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      s.groups.push_back(group(QStringLiteral("column"),
                               Troop::Swordsman,
                               1,
                               3,
                               {-0.5F, 0.0F, -6.0F},
                               4,
                               {0.0F, 0.0F, -2.5F}));
      auto move = at(0.5F, Command::Move, QStringLiteral("column"));
      move.destination = {-0.5F, 0.0F, 8.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("column")});
      s.expectations.push_back(
          expectation(Expect::GateOpenedObserved, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("column"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 4.0F);
      reached.position = move.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }
  }

  {
    auto s = definition(
        QString::fromLatin1(k_crossing_formations_id),
        QStringLiteral("Crossing Formations"),
        QStringLiteral("Two friendly groups cross without formation collapse."),
        10.0F,
        {24.0F, 52.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("left"), Troop::Swordsman, 1, 3, {-7.0F, 0.0F, -5.0F}),
        group(QStringLiteral("right"), Troop::Spearman, 1, 3, {7.0F, 0.0F, 5.0F})};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("left")),
               at(0.5F, Command::FormationMove, QStringLiteral("right"))};
    s.steps[0].destination = {7.0F, 0.0F, 5.0F};
    s.steps[1].destination = {-7.0F, 0.0F, -5.0F};
    add_visual_stability(s, {QStringLiteral("left"), QStringLiteral("right")});
    s.expectations.push_back(
        expectation(Expect::FormationOrderPreserved, QStringLiteral("left"), {}, 1.0F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("right"), {}, 1.0F));
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_fog_of_war_recon_id),
        QStringLiteral("Fog of War Recon"),
        QStringLiteral(
            "A patrol crosses the map and returns. Exercises exploration, loss "
            "of sight over ground already walked, and enemies that appear and "
            "vanish with the patrol's vision."),
        34.0F,
        {58.0F, 58.0F, 30.0F});
    s.groups = {
        group(
            QStringLiteral("patrol"), Troop::Swordsman, 1, 2, {-16.0F, 0.0F, 14.0F}, 8),
        group(QStringLiteral("camp_guards"),
              Troop::Spearman,
              2,
              2,
              {16.0F, 0.0F, -14.0F},
              8)};

    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("patrol")),
               at(6.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(12.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(18.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(24.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(29.0F, Command::FormationMove, QStringLiteral("patrol"))};
    s.steps[0].destination = {-9.0F, 0.0F, 8.0F};
    s.steps[1].destination = {-2.0F, 0.0F, 1.0F};
    s.steps[2].destination = {6.0F, 0.0F, -5.0F};
    s.steps[3].destination = {12.0F, 0.0F, -10.0F};
    s.steps[4].destination = {2.0F, 0.0F, -1.0F};
    s.steps[5].destination = {-11.0F, 0.0F, 9.0F};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("patrol")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("camp_guards")));
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("patrol")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.5F));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
