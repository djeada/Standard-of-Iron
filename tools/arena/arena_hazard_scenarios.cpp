#include "arena_hazard_scenarios.h"

#include <QVector3D>

#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_features.h"
#include "game/systems/nation_registry.h"
#include "game/units/troop_type.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Trigger = ScenarioTriggerKind;
using Game::Map::RockfallTriggerMode;

constexpr float k_ridge_offset = 12.0F;
constexpr float k_march_to = 22.0F;

auto alpine_pass(const char* id,
                 const char* label,
                 const char* description,
                 float duration) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = QString::fromLatin1(id);
  result.label = QString::fromLatin1(label);
  result.description = QString::fromLatin1(description);
  result.duration_seconds = duration;
  result.camera = {46.0F, 58.0F, 0.0F};
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.terrain_grid_extent = 64;
  result.arena_floor_half_extent = 30.0F;
  result.ground_type = QStringLiteral("alpine_mix");
  result.terrain_snowbound = true;
  result.suppress_terrain_scatter = true;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_boundary_mountains = true;

  for (float side : {-1.0F, 1.0F}) {
    for (float x = -28.0F; x <= 28.0F; x += 7.0F) {
      Game::Map::TerrainFeature ridge;
      ridge.type = Game::Map::TerrainType::Mountain;
      ridge.center_x = x;
      ridge.center_z = side * k_ridge_offset;
      ridge.radius = 10.0F;
      ridge.height = 14.0F;
      result.terrain_features.push_back(ridge);
    }
  }
  return result;
}

auto column() -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = QStringLiteral("column");
  result.troop_type = Game::Units::TroopType::Spearman;
  result.nation_id = Game::Systems::NationID::Carthage;
  result.owner_id = 1;
  result.count = 3;
  result.individuals_per_unit = 16;
  result.origin = QVector3D(-12.0F, 0.0F, 0.0F);
  result.spacing = QVector3D(-6.5F, 0.0F, 0.0F);
  result.facing_degrees = 90.0F;
  return result;
}

auto march() -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("march");
  step.trigger = {Trigger::AtTime, 0.5F, {}, {}, 0.0F};
  step.command = Command::FormationMove;
  step.group = QStringLiteral("column");
  step.destination = QVector3D(k_march_to, 0.0F, 0.0F);
  return step;
}

auto trap(const char* id,
          QVector3D release,
          QVector3D target,
          RockfallTriggerMode trigger,
          int owner_id) -> Game::Map::RockfallTrap {
  Game::Map::RockfallTrap result;
  result.id = QString::fromLatin1(id);
  result.release_x = release.x();
  result.release_z = release.z();
  result.target_x = target.x();
  result.target_z = target.z();
  result.trigger = trigger;
  result.owner_id = owner_id;
  result.zone_radius = 6.0F;
  result.boulder_count = 6;
  result.boulder_radius = 0.6F;
  result.release_spread = 7.0F;
  result.release_interval = 0.3F;
  return result;
}

auto expectation(Expect kind, float threshold = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = QStringLiteral("column");
  result.threshold = threshold;
  return result;
}

constexpr float k_hill_x = 6.0F;
const QVector3D k_cache_post(3.0F, 0.0F, 1.5F);
const QVector3D k_ramp_middle(-8.5F, 0.0F, 0.5F);

auto hill_ramp(const char* id,
               const char* label,
               const char* description,
               float duration) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = QString::fromLatin1(id);
  result.label = QString::fromLatin1(label);
  result.description = QString::fromLatin1(description);
  result.duration_seconds = duration;
  result.camera = {44.0F, 46.0F, 300.0F};
  result.camera_focus = QVector3D(-6.0F, 0.0F, 0.0F);
  result.terrain_grid_extent = 100;
  result.arena_floor_half_extent = 36.0F;
  result.ground_type = QStringLiteral("soil_rocky");
  result.suppress_terrain_scatter = true;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_boundary_mountains = true;

  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = k_hill_x;
  hill.center_z = 0.0F;
  hill.radius = 12.0F;
  hill.height = 9.0F;
  hill.entrances.push_back(QVector3D(k_hill_x - 13.0F, 0.0F, 0.0F));
  result.terrain_features.push_back(hill);
  return result;
}

auto troop(const char* name,
           Game::Units::TroopType type,
           int owner_id,
           int count,
           QVector3D origin,
           float facing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = QString::fromLatin1(name);
  result.troop_type = type;
  result.nation_id = owner_id == 1 ? Game::Systems::NationID::Carthage
                                   : Game::Systems::NationID::RomanRepublic;
  result.owner_id = owner_id;
  result.count = count;
  result.individuals_per_unit = 12;
  result.origin = origin;
  result.spacing = QVector3D(-6.0F, 0.0F, 0.0F);
  result.facing_degrees = facing;
  return result;
}

auto order_move(const char* name,
                const char* group,
                float at,
                QVector3D to) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QString::fromLatin1(name);
  step.trigger = {Trigger::AtTime, at, {}, {}, 0.0F};
  step.command = Command::Move;
  step.group = QString::fromLatin1(group);
  step.destination = to;
  return step;
}

constexpr float k_raft_river_width = 10.0F;
constexpr float k_raft_bank_z = 16.0F;

auto river_crossing(const char* id,
                    const char* label,
                    const char* description,
                    float duration) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = QString::fromLatin1(id);
  result.label = QString::fromLatin1(label);
  result.description = QString::fromLatin1(description);
  result.duration_seconds = duration;
  result.camera = {40.0F, 52.0F, 300.0F};
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.terrain_grid_extent = 100;
  result.arena_floor_half_extent = 36.0F;
  result.ground_type = QStringLiteral("soil_rocky");
  result.suppress_terrain_scatter = true;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_boundary_mountains = true;
  result.rivers.push_back(Game::Map::RiverSegment{
      {-44.0F, 0.0F, 0.0F}, {44.0F, 0.0F, 0.0F}, k_raft_river_width});
  Game::Map::RaftCrossing raft;
  raft.id = QStringLiteral("ferry");
  result.rafts = {raft};
  return result;
}

auto order_raft(const char* group, float at) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = QStringLiteral("cross_by_raft");
  step.trigger = {Trigger::AtTime, at, {}, {}, 0.0F};
  step.command = Command::CrossByRaft;
  step.group = QString::fromLatin1(group);
  return step;
}

void expect_readable_casualties(ArenaScenarioDefinition& scenario) {
  scenario.expectations.push_back(expectation(Expect::AllGroupsRespondWithin, 2.5F));
  scenario.expectations.push_back(expectation(Expect::GroupHealthReduced, 1.0F));
  scenario.expectations.push_back(expectation(Expect::DeathAnimationObserved));
  scenario.expectations.push_back(expectation(Expect::LaunchedCasualtyObserved));
}

} // namespace

auto build_hazard_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto scenario = alpine_pass(
        k_rockfall_alpine_pass_id,
        "Rockfall: Ambush in the Alpine Pass",
        "A column marches through a gorge. Tribesmen on the northern heights "
        "roll boulders down onto it as it passes, and a second cache on the "
        "southern slope tips over when the column reaches it.",
        26.0F);
    scenario.groups = {column()};
    scenario.rockfall_traps = {
        trap("north_heights",
             {-3.0F, 0.0F, -k_ridge_offset + 3.0F},
             {1.0F, 0.0F, 0.0F},
             RockfallTriggerMode::Scripted,
             2),
        trap("south_cache",
             {9.0F, 0.0F, k_ridge_offset - 3.0F},
             {9.0F, 0.0F, 0.0F},
             RockfallTriggerMode::Zone,
             2),
    };
    scenario.rockfall_traps[1].boulder_count = 4;
    scenario.rockfall_traps[1].release_spread = 4.0F;
    scenario.rockfall_traps[1].zone_radius = 3.5F;

    ArenaScenarioStep ambush;
    ambush.name = QStringLiteral("ambush");

    ambush.trigger = {
        Trigger::GroupEnteredArea, 0.0F, QStringLiteral("column"), {}, 2.0F};
    ambush.trigger.position = QVector3D(-2.0F, 0.0F, 0.0F);
    ambush.command = Command::TriggerRockfall;
    ambush.zone_id = QStringLiteral("north_heights");
    scenario.steps = {march(), ambush};
    expect_readable_casualties(scenario);
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = alpine_pass(
        k_rockfall_ai_defenders_id,
        "Rockfall: AI Defenders Hold the Heights",
        "The heights belong to an AI. It waits until more than one troop of "
        "the column is in the gorge below before it lets the rocks go.",
        26.0F);
    scenario.groups = {column()};
    auto defenders = trap("allobroges",
                          {0.0F, 0.0F, -k_ridge_offset + 3.0F},
                          {3.0F, 0.0F, 0.0F},
                          RockfallTriggerMode::AiDefender,
                          2);
    defenders.zone_radius = 9.0F;
    defenders.ai_min_targets = 2;
    scenario.rockfall_traps = {defenders};
    scenario.steps = {march()};
    expect_readable_casualties(scenario);
    result.push_back(std::move(scenario));
  }

  {
    auto scenario =
        hill_ramp(k_rockfall_hill_ramp_id,
                  "Rockfall: Roll the Stones Down the Ramp",
                  "Spearmen hold the top of a hill path, beside the stone cache staged "
                  "there. A column climbs the ramp, and the defenders roll the stones "
                  "onto it.",
                  30.0F);
    scenario.groups = {
        troop(
            "defenders", Game::Units::TroopType::Spearman, 1, 1, k_cache_post, 270.0F),
        troop("column",
              Game::Units::TroopType::Swordsman,
              2,
              2,
              QVector3D(-30.0F, 0.0F, 0.0F),
              90.0F)};
    ArenaScenarioStep roll;
    roll.name = QStringLiteral("roll");
    roll.trigger = {
        Trigger::GroupEnteredArea, 0.0F, QStringLiteral("column"), {}, 5.0F};
    roll.trigger.position = k_ramp_middle;
    roll.command = Command::RollStones;
    roll.group = QStringLiteral("defenders");
    scenario.steps = {
        order_move("climb", "column", 1.0F, QVector3D(k_hill_x, 0.0F, 0.0F)), roll};
    scenario.expectations.push_back(expectation(Expect::GroupHealthReduced, 1.0F));
    scenario.expectations.push_back(expectation(Expect::DeathAnimationObserved));
    scenario.expectations.push_back(expectation(Expect::LaunchedCasualtyObserved));
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = hill_ramp(
        k_rockfall_hill_ai_id,
        "Rockfall: The AI Rolls Its Stones",
        "The AI's troops reach the stones at the top of the hill path first. "
        "When the player's column climbs the ramp, the AI rolls them on its own.",
        30.0F);
    scenario.groups = {
        troop(
            "defenders", Game::Units::TroopType::Spearman, 2, 1, k_cache_post, 270.0F),
        troop("column",
              Game::Units::TroopType::Swordsman,
              1,
              2,
              QVector3D(-30.0F, 0.0F, 0.0F),
              90.0F)};
    scenario.steps = {
        order_move("climb", "column", 1.0F, QVector3D(k_hill_x, 0.0F, 0.0F))};
    scenario.expectations.push_back(expectation(Expect::GroupHealthReduced, 1.0F));
    scenario.expectations.push_back(expectation(Expect::LaunchedCasualtyObserved));
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = river_crossing(
        k_raft_crossing_id,
        "Rafts: Ferrying an Army Across",
        "Three troops line up at a raft on a wide river. The raft carries one "
        "troop at a time to the far bank and comes back empty for the next.",
        48.0F);
    auto crossers = troop("crossers",
                          Game::Units::TroopType::Swordsman,
                          1,
                          3,
                          QVector3D(6.0F, 0.0F, -k_raft_bank_z),
                          0.0F);
    scenario.groups = {crossers};
    scenario.steps = {order_raft("crossers", 1.0F)};
    scenario.expectations.push_back(expectation(Expect::RaftFerryObserved, 0.0F));
    scenario.expectations.back().group = QStringLiteral("crossers");
    scenario.expectations.push_back(expectation(Expect::GroupHealthUnchanged));
    scenario.expectations.back().group = QStringLiteral("crossers");
    result.push_back(std::move(scenario));
  }

  {
    auto scenario = river_crossing(
        k_raft_contested_crossing_id,
        "Rafts: Contested Crossing",
        "Troops ferry across a wide river by raft while archers on the far "
        "bank shoot at every boat-load. Men afloat cannot close ranks, so "
        "the arrows bite harder on the water.",
        55.0F);
    scenario.groups = {troop("crossers",
                             Game::Units::TroopType::Swordsman,
                             1,
                             3,
                             QVector3D(6.0F, 0.0F, -k_raft_bank_z),
                             0.0F),
                       troop("bank_archers",
                             Game::Units::TroopType::Archer,
                             2,
                             2,
                             QVector3D(5.0F, 0.0F, 3.5F),
                             180.0F)};
    scenario.groups[1].spacing = QVector3D(-10.0F, 0.0F, 0.0F);
    scenario.steps = {order_raft("crossers", 1.0F)};
    scenario.expectations.push_back(expectation(Expect::RaftFerryObserved));
    scenario.expectations.back().group = QStringLiteral("crossers");
    scenario.expectations.push_back(expectation(Expect::GroupHealthReduced, 1.0F));
    scenario.expectations.back().group = QStringLiteral("crossers");
    result.push_back(std::move(scenario));
  }

  return result;
}

} // namespace Arena::Scenarios
