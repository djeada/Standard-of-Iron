#include "arena_skirmisher_scenarios.h"

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
using Troop = Game::Units::TroopType;

constexpr int k_rome_owner = 1;
constexpr int k_carthage_owner = 2;

auto skirmisher_lineup() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_skirmisher_lineup_id),
      QStringLiteral("Skirmisher Lineup"),
      QStringLiteral("Roman velites with parma, javelins and wolf-skin beside "
                     "unarmoured Balearic slingers, facing the camera and turned "
                     "away."),
      10.0F,
      {6.0F, 8.0F, 0.0F});
  s.suppress_terrain_scatter = true;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.camera_focus = QVector3D(0.0F, 0.6F, 0.0F);
  s.owner_teams = {{.owner_id = k_rome_owner, .team_id = 1},
                   {.owner_id = k_carthage_owner, .team_id = 1}};

  struct LineupEntry {
    const char* group_name{};
    Troop troop;
    Nation nation;
    int owner{};
    float x{};
    float facing{};
  };
  const LineupEntry entries[] = {
      {"velites", Troop::Velites, Nation::RomanRepublic, k_rome_owner, -2.4F, 0.0F},
      {"velites_back",
       Troop::Velites,
       Nation::RomanRepublic,
       k_rome_owner,
       -0.8F,
       180.0F},
      {"slingers", Troop::Slinger, Nation::Carthage, k_carthage_owner, 0.8F, 0.0F},
      {"slingers_back",
       Troop::Slinger,
       Nation::Carthage,
       k_carthage_owner,
       2.4F,
       180.0F},
  };
  for (auto const& entry : entries) {
    auto troop = nation_group(QString::fromLatin1(entry.group_name),
                              entry.troop,
                              entry.nation,
                              entry.owner,
                              1,
                              {entry.x, 0.0F, 0.0F},
                              1);
    troop.facing_degrees = entry.facing;
    s.groups.push_back(std::move(troop));
    s.steps.push_back(at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
  }
  s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
  return s;
}

auto skirmisher_screen() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_skirmisher_screen_id),
      QStringLiteral("Skirmish Screen"),
      QStringLiteral("Velites and Balearic slingers trade javelins and stones ahead "
                     "of their lines. When the Carthaginian spearmen advance, the "
                     "velites fall back behind the legionaries on their own."),
      22.0F,
      {26.0F, 34.0F, -20.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
  s.owner_teams = {{.owner_id = k_rome_owner, .team_id = 1},
                   {.owner_id = k_carthage_owner, .team_id = 2}};

  s.groups = {
      nation_group(QStringLiteral("roman_line"),
                   Troop::Swordsman,
                   Nation::RomanRepublic,
                   k_rome_owner,
                   2,
                   {-1.5F, 0.0F, -5.0F},
                   0,
                   {3.0F, 0.0F, 0.0F}),
      nation_group(QStringLiteral("velites"),
                   Troop::Velites,
                   Nation::RomanRepublic,
                   k_rome_owner,
                   1,
                   {0.0F, 0.0F, 1.0F}),
      nation_group(QStringLiteral("slingers"),
                   Troop::Slinger,
                   Nation::Carthage,
                   k_carthage_owner,
                   1,
                   {0.0F, 0.0F, 8.5F}),
      nation_group(QStringLiteral("carthage_line"),
                   Troop::Spearman,
                   Nation::Carthage,
                   k_carthage_owner,
                   2,
                   {-1.5F, 0.0F, 14.0F},
                   0,
                   {3.0F, 0.0F, 0.0F}),
  };
  s.steps = {
      at(0.0F, Command::Hold, QStringLiteral("roman_line")),
      at(0.2F, Command::Attack, QStringLiteral("velites"), QStringLiteral("slingers")),
      at(0.2F, Command::Attack, QStringLiteral("slingers"), QStringLiteral("velites")),
      at(4.0F,
         Command::Attack,
         QStringLiteral("carthage_line"),
         QStringLiteral("velites")),
  };
  s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                       QStringLiteral("velites"),
                                       QStringLiteral("slingers")));
  s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                       QStringLiteral("slingers"),
                                       QStringLiteral("velites")));
  s.expectations.push_back(
      expectation(Expect::AttackAnimationObserved, QStringLiteral("velites")));
  s.expectations.push_back(
      expectation(Expect::AttackAnimationObserved, QStringLiteral("slingers")));
  auto withdrew = expectation(
      Expect::GroupReachedDestination, QStringLiteral("velites"), {}, 0.0F, 4.0F, 2.5F);
  withdrew.position = QVector3D(0.0F, 0.0F, -2.0F);
  s.expectations.push_back(withdrew);
  return s;
}

} // namespace

auto build_skirmisher_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(skirmisher_lineup());
  result.push_back(skirmisher_screen());
  return result;
}

} // namespace Arena::Scenarios
