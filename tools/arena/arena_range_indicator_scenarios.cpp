#include "arena_range_indicator_scenarios.h"

#include <array>
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

auto build_range_indicator_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_archers_id),
        QStringLiteral("Range Indicator: Archers"),
        QStringLiteral("Selected archers publish the projectile range ring the combat "
                       "system actually fires with."),
        3.0F,
        {34.0F, 52.0F, 30.0F});
    s.groups = {group(QStringLiteral("archers"),
                      Troop::Archer,
                      1,
                      3,
                      {-3.0F, 0.0F, 0.0F},
                      4,
                      {3.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("archers"), {}, 11.4F),
        expectation(Expect::GroupIsRendered, QStringLiteral("archers")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_siege_minimum_id),
        QStringLiteral("Range Indicator: Siege Minimum"),
        QStringLiteral("A catapult carrying a minimum firing distance draws both its "
                       "inner dead zone and its outer reach."),
        3.0F,
        {46.0F, 54.0F, 28.0F});
    auto catapult =
        group(QStringLiteral("catapult"), Troop::Catapult, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    catapult.attack_min_range_override = 5.0F;
    s.groups = {
        catapult,
        group(QStringLiteral("crew"), Troop::Archer, 1, 1, {-8.0F, 0.0F, 6.0F}, 4)};
    s.expectations = {
        expectation(Expect::RangeIndicatorObserved,
                    QStringLiteral("catapult"),
                    {},
                    18.0F,
                    0.0F,
                    5.0F),
        expectation(Expect::RangeIndicatorObserved, QStringLiteral("crew"), {}, 11.4F),
        expectation(Expect::GroupIsRendered, QStringLiteral("crew")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_mage_id),
        QStringLiteral("Range Indicator: Arcane Caster"),
        QStringLiteral("A grave priest draws the dotted arcane projectile ring, "
                       "distinct in pattern from bow and siege weapons."),
        3.0F,
        {34.0F, 52.0F, 30.0F});
    auto priest = group(
        QStringLiteral("priest"), Troop::GravePriest, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    priest.nation_id = Nation::IronSepulcher;
    s.groups = {priest};
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.expectations = {
        expectation(Expect::RangeIndicatorObserved, QStringLiteral("priest"), {}, 8.6F),
        expectation(Expect::GroupIsRendered, QStringLiteral("priest")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_elevation_id),
        QStringLiteral("Range Indicator: Elevation"),
        QStringLiteral("Archers on a hill keep the same ring radius: the "
                       "indicator states ground distance, not line of sight."),
        3.0F,
        {40.0F, 48.0F, 32.0F});
    s.elevation_patches.push_back({{0.0F, 0.0F, 0.0F}, 12.0F, 4.0F});
    s.groups = {group(QStringLiteral("hill_archers"),
                      Troop::Archer,
                      1,
                      2,
                      {-2.0F, 0.0F, 0.0F},
                      4,
                      {4.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("hill_archers"), {}, 11.4F),
        expectation(Expect::GroupIsRendered, QStringLiteral("hill_archers")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_hold_bonus_id),
        QStringLiteral("Range Indicator: Hold Bonus"),
        QStringLiteral("Holding position lengthens archer reach; the ring has "
                       "to grow with it in the same tick."),
        4.0F,
        {40.0F, 52.0F, 30.0F});
    s.groups = {group(QStringLiteral("held_archers"),
                      Troop::Archer,
                      1,
                      2,
                      {-2.0F, 0.0F, 0.0F},
                      4,
                      {4.0F, 0.0F, 0.0F})};
    s.steps = {at(0.5F, Command::Hold, QStringLiteral("held_archers"))};
    s.expectations = {
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("held_archers"), {}, 17.1F),
        expectation(Expect::GroupIsRendered, QStringLiteral("held_archers")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_mixed_selection_id),
        QStringLiteral("Range Indicator: Mixed Selection"),
        QStringLiteral("A large mixed selection stays legible: melee troops "
                       "draw nothing and the ring count stays under the cap."),
        3.5F,
        {70.0F, 56.0F, 28.0F});
    s.arena_floor_half_extent = 44.0F;
    s.groups = {group(QStringLiteral("line_archers"),
                      Troop::Archer,
                      1,
                      6,
                      {-16.0F, 0.0F, -6.0F},
                      4,
                      {6.0F, 0.0F, 0.0F}),
                group(QStringLiteral("outriders"),
                      Troop::HorseArcher,
                      1,
                      4,
                      {-12.0F, 0.0F, 6.0F},
                      3,
                      {7.0F, 0.0F, 0.0F}),
                group(QStringLiteral("engines"),
                      Troop::Ballista,
                      1,
                      2,
                      {-6.0F, 0.0F, 14.0F},
                      1,
                      {9.0F, 0.0F, 0.0F}),
                group(QStringLiteral("shield_line"),
                      Troop::Swordsman,
                      1,
                      4,
                      {-9.0F, 0.0F, -14.0F},
                      4,
                      {6.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(Expect::RangeIndicatorCountAtMost, {}, {}, 12.0F),
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("engines"), {}, 21.0F),
        expectation(Expect::GroupIsRendered, QStringLiteral("line_archers")),
        expectation(Expect::GroupIsRendered, QStringLiteral("shield_line")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_unarmed_support_brawl_id),
        QStringLiteral("Unarmed Support Brawl"),
        QStringLiteral("Roman and Carthaginian builders, civilians, and healers fight "
                       "matching roles at close range. Builders swing the mallet and "
                       "civilians the cudgel; healers validate the dedicated unarmed "
                       "BPAT guard, jab, cross, hook, contact, and recovery path."),
        10.0F,
        {5.5F, 10.0F, 0.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.camera_focus = QVector3D(0.5F, 0.8F, 0.0F);

    auto roman_builders = group(QStringLiteral("roman_builders"),
                                Troop::Builder,
                                1,
                                1,
                                {-2.3F, 0.0F, -1.40F},
                                1);
    auto carthage_builders = group(QStringLiteral("carthage_builders"),
                                   Troop::Builder,
                                   2,
                                   1,
                                   {-2.3F, 0.0F, 1.40F},
                                   1);
    auto roman_civilians = group(QStringLiteral("roman_civilians"),
                                 Troop::Civilian,
                                 1,
                                 1,
                                 {0.0F, 0.0F, -1.40F},
                                 1);
    auto carthage_civilians = group(QStringLiteral("carthage_civilians"),
                                    Troop::Civilian,
                                    2,
                                    1,
                                    {0.0F, 0.0F, 1.40F},
                                    1);
    auto roman_healers = group(
        QStringLiteral("roman_healers"), Troop::Healer, 1, 1, {2.3F, 0.0F, -1.40F}, 1);
    auto carthage_healers = group(QStringLiteral("carthage_healers"),
                                  Troop::Healer,
                                  2,
                                  1,
                                  {2.3F, 0.0F, 1.40F},
                                  1);
    for (auto* combatant : {&roman_builders,
                            &carthage_builders,
                            &roman_civilians,
                            &carthage_civilians,
                            &roman_healers,
                            &carthage_healers}) {
      combatant->health_override = 600;
      combatant->max_health_override = 600;
    }
    s.groups = {roman_builders,
                carthage_builders,
                roman_civilians,
                carthage_civilians,
                roman_healers,
                carthage_healers};

    struct Pair {
      const char* roman;
      const char* carthage;
    };
    constexpr std::array<Pair, 3> k_pairs{{
        {"roman_builders", "carthage_builders"},
        {"roman_civilians", "carthage_civilians"},
        {"roman_healers", "carthage_healers"},
    }};
    for (auto const& pair : k_pairs) {
      QString const roman = QString::fromLatin1(pair.roman);
      QString const carthage = QString::fromLatin1(pair.carthage);
      s.steps.push_back(at(0.35F, Command::Attack, roman, carthage));
      s.steps.push_back(at(0.35F, Command::Attack, carthage, roman));
      for (auto const& [actor, opponent] :
           {std::pair{roman, carthage}, std::pair{carthage, roman}}) {
        s.expectations.push_back(expectation(Expect::GroupIsRendered, actor));
        s.expectations.push_back(expectation(Expect::AttackAnimationObserved, actor));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, actor, {}, 2.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, actor, opponent));
        s.expectations.push_back(expectation(Expect::NoLimbOverextension, actor));
        s.expectations.push_back(expectation(Expect::NoUnexpectedFallPose, actor));
      }
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
