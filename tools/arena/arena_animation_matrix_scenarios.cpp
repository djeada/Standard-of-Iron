#include "arena_animation_matrix_scenarios.h"

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

auto build_animation_matrix_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_infantry_locomotion_matrix_id),
        QStringLiteral("Infantry Locomotion Matrix"),
        QStringLiteral(
            "Archers, swordsmen, and spearmen start, march, stop, and reverse."),
        13.0F,
        {26.0F, 48.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-6.0F, 0.0F, -9.0F}, 8),
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -9.0F}, 8),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {6.0F, 0.0F, -9.0F}, 8)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("archers")),
               at(0.5F, Command::FormationMove, QStringLiteral("swords")),
               at(0.5F, Command::FormationMove, QStringLiteral("spears")),
               at(6.0F, Command::Stop, QStringLiteral("archers")),
               at(6.0F, Command::Stop, QStringLiteral("swords")),
               at(6.0F, Command::Stop, QStringLiteral("spears")),
               at(8.0F, Command::FormationMove, QStringLiteral("archers")),
               at(8.0F, Command::FormationMove, QStringLiteral("swords")),
               at(8.0F, Command::FormationMove, QStringLiteral("spears"))};
    for (int lane = 0; lane < 3; ++lane) {
      s.steps[static_cast<std::size_t>(lane)].destination = {
          -6.0F + 6.0F * static_cast<float>(lane), 0.0F, 8.0F};
      s.steps[static_cast<std::size_t>(lane + 6)].destination = {
          -6.0F + 6.0F * static_cast<float>(lane), 0.0F, -16.0F};
    }
    add_visual_stability(s,
                         {QStringLiteral("archers"),
                          QStringLiteral("swords"),
                          QStringLiteral("spears")});
    for (auto const& name : {QStringLiteral("archers"),
                             QStringLiteral("swords"),
                             QStringLiteral("spears")}) {
      s.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, {}, 0.45F));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(
          expectation(Expect::FormationOrderPreserved, name, {}, 0.8F));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_locomotion_matrix_id),
                        QStringLiteral("Mounted Locomotion Matrix"),
                        QStringLiteral("Horse archers, swordsmans, and horse spearmen "
                                       "start, ride, stop, and reverse."),
                        13.0F,
                        {30.0F, 50.0F, 30.0F});
    s.groups = {group(QStringLiteral("horse_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {-7.0F, 0.0F, -10.0F},
                      6),
                group(QStringLiteral("swordsmans"),
                      Troop::MountedSwordsman,
                      1,
                      1,
                      {0.0F, 0.0F, -10.0F},
                      6),
                group(QStringLiteral("horse_spears"),
                      Troop::HorseSpearman,
                      1,
                      1,
                      {7.0F, 0.0F, -10.0F},
                      6)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("horse_archers")),
               at(0.5F, Command::FormationMove, QStringLiteral("swordsmans")),
               at(0.5F, Command::FormationMove, QStringLiteral("horse_spears")),
               at(6.0F, Command::Stop, QStringLiteral("horse_archers")),
               at(6.0F, Command::Stop, QStringLiteral("swordsmans")),
               at(6.0F, Command::Stop, QStringLiteral("horse_spears")),
               at(8.0F, Command::FormationMove, QStringLiteral("horse_archers")),
               at(8.0F, Command::FormationMove, QStringLiteral("swordsmans")),
               at(8.0F, Command::FormationMove, QStringLiteral("horse_spears"))};
    for (int lane = 0; lane < 3; ++lane) {
      s.steps[static_cast<std::size_t>(lane)].destination = {
          -7.0F + 7.0F * static_cast<float>(lane), 0.0F, 9.0F};
      s.steps[static_cast<std::size_t>(lane + 6)].destination = {
          -7.0F + 7.0F * static_cast<float>(lane), 0.0F, -20.0F};
    }
    add_visual_stability(s,
                         {QStringLiteral("horse_archers"),
                          QStringLiteral("swordsmans"),
                          QStringLiteral("horse_spears")});
    for (auto const& name : {QStringLiteral("horse_archers"),
                             QStringLiteral("swordsmans"),
                             QStringLiteral("horse_spears")}) {
      s.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, {}, 0.45F));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(
          expectation(Expect::FormationOrderPreserved, name, {}, 0.8F));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_elephant_locomotion_matrix_id),
                        QStringLiteral("Elephant Locomotion Matrix"),
                        QStringLiteral("War elephants start, travel, stop, and reverse "
                                       "using the production authored skin."),
                        13.0F,
                        {24.0F, 48.0F, 28.0F});
    s.groups = {group(QStringLiteral("elephants"),
                      Troop::Elephant,
                      2,
                      3,
                      {0.0F, 0.0F, -10.0F},
                      1,
                      {5.5F, 0.0F, 0.0F})};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("elephants")),
               at(6.0F, Command::Stop, QStringLiteral("elephants")),
               at(8.0F, Command::FormationMove, QStringLiteral("elephants"))};
    s.steps[0].destination = {0.0F, 0.0F, 9.0F};
    s.steps[2].destination = {0.0F, 0.0F, -20.0F};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("elephants")));
    s.expectations.push_back(
        expectation(Expect::NoRootTeleport, QStringLiteral("elephants")));
    s.expectations.push_back(
        expectation(Expect::MovementIsContinuous, QStringLiteral("elephants")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("elephants"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("elephants"), {}, 0.8F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_infantry_damage_matrix_id),
                        QStringLiteral("Infantry Damage Matrix"),
                        QStringLiteral("Archers, swordsmen, and spearmen absorb a hit "
                                       "and enter stable death sequences."),
                        10.0F,
                        {19.0F, 46.0F, 30.0F});
    auto archers =
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-5.0F, 0.0F, 0.0F}, 1);
    auto swords =
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    auto spears =
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {5.0F, 0.0F, 0.0F}, 1);
    for (auto* troop : {&archers, &swords, &spears}) {
      troop->nation_id = Nation::Carthage;
      troop->owner_id = 2;
      troop->facing_degrees = 180.0F;
      troop->health_override = 90;
      troop->max_health_override = 90;
    }
    archers.origin.setZ(1.8F);
    swords.origin.setZ(1.8F);
    spears.origin.setZ(1.8F);
    s.groups = {archers,
                swords,
                spears,
                group(QStringLiteral("attack_archers"),
                      Troop::Swordsman,
                      1,
                      1,
                      {-5.0F, 0.0F, -1.8F},
                      1),
                group(QStringLiteral("attack_swords"),
                      Troop::Swordsman,
                      1,
                      1,
                      {0.0F, 0.0F, -1.8F},
                      1),
                group(QStringLiteral("attack_spears"),
                      Troop::Swordsman,
                      1,
                      1,
                      {5.0F, 0.0F, -1.8F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_archers"),
                  QStringLiteral("archers")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_swords"),
                  QStringLiteral("swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_spears"),
                  QStringLiteral("spears"))};
    add_visual_stability(s,
                         {QStringLiteral("archers"),
                          QStringLiteral("swords"),
                          QStringLiteral("spears")});
    for (auto const& name : {QStringLiteral("archers"),
                             QStringLiteral("swords"),
                             QStringLiteral("spears")}) {
      s.expectations.push_back(expectation(Expect::DeathAnimationObserved, name));
      s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mounted_damage_matrix_id),
        QStringLiteral("Mounted Damage Matrix"),
        QStringLiteral("Horse archers, swordsmans, and horse spearmen absorb a "
                       "hit and dismount through stable death sequences."),
        10.0F,
        {23.0F, 48.0F, 30.0F});
    auto archers = group(QStringLiteral("horse_archers"),
                         Troop::HorseArcher,
                         1,
                         1,
                         {-6.0F, 0.0F, 0.0F},
                         1);
    auto swordsmans = group(QStringLiteral("swordsmans"),
                            Troop::MountedSwordsman,
                            1,
                            1,
                            {0.0F, 0.0F, 0.0F},
                            1);
    auto spears = group(QStringLiteral("horse_spears"),
                        Troop::HorseSpearman,
                        1,
                        1,
                        {6.0F, 0.0F, 0.0F},
                        1);
    for (auto* troop : {&archers, &swordsmans, &spears}) {
      troop->nation_id = Nation::Carthage;
      troop->owner_id = 2;
      troop->facing_degrees = 180.0F;
      troop->health_override = 120;
      troop->max_health_override = 120;
    }
    archers.origin.setZ(2.4F);
    swordsmans.origin.setZ(2.4F);
    spears.origin.setZ(2.4F);
    s.groups = {archers,
                swordsmans,
                spears,
                group(QStringLiteral("attack_archers"),
                      Troop::MountedSwordsman,
                      1,
                      1,
                      {-6.0F, 0.0F, -2.4F},
                      1),
                group(QStringLiteral("attack_swordsmans"),
                      Troop::MountedSwordsman,
                      1,
                      1,
                      {0.0F, 0.0F, -2.4F},
                      1),
                group(QStringLiteral("attack_spears"),
                      Troop::MountedSwordsman,
                      1,
                      1,
                      {6.0F, 0.0F, -2.4F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_archers"),
                  QStringLiteral("horse_archers")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_swordsmans"),
                  QStringLiteral("swordsmans")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_spears"),
                  QStringLiteral("horse_spears"))};
    add_visual_stability(s,
                         {QStringLiteral("horse_archers"),
                          QStringLiteral("swordsmans"),
                          QStringLiteral("horse_spears")});
    for (auto const& name : {QStringLiteral("horse_archers"),
                             QStringLiteral("swordsmans"),
                             QStringLiteral("horse_spears")}) {
      s.expectations.push_back(expectation(Expect::DeathAnimationObserved, name));
      s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
    }
    result.push_back(std::move(s));
  }

  struct ActionTransitionSpec {
    const char* id;
    const char* label;
    Troop troop;
    bool mounted;
  };
  for (auto const& spec :
       {ActionTransitionSpec{k_archer_action_transition_id,
                             "Archer Action Transition",
                             Troop::Archer,
                             false},
        ActionTransitionSpec{k_swordsman_action_transition_id,
                             "Swordsman Action Transition",
                             Troop::Swordsman,
                             false},
        ActionTransitionSpec{k_spearman_action_transition_id,
                             "Spearman Action Transition",
                             Troop::Spearman,
                             false},
        ActionTransitionSpec{k_horse_archer_action_transition_id,
                             "Horse Archer Action Transition",
                             Troop::HorseArcher,
                             true},
        ActionTransitionSpec{k_mounted_swordsman_action_transition_id,
                             "Mounted Swordsman Action Transition",
                             Troop::MountedSwordsman,
                             true},
        ActionTransitionSpec{k_horse_spearman_action_transition_id,
                             "Horse Spearman Action Transition",
                             Troop::HorseSpearman,
                             true}}) {
    auto s = definition(
        QString::fromLatin1(spec.id),
        QString::fromLatin1(spec.label),
        QStringLiteral("Move, acquire one target, complete the authored attack and "
                       "recovery, then return to player-controlled locomotion."),
        10.0F,
        {spec.mounted ? 15.0F : 12.0F, 46.0F, 28.0F});
    auto target =
        group(QStringLiteral("target"), Troop::Healer, 2, 1, {0.0F, 0.0F, 2.0F}, 1);
    target.health_override = 8;
    target.max_health_override = 8;
    s.groups = {group(QStringLiteral("actor"),
                      spec.troop,
                      1,
                      1,
                      {0.0F, 0.0F, spec.mounted ? -9.0F : -8.0F},
                      1),
                target};
    s.steps = {at(0.0F, Command::Move, QStringLiteral("actor")),
               at(2.0F,
                  Command::AttackMove,
                  QStringLiteral("actor"),
                  QStringLiteral("target")),
               when_destroyed(QStringLiteral("target"),
                              Command::FormationMove,
                              QStringLiteral("actor"),
                              {})};
    s.steps[0].destination = {0.0F, 0.0F, -3.0F};
    s.steps[2].destination = {0.0F, 0.0F, -8.0F};
    add_visual_stability(s, {QStringLiteral("actor")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::AttackRecoveryObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("actor")));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
