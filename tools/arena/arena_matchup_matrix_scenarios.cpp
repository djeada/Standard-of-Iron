#include "arena_matchup_matrix_scenarios.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using SpawnType = Game::Units::SpawnType;
using Troop = Game::Units::TroopType;

enum class Body : std::uint8_t {
  Humanoid,
  Beast,
  Siege,
  Building,
  Wolves,
};

struct Side {
  const char* key;
  Body body;
  Troop troop;
  std::optional<SpawnType> structure;
  int individuals;
  bool fights;
  bool kills_wolves_before_they_bite{false};
};

constexpr Side k_wolves{"wolves", Body::Wolves, Troop::Wolf, std::nullopt, 0, true};
constexpr Side k_builder{
    "builder", Body::Humanoid, Troop::Builder, std::nullopt, 0, true};
constexpr Side k_civilian{
    "civilian", Body::Humanoid, Troop::Civilian, std::nullopt, 0, true};
constexpr Side k_healer{"healer", Body::Humanoid, Troop::Healer, std::nullopt, 0, true};
constexpr Side k_swordsman{
    "swordsman", Body::Humanoid, Troop::Swordsman, std::nullopt, 0, true};
constexpr Side k_spearman{
    "spearman", Body::Humanoid, Troop::Spearman, std::nullopt, 0, true};
constexpr Side k_archer{"archer", Body::Humanoid, Troop::Archer, std::nullopt, 0, true};
constexpr Side k_horse_archer{
    "horse_archer", Body::Humanoid, Troop::HorseArcher, std::nullopt, 0, true};
constexpr Side k_mounted_swordsman{"mounted_swordsman",
                                   Body::Humanoid,
                                   Troop::MountedSwordsman,
                                   std::nullopt,
                                   0,
                                   true};
constexpr Side k_horse_spearman{
    "horse_spearman", Body::Humanoid, Troop::HorseSpearman, std::nullopt, 0, true};
constexpr Side k_commander{"commander",
                           Body::Humanoid,
                           Troop::RomanVeteranConsul,
                           std::nullopt,
                           1,
                           true,
                           true};
constexpr Side k_elephant{
    "elephant", Body::Beast, Troop::Elephant, std::nullopt, 1, true};
constexpr Side k_ram{"ram", Body::Siege, Troop::Ram, std::nullopt, 1, false};
constexpr Side k_catapult{
    "catapult", Body::Siege, Troop::Catapult, std::nullopt, 1, false};
constexpr Side k_ballista{
    "ballista", Body::Siege, Troop::Ballista, std::nullopt, 1, false};
constexpr Side k_siege_tower{
    "siege_tower", Body::Siege, Troop::SiegeTower, std::nullopt, 1, false};
constexpr Side k_home{
    "home", Body::Building, Troop::Swordsman, SpawnType::Home, 0, false};
constexpr Side k_barracks{
    "barracks", Body::Building, Troop::Swordsman, SpawnType::Barracks, 0, false};

struct Matchup {
  Side attacker;
  Side defender;
};

constexpr std::array k_matchups{
    Matchup{k_wolves, k_builder},
    Matchup{k_wolves, k_civilian},
    Matchup{k_wolves, k_healer},
    Matchup{k_wolves, k_swordsman},
    Matchup{k_wolves, k_spearman},
    Matchup{k_wolves, k_archer},
    Matchup{k_wolves, k_horse_archer},
    Matchup{k_wolves, k_mounted_swordsman},
    Matchup{k_wolves, k_elephant},
    Matchup{k_wolves, k_commander},
    Matchup{k_elephant, k_swordsman},
    Matchup{k_elephant, k_spearman},
    Matchup{k_elephant, k_archer},
    Matchup{k_elephant, k_builder},
    Matchup{k_elephant, k_civilian},
    Matchup{k_elephant, k_mounted_swordsman},
    Matchup{k_elephant, k_horse_spearman},
    Matchup{k_elephant, k_elephant},
    Matchup{k_elephant, k_home},
    Matchup{k_elephant, k_barracks},
    Matchup{k_elephant, k_ram},
    Matchup{k_elephant, k_catapult},
    Matchup{k_swordsman, k_ram},
    Matchup{k_swordsman, k_catapult},
    Matchup{k_spearman, k_ballista},
    Matchup{k_mounted_swordsman, k_siege_tower},
    Matchup{k_builder, k_ram},
    Matchup{k_archer, k_catapult},
    Matchup{k_swordsman, k_home},
    Matchup{k_spearman, k_barracks},
    Matchup{k_swordsman, k_spearman},
    Matchup{k_spearman, k_mounted_swordsman},
    Matchup{k_horse_spearman, k_archer},
    Matchup{k_horse_archer, k_swordsman},
    Matchup{k_archer, k_swordsman},
    Matchup{k_swordsman, k_healer},
    Matchup{k_swordsman, k_civilian},
    Matchup{k_builder, k_spearman},
    Matchup{k_commander, k_swordsman},
    Matchup{k_commander, k_elephant},
};

constexpr int k_attacker_owner = 1;
constexpr int k_defender_owner = 2;

auto make_side(const Side& side,
               QString name,
               int owner,
               QVector3D origin,
               float facing) -> ArenaScenarioGroup {
  if (side.structure.has_value()) {
    auto result =
        building(std::move(name), *side.structure, Nation::Carthage, owner, 1, origin);
    result.facing_degrees = facing;
    result.health_override = result.max_health_override = 1500;
    return result;
  }
  auto result = group(std::move(name), side.troop, owner, 1, origin, side.individuals);
  result.nation_id =
      owner == k_attacker_owner ? Nation::RomanRepublic : Nation::Carthage;
  result.facing_degrees = facing;
  result.health_override = result.max_health_override =
      side.body == Body::Humanoid ? 2000 : 2500;
  return result;
}

void configure_wolves(ArenaScenarioDefinition& s, QVector3D near) {
  s.wildlife = Game::Wildlife::default_settings();
  s.wildlife.enabled = true;
  s.wildlife.seed = 4011U;
  s.wildlife.sheep.enabled = false;
  s.wildlife.sheep.group_count = 0;
  s.wildlife.birds.enabled = false;
  s.wildlife.birds.group_count = 0;
  s.wildlife.wolves.enabled = true;
  s.wildlife.wolves.group_count = 1;
  s.wildlife.wolves.group_size_min = 2;
  s.wildlife.wolves.group_size_max = 2;
  s.wildlife.wolves.aggression = 1.0F;
  s.wildlife.wolves.roam_radius = 14.0F;
  s.wildlife.wolves.alert_radius = 10.0F;
  s.wildlife.wolves.respawn = false;
  s.wildlife.wolves.spawn_areas = {{near.x(), near.y(), near.z()}};
}

auto siege_wreck(const Side& engine) -> ArenaScenarioDefinition {
  QString const engine_key = QString::fromLatin1(engine.key);
  auto s = definition(
      QStringLiteral("matchup_destroy_%1").arg(engine_key),
      QStringLiteral("Matchup: destroy a %1").arg(engine_key),
      QStringLiteral("Swordsmen break a %1. It must tip over and settle as a wreck "
                     "and its crew must fall with it, then everything sinks away; "
                     "nothing may simply vanish.")
          .arg(engine_key),
      22.0F,
      {16.0F, 40.0F, 35.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 3.0F);
  s.suppress_terrain_scatter = true;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.collect_animation_diagnostics = true;
  QString const attacker = QStringLiteral("attacker");
  QString const defender = QStringLiteral("defender");
  auto target =
      make_side(engine, defender, k_defender_owner, {0.0F, 0.0F, 5.0F}, 180.0F);
  target.health_override = target.max_health_override = 120;
  s.groups = {
      target,
      make_side(k_swordsman, attacker, k_attacker_owner, {0.0F, 0.0F, 1.5F}, 0.0F)};
  s.steps = {at(0.5F, Command::AttackMove, attacker, defender)};
  s.expectations.push_back(expectation(Expect::GroupDestroyed, defender));
  s.expectations.push_back(expectation(Expect::GroupHealthUnchanged, attacker));
  add_transition_continuity(s, {attacker});
  add_motion_smoothness(s, {attacker});
  return s;
}

auto matchup(const Matchup& pair) -> ArenaScenarioDefinition {
  QString const attacker_key = QString::fromLatin1(pair.attacker.key);
  QString const defender_key = QString::fromLatin1(pair.defender.key);
  QString const id = QStringLiteral("matchup_%1_vs_%2").arg(attacker_key, defender_key);
  QString const attacker = QStringLiteral("attacker");
  QString const defender = QStringLiteral("defender");
  bool const wolves = pair.attacker.body == Body::Wolves;
  bool const defender_is_structure =
      pair.defender.body == Body::Building || pair.defender.body == Body::Siege;

  auto s = definition(
      id,
      QStringLiteral("Matchup: %1 vs %2").arg(attacker_key, defender_key),
      QStringLiteral(
          "A %1 attacks a %2, breaks off, and attacks again. Nobody may teleport, "
          "spin, swing back and forth or snap between poses; harmless targets "
          "must stay harmless.")
          .arg(attacker_key, defender_key),
      15.0F,
      {22.0F, 46.0F, 30.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.suppress_terrain_scatter = true;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.collect_animation_diagnostics = true;

  bool const siege_target = pair.defender.body == Body::Siege;
  QVector3D const defender_at(0.0F, 0.0F, defender_is_structure ? 6.0F : 4.0F);
  QVector3D const attacker_at(0.0F, 0.0F, siege_target ? 2.5F : -6.0F);
  s.groups.push_back(
      make_side(pair.defender, defender, k_defender_owner, defender_at, 180.0F));

  if (wolves) {
    configure_wolves(s, defender_at + QVector3D(5.0F, 0.0F, -3.0F));
    s.groups.back().owner_id = k_attacker_owner;
    s.groups.back().attacks_disabled = !pair.defender.fights;
    s.steps.push_back(at(9.0F, Command::Move, defender));
    s.steps.back().destination = defender_at + QVector3D(-8.0F, 0.0F, -2.0F);
    s.steps.push_back(at(12.0F, Command::Stop, defender));
    s.expectations.push_back(expectation(Expect::WildlifeHuntObserved));
    s.expectations.push_back(pair.defender.kills_wolves_before_they_bite
                                 ? expectation(Expect::WildlifeCasualtyObserved)
                                 : expectation(Expect::GroupHealthReduced, defender));
  } else {
    s.groups.push_back(
        make_side(pair.attacker, attacker, k_attacker_owner, attacker_at, 0.0F));
    s.steps.push_back(at(0.5F, Command::AttackMove, attacker, defender));
    if (pair.defender.fights) {
      s.steps.push_back(at(0.5F, Command::AttackMove, defender, attacker));
    }
    if (!siege_target) {
      s.steps.push_back(at(8.5F, Command::Move, attacker));
      s.steps.back().destination = attacker_at + QVector3D(0.0F, 0.0F, -2.0F);
      s.steps.push_back(at(11.0F, Command::AttackMove, attacker, defender));
    }
    s.expectations.push_back(expectation(Expect::GroupHealthReduced, defender));
    if (!pair.defender.fights) {
      s.expectations.push_back(expectation(Expect::GroupHealthUnchanged, attacker));
    }
  }

  for (auto const& participant : s.groups) {
    bool const humanoid = participant.name == defender
                              ? pair.defender.body == Body::Humanoid
                              : pair.attacker.body == Body::Humanoid;
    if (humanoid) {
      add_transition_continuity(s, {participant.name});
    }
    if (!participant.spawn_type.has_value()) {
      add_motion_smoothness(s, {participant.name});
    }
  }
  if (wolves) {
    add_motion_smoothness(s, {QString::fromLatin1(k_wildlife_group)});
  }
  if (s.expectations.empty() ||
      std::none_of(s.expectations.begin(), s.expectations.end(), [](auto const& e) {
        return e.kind == Expect::FrameBudget;
      })) {
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
  }
  return s;
}

} // namespace

auto build_matchup_matrix_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.reserve(k_matchups.size());
  for (auto const& pair : k_matchups) {
    result.push_back(matchup(pair));
  }
  for (auto const& engine : {k_ram, k_catapult, k_ballista, k_siege_tower}) {
    result.push_back(siege_wreck(engine));
  }
  return result;
}

} // namespace Arena::Scenarios
