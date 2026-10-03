#include "arena_commander_duel_scenarios.h"

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

auto build_commander_duel_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    struct DuelSpec {
      const char* id;
      const char* label;
      Troop roman;
      Troop carthaginian;
    };
    constexpr DuelSpec duels[] = {
        {k_commander_sword_duel_id,
         "Sword Commanders: Scipio vs Hannibal",
         Troop::RomanVeteranConsul,
         Troop::CarthageSwordCommander},
        {k_commander_spear_duel_id,
         "Spear Commanders: Fabius vs Hanno",
         Troop::RomanLegionOrganizer,
         Troop::CarthageSpearCommander},
        {k_commander_bow_duel_id,
         "Bow Commanders: Marcellus vs Hasdrubal",
         Troop::RomanFieldCommander,
         Troop::CarthageBowCommander},
    };
    for (auto const& duel : duels) {
      auto s = definition(
          QString::fromLatin1(duel.id),
          QString::fromLatin1(duel.label),
          QStringLiteral("A durable one-on-one commander duel validating distinct "
                         "weapons, authored contact, reactions, attack cadence, and "
                         "readable silhouettes without bodyguards."),
          11.0F,
          {10.5F, 52.0F, 90.0F});
      s.suppress_terrain_scatter = true;
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      auto roman = group(
          QStringLiteral("roman_commander"), duel.roman, 1, 1, {0.0F, 0.0F, -4.5F}, 1);
      auto carthaginian = group(QStringLiteral("carthage_commander"),
                                duel.carthaginian,
                                2,
                                1,
                                {0.0F, 0.0F, 4.5F},
                                1);
      roman.health_override = roman.max_health_override = 1200;
      carthaginian.health_override = carthaginian.max_health_override = 1200;
      s.groups = {roman, carthaginian};
      s.steps = {
          at(0.4F,
             Command::Attack,
             QStringLiteral("roman_commander"),
             QStringLiteral("carthage_commander")),
          at(0.4F,
             Command::Attack,
             QStringLiteral("carthage_commander"),
             QStringLiteral("roman_commander")),
      };
      add_visual_stability(
          s, {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")});
      for (auto const& name :
           {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")}) {
        QString const opponent = name == QStringLiteral("roman_commander")
                                     ? QStringLiteral("carthage_commander")
                                     : QStringLiteral("roman_commander");
        s.expectations.push_back(expectation(Expect::AttackAnimationObserved, name));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, name, {}, 2.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, name, opponent));
        s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      }
      result.push_back(std::move(s));
    }
  }

  {
    struct SignatureDuel {
      const char* id;
      const char* label;
      const char* description;
      Troop roman;
      Troop carthaginian;
    };
    constexpr std::array<SignatureDuel, 3> signature_duels{{
        {k_commander_signature_spear_vs_sword_id,
         "Commander Signatures: Fabius vs Hannibal",
         "Spear against sword. Fabius answers with the braced thrust that rocks "
         "his man back; Hannibal replies with the encircling cut. Long enough for "
         "both signatures to come round more than once.",
         Troop::RomanLegionOrganizer,
         Troop::CarthageSwordCommander},
        {k_commander_signature_sword_vs_bow_id,
         "Commander Signatures: Scipio vs Hasdrubal",
         "Sword against bow at arm's length. Scipio's consular riposte lands "
         "heavy; Hasdrubal keeps loosing hunting shots into the clinch instead "
         "of backing away.",
         Troop::RomanVeteranConsul,
         Troop::CarthageBowCommander},
        {k_commander_signature_bow_vs_spear_id,
         "Commander Signatures: Marcellus vs Hanno",
         "Bow against spear. Marcellus fires point-blank between sword strokes "
         "while Hanno sweeps his spear through the clinch.",
         Troop::RomanFieldCommander,
         Troop::CarthageSpearCommander},
    }};

    for (auto const& duel : signature_duels) {
      auto s = definition(QString::fromLatin1(duel.id),
                          QString::fromLatin1(duel.label),
                          QString::fromLatin1(duel.description),
                          24.0F,
                          {9.0F, 46.0F, 90.0F});
      s.suppress_terrain_scatter = true;
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.9F, 0.0F);

      auto roman = group(
          QStringLiteral("roman_commander"), duel.roman, 1, 1, {0.0F, 0.0F, -4.0F}, 1);
      auto carthaginian = group(QStringLiteral("carthage_commander"),
                                duel.carthaginian,
                                2,
                                1,
                                {0.0F, 0.0F, 4.0F},
                                1);

      roman.health_override = roman.max_health_override = 9000;
      carthaginian.health_override = carthaginian.max_health_override = 9000;
      s.groups = {roman, carthaginian};
      s.steps = {
          at(0.4F,
             Command::Attack,
             QStringLiteral("roman_commander"),
             QStringLiteral("carthage_commander")),
          at(0.4F,
             Command::Attack,
             QStringLiteral("carthage_commander"),
             QStringLiteral("roman_commander")),
      };
      add_visual_stability(
          s, {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")});
      for (auto const& name :
           {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")}) {
        QString const opponent = name == QStringLiteral("roman_commander")
                                     ? QStringLiteral("carthage_commander")
                                     : QStringLiteral("roman_commander");
        s.expectations.push_back(expectation(Expect::GroupExists, name));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, name, {}, 3.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, name, opponent));
        s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      }
      result.push_back(std::move(s));
    }
  }

  {
    struct PressScene {
      const char* id;
      const char* label;
      const char* description;
      Troop commander;
      bool ranged;
    };
    constexpr std::array<PressScene, 3> press_scenes{{
        {k_commander_press_sword_id,
         "Commander in the Press: Scipio",
         "A sword commander wades into a Carthaginian spear block with a "
         "cohort at his back. The battle camera that has to show his combo "
         "links, arcs and knockdowns among eighty bodies.",
         Troop::RomanVeteranConsul,
         false},
        {k_commander_press_spear_id,
         "Commander in the Press: Fabius",
         "A spear commander holds the front of a spear block: thrusts, sweeps "
         "and the phalanx sweep signature read from a zoomed-out camera.",
         Troop::RomanLegionOrganizer,
         false},
        {k_commander_press_bow_id,
         "Commander in the Press: Marcellus",
         "A bow commander shoots over his own line into the press: commander "
         "arrows and the point-blank volley signature.",
         Troop::RomanFieldCommander,
         true},
    }};

    for (auto const& scene : press_scenes) {
      auto s = definition(QString::fromLatin1(scene.id),
                          QString::fromLatin1(scene.label),
                          QString::fromLatin1(scene.description),
                          26.0F,
                          {15.0F, 48.0F, 20.0F});
      s.suppress_terrain_scatter = true;
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(-4.0F, 0.9F, 1.0F);

      auto commander = group(QStringLiteral("commander"),
                             scene.commander,
                             1,
                             1,
                             {0.0F, 0.0F, scene.ranged ? -11.0F : -5.0F},
                             1);
      commander.health_override = commander.max_health_override = 9000;
      auto roman_swords = group(QStringLiteral("roman_swords"),
                                Troop::Swordsman,
                                1,
                                2,
                                {-5.0F, 0.0F, -8.0F},
                                16,
                                {10.0F, 0.0F, 0.0F});
      auto punic_spears = group(QStringLiteral("punic_spears"),
                                Troop::Spearman,
                                2,
                                3,
                                {-8.0F, 0.0F, 6.0F},
                                16,
                                {8.0F, 0.0F, 0.0F});
      auto punic_swords = group(QStringLiteral("punic_swords"),
                                Troop::Swordsman,
                                2,
                                2,
                                {-5.0F, 0.0F, 11.0F},
                                16,
                                {10.0F, 0.0F, 0.0F});
      s.groups = {commander, roman_swords, punic_spears, punic_swords};
      s.steps = {
          at(0.4F,
             scene.ranged ? Command::Attack : Command::AttackMove,
             QStringLiteral("commander"),
             QStringLiteral("punic_spears")),
          at(0.4F,
             Command::AttackMove,
             QStringLiteral("roman_swords"),
             QStringLiteral("punic_spears")),
          at(0.4F,
             Command::AttackMove,
             QStringLiteral("punic_spears"),
             QStringLiteral("commander")),
          at(0.4F,
             Command::AttackMove,
             QStringLiteral("punic_swords"),
             QStringLiteral("roman_swords")),
      };
      for (auto const& name :
           {QStringLiteral("commander"), QStringLiteral("punic_spears")}) {
        s.expectations.push_back(expectation(Expect::NoPoseOscillation, name));
        s.expectations.push_back(expectation(Expect::MovementIsContinuous, name));
        s.expectations.push_back(expectation(Expect::GroupIsRendered, name));
      }
      s.expectations.push_back(
          expectation(Expect::NoRootTeleport, QStringLiteral("commander")));
      s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("commander")));
      s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                           QStringLiteral("commander"),
                                           QStringLiteral("punic_spears")));
      result.push_back(std::move(s));
    }
  }

  return result;
}

} // namespace Arena::Scenarios
