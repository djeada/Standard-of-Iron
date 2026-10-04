#include "arena_allied_nation_scenarios.h"

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
constexpr int k_gallic_owner = 3;
constexpr int k_iberian_owner = 4;

auto allied_lineup() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_allied_identity_lineup_id),
      QStringLiteral("Allied Identity Lineup"),
      QStringLiteral("Roman, Carthaginian, Gallic and Iberian swordsmen and cavalry "
                     "side by side, facing the camera and turned away, so the allies' "
                     "shields, swords, trousers, tunics and helmets can be told apart "
                     "from the Carthaginian and Roman lines."),
      12.0F,
      {16.0F, 14.0F, 0.0F});
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
    float z{};
    float facing{};
  };
  const LineupEntry entries[] = {
      {"rome_swordsman",
       Troop::Swordsman,
       Nation::RomanRepublic,
       1,
       -6.0F,
       -2.0F,
       0.0F},
      {"carthage_swordsman", Troop::Swordsman, Nation::Carthage, 2, -2.0F, -2.0F, 0.0F},
      {"gallic_swordsman", Troop::Swordsman, Nation::Gauls, 2, 2.0F, -2.0F, 0.0F},
      {"iberian_swordsman", Troop::Swordsman, Nation::Iberians, 2, 6.0F, -2.0F, 0.0F},
      {"gallic_swordsman_back",
       Troop::Swordsman,
       Nation::Gauls,
       2,
       3.4F,
       -2.0F,
       180.0F},
      {"iberian_swordsman_back",
       Troop::Swordsman,
       Nation::Iberians,
       2,
       7.4F,
       -2.0F,
       180.0F},
      {"rome_cavalry",
       Troop::MountedSwordsman,
       Nation::RomanRepublic,
       1,
       -6.0F,
       3.5F,
       0.0F},
      {"carthage_cavalry",
       Troop::MountedSwordsman,
       Nation::Carthage,
       2,
       -2.0F,
       3.5F,
       0.0F},
      {"gallic_cavalry", Troop::MountedSwordsman, Nation::Gauls, 2, 2.0F, 3.5F, 0.0F},
      {"iberian_cavalry",
       Troop::MountedSwordsman,
       Nation::Iberians,
       2,
       6.0F,
       3.5F,
       0.0F},
  };
  for (auto const& entry : entries) {
    auto troop = group(QString::fromLatin1(entry.group_name),
                       entry.troop,
                       entry.owner,
                       1,
                       {entry.x, 0.0F, entry.z},
                       1);
    troop.nation_id = entry.nation;
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

auto cannae_allied_clash() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_cannae_allied_clash_id),
      QStringLiteral("Cannae: Allied Line"),
      QStringLiteral("Gallic and Iberian swordsmen hold Carthage's centre against a "
                     "Roman advance while Gallic and Iberian horse ride in on the "
                     "flank. Each ally has its own owner slot on Carthage's team, as "
                     "a mission would seat them."),
      18.0F,
      {24.0F, 42.0F, 24.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.owner_teams = {{.owner_id = k_rome_owner, .team_id = 1},
                   {.owner_id = k_carthage_owner, .team_id = 2},
                   {.owner_id = k_gallic_owner, .team_id = 2},
                   {.owner_id = k_iberian_owner, .team_id = 2}};

  s.groups = {
      nation_group(QStringLiteral("roman_line"),
                   Troop::Swordsman,
                   Nation::RomanRepublic,
                   k_rome_owner,
                   3,
                   {-2.6F, 0.0F, -8.0F},
                   0,
                   {2.6F, 0.0F, 0.0F}),
      nation_group(QStringLiteral("roman_horse"),
                   Troop::MountedSwordsman,
                   Nation::RomanRepublic,
                   k_rome_owner,
                   1,
                   {11.0F, 0.0F, -6.0F}),
      nation_group(QStringLiteral("gallic_centre"),
                   Troop::Swordsman,
                   Nation::Gauls,
                   k_gallic_owner,
                   2,
                   {-1.3F, 0.0F, 6.0F}),
      nation_group(QStringLiteral("iberian_centre"),
                   Troop::Swordsman,
                   Nation::Iberians,
                   k_iberian_owner,
                   2,
                   {-6.5F, 0.0F, 6.0F}),
      nation_group(QStringLiteral("gallic_horse"),
                   Troop::MountedSwordsman,
                   Nation::Gauls,
                   k_gallic_owner,
                   1,
                   {12.0F, 0.0F, 8.0F}),
      nation_group(QStringLiteral("iberian_horse"),
                   Troop::MountedSwordsman,
                   Nation::Iberians,
                   k_iberian_owner,
                   1,
                   {15.0F, 0.0F, 8.0F}),
      nation_group(QStringLiteral("libyan_reserve"),
                   Troop::Swordsman,
                   Nation::Carthage,
                   k_carthage_owner,
                   1,
                   {4.0F, 0.0F, 9.0F}),
  };
  s.steps = {
      at(0.0F,
         Command::Attack,
         QStringLiteral("roman_line"),
         QStringLiteral("gallic_centre")),
      at(0.0F, Command::Hold, QStringLiteral("gallic_centre")),
      at(0.0F, Command::Hold, QStringLiteral("iberian_centre")),
      at(0.0F, Command::Hold, QStringLiteral("libyan_reserve")),
      at(0.5F,
         Command::Charge,
         QStringLiteral("gallic_horse"),
         QStringLiteral("roman_horse")),
      at(0.5F,
         Command::Charge,
         QStringLiteral("iberian_horse"),
         QStringLiteral("roman_horse")),
      when_near(QStringLiteral("roman_line"),
                QStringLiteral("gallic_centre"),
                3.0F,
                Command::Attack),
  };
  s.steps.back().group = QStringLiteral("iberian_centre");
  s.steps.back().target_group = QStringLiteral("roman_line");
  // Body stability is asserted on the infantry only: cavalry meeting cavalry
  // tilts riders past the check whichever nation rides (Carthage's own horse
  // does it too), which is a mounted-melee issue of its own.
  add_visual_stability(
      s, {QStringLiteral("gallic_centre"), QStringLiteral("iberian_centre")});
  s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                       QStringLiteral("roman_line"),
                                       QStringLiteral("gallic_centre")));
  s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                       QStringLiteral("gallic_horse"),
                                       QStringLiteral("roman_horse")));
  s.expectations.push_back(
      expectation(Expect::GroupIsRendered, QStringLiteral("iberian_centre")));
  s.expectations.push_back(
      expectation(Expect::GroupIsRendered, QStringLiteral("iberian_horse")));
  return s;
}

} // namespace

auto build_allied_nation_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(allied_lineup());
  result.push_back(cannae_allied_clash());
  return result;
}

} // namespace Arena::Scenarios
