#include "arena_combat_scenarios.h"

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

auto build_combat_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_melee_lock_id),
        QStringLiteral("Melee Lock"),
        QStringLiteral("Reciprocal melee-lock ownership and animation flow."),
        7.0F,
        {8.0F, 40.0F, 28.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Swordsman, 1, 1, {-0.8F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Swordsman, 2, 1, {0.8F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::MeleeLock, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::MeleeLock, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_chase_to_attack_id),
        QStringLiteral("Chase To Attack"),
        QStringLiteral("Out-of-range attacker chases into a visible strike."),
        12.0F,
        {17.0F, 46.0F, 20.0F});
    s.groups = {
        group(QStringLiteral("attacker"),
              Troop::Swordsman,
              1,
              1,
              {0.0F, 0.0F, -10.0F},
              6),
        group(
            QStringLiteral("defender"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 4.0F}, 6)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("defender"))};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("defender")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("attacker"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("defender")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_attack_to_chase_id),
                   QStringLiteral("Attack To Chase"),
                   QStringLiteral("Target disengages and attacker returns to chase."),
                   10.0F,
                   {14.0F, 42.0F, 28.0F});
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 4),
        group(QStringLiteral("runner"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 1.6F}, 4)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("runner")),
               at(0.85F, Command::Move, QStringLiteral("runner"))};
    s.steps.back().destination = {0.0F, 0.0F, 10.0F};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("runner")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("runner"), {}, 0.45F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_target_death_id),
                   QStringLiteral("Target Death"),
                   QStringLiteral("Normal damage path into a stable death sequence."),
                   8.0F,
                   {10.0F, 42.0F, 28.0F});
    auto target =
        group(QStringLiteral("target"), Troop::Healer, 2, 1, {0.0F, 0.0F, 1.8F}, 1);
    target.health_override = 6;
    target.max_health_override = 6;
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 1),
        target};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("target"))};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("target")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("target")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_combat_feedback_capture_id),
        QStringLiteral("Combat Feedback Capture"),
        QStringLiteral("Every kind of hit the player has to read at once: a melee "
                       "lane, an archer volley that connects, a volley loosed at "
                       "riders who ride out of it and miss, a ballista chipping a "
                       "wall, and a killing blow. Overlays are kept in the capture "
                       "so the target lock rings, incoming-attacker chevrons and "
                       "projectile relation tints can be reviewed."),
        12.0F,
        {30.0F, 50.0F, 8.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    auto swords = group(
        QStringLiteral("swords"), Troop::Swordsman, 1, 1, {-10.0F, 0.0F, -4.0F}, 3);
    auto sword_targets = group(QStringLiteral("sword_targets"),
                               Troop::Swordsman,
                               2,
                               1,
                               {-10.0F, 0.0F, 3.0F},
                               3);
    auto archers =
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-2.0F, 0.0F, -9.0F}, 4);
    auto archer_targets = group(QStringLiteral("archer_targets"),
                                Troop::Spearman,
                                2,
                                1,
                                {-2.0F, 0.0F, 4.0F},
                                3);
    auto enemy_archers = group(
        QStringLiteral("enemy_archers"), Troop::Archer, 2, 1, {5.0F, 0.0F, 6.0F}, 3);
    auto riders = group(
        QStringLiteral("riders"), Troop::HorseArcher, 1, 1, {5.0F, 0.0F, -3.0F}, 2);
    auto ballista = group(
        QStringLiteral("ballista"), Troop::Ballista, 1, 1, {11.0F, 0.0F, -9.0F}, 1);
    auto wall = building(QStringLiteral("wall"),
                         Game::Units::SpawnType::WallSegment,
                         Nation::Carthage,
                         2,
                         1,
                         {12.0F, 0.0F, 4.0F});
    auto killer = group(
        QStringLiteral("killer"), Troop::Swordsman, 1, 1, {-15.0F, 0.0F, -2.0F}, 1);
    auto victim =
        group(QStringLiteral("victim"), Troop::Healer, 2, 1, {-15.0F, 0.0F, 1.6F}, 1);
    swords.health_override = swords.max_health_override = 900;
    sword_targets.health_override = sword_targets.max_health_override = 900;
    archer_targets.health_override = archer_targets.max_health_override = 900;
    riders.health_override = riders.max_health_override = 900;
    wall.health_override = wall.max_health_override = 1400;
    victim.health_override = victim.max_health_override = 6;
    s.groups = {swords,
                sword_targets,
                archers,
                archer_targets,
                enemy_archers,
                riders,
                ballista,
                wall,
                killer,
                victim};
    s.steps = {
        at(0.1F,
           Command::Attack,
           QStringLiteral("swords"),
           QStringLiteral("sword_targets")),
        at(0.1F,
           Command::Attack,
           QStringLiteral("archers"),
           QStringLiteral("archer_targets")),
        at(0.1F,
           Command::Attack,
           QStringLiteral("enemy_archers"),
           QStringLiteral("riders")),
        at(0.1F, Command::Attack, QStringLiteral("ballista"), QStringLiteral("wall")),
        at(0.1F,
           Command::AttackMove,
           QStringLiteral("killer"),
           QStringLiteral("victim")),
        at(0.9F, Command::Run, QStringLiteral("riders")),
    };
    s.steps.back().destination = {16.0F, 0.0F, -14.0F};
    s.capture_ui_overlays = true;
    add_visual_stability(s,
                         {QStringLiteral("swords"), QStringLiteral("sword_targets")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("swords")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("sword_targets")));
    s.expectations.push_back(expectation(
        Expect::GroupHealthReduced, QStringLiteral("sword_targets"), {}, 1.0F));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("archers"),
                                         QStringLiteral("archer_targets")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("archers"),
                                         QStringLiteral("archer_targets")));
    s.expectations.push_back(expectation(
        Expect::GroupHealthReduced, QStringLiteral("archer_targets"), {}, 1.0F));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("enemy_archers"),
                                         QStringLiteral("riders")));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("riders"), {}, 1.5F));
    s.expectations.push_back(expectation(Expect::GroupExists, QStringLiteral("wall")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("wall"), {}, 1.0F));
    s.expectations.push_back(
        expectation(Expect::StructureDamageCueObserved, QStringLiteral("wall")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("victim")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("victim")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_retargeting_id),
                   QStringLiteral("Retargeting"),
                   QStringLiteral("Attacker kills one target and reacquires another."),
                   12.0F,
                   {13.0F, 42.0F, 28.0F});
    auto primary =
        group(QStringLiteral("primary"), Troop::Healer, 2, 1, {-0.9F, 0.0F, 1.8F}, 1);
    primary.health_override = 6;
    primary.max_health_override = 6;
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 1),
        primary,
        group(QStringLiteral("secondary"),
              Troop::Swordsman,
              2,
              1,
              {2.4F, 0.0F, 2.8F},
              1)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("primary")),
               when_destroyed(QStringLiteral("primary"),
                              Command::AttackMove,
                              QStringLiteral("attacker"),
                              QStringLiteral("secondary"))};
    add_visual_stability(s,
                         {QStringLiteral("attacker"),
                          QStringLiteral("primary"),
                          QStringLiteral("secondary")});
    s.expectations.push_back(expectation(Expect::TargetRetakenAfterDeath,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("secondary")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_hold_guard_exit_id),
                   QStringLiteral("Hold / Guard Exit"),
                   QStringLiteral("Hold and guard units exit stance under pressure."),
                   10.0F,
                   {14.0F, 44.0F, 32.0F});
    s.groups = {
        group(QStringLiteral("hold"), Troop::Spearman, 1, 1, {-1.8F, 0.0F, 0.0F}, 4),
        group(QStringLiteral("guard"), Troop::Swordsman, 1, 1, {1.8F, 0.0F, 0.0F}, 4),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 7.0F}, 6)};
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("hold")),
        at(0.0F, Command::Guard, QStringLiteral("guard"), QStringLiteral("hold")),
        at(0.0F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("hold")),
        at(1.0F, Command::Hold, QStringLiteral("hold")),
        at(1.0F, Command::Guard, QStringLiteral("guard"), QStringLiteral("hold")),
        at(1.0F,
           Command::AttackMove,
           QStringLiteral("guard"),
           QStringLiteral("enemy"))};
    s.steps[3].enabled = false;
    s.steps[4].enabled = false;
    add_visual_stability(
        s, {QStringLiteral("hold"), QStringLiteral("guard"), QStringLiteral("enemy")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_testudo_missile_defense_id),
        QStringLiteral("Testudo Missile Defense"),
        QStringLiteral("Roman swordsmen answer a guard order by forming the testudo "
                       "while Carthaginian archers shoot into their shield face."),
        16.0F,
        {13.0F, 28.0F, 55.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.groups = {group(QStringLiteral("legion"),
                      Troop::Swordsman,
                      1,
                      6,
                      {-3.0F, 0.0F, -3.0F},
                      6,
                      {2.4F, 0.0F, 0.0F}),
                group(QStringLiteral("archers"),
                      Troop::Archer,
                      2,
                      4,
                      {-2.4F, 0.0F, 3.0F},
                      6,
                      {2.4F, 0.0F, 0.0F})};
    s.steps = {
        at(0.5F, Command::Guard, QStringLiteral("legion"), QStringLiteral("archers")),
        at(3.5F, Command::Attack, QStringLiteral("archers"), QStringLiteral("legion"))};
    s.steps[1].chase = false;
    add_visual_stability(s, {QStringLiteral("legion"), QStringLiteral("archers")});
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("legion")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("legion")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_shield_wall_cavalry_impact_id),
        QStringLiteral("Shield Wall Cavalry Impact"),
        QStringLiteral("Carthaginian citizen infantry hold a shield wall on a guard "
                       "order and receive a Roman cavalry charge on the shield face."),
        16.0F,
        {12.0F, 26.0F, 55.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.groups = {group(QStringLiteral("wall"),
                      Troop::Swordsman,
                      2,
                      5,
                      {-4.8F, 0.0F, -2.0F},
                      6,
                      {2.4F, 0.0F, 0.0F}),
                group(QStringLiteral("cavalry"),
                      Troop::MountedSwordsman,
                      1,
                      3,
                      {-2.4F, 0.0F, 12.0F},
                      4,
                      {2.6F, 0.0F, 0.0F})};
    s.steps = {
        at(0.5F, Command::Guard, QStringLiteral("wall"), QStringLiteral("cavalry")),
        at(3.5F, Command::Charge, QStringLiteral("cavalry"), QStringLiteral("wall"))};
    add_visual_stability(s, {QStringLiteral("wall"), QStringLiteral("cavalry")});
    s.expectations.push_back(expectation(Expect::GroupExists, QStringLiteral("wall")));
    s.expectations.push_back(
        expectation(Expect::AttackHasVisibleContact, QStringLiteral("cavalry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_hold_stance_review_id),
        QStringLiteral("Hold Stance Review"),
        QStringLiteral("Side-on close review of the kneeling hold stance. A spearman "
                       "braces its point down the approach lane, an archer keeps the "
                       "bow ready, and a swordsman proves the stance stays limited to "
                       "archers and spearmen."),
        9.0F,
        {6.4F, 18.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.4F);
    s.groups = {
        group(QStringLiteral("spear"), Troop::Spearman, 1, 1, {-2.2F, 0.0F, -1.2F}, 1),
        group(QStringLiteral("archer"), Troop::Archer, 1, 1, {0.0F, 0.0F, -1.2F}, 1),
        group(QStringLiteral("sword"), Troop::Swordsman, 1, 1, {2.2F, 0.0F, -1.2F}, 1),
        group(QStringLiteral("threat"),
              Troop::Civilian,
              2,
              3,
              {-2.2F, 0.0F, 2.0F},
              1,
              {2.2F, 0.0F, 0.0F})};
    s.groups[3].health_override = 5000;
    s.groups[3].max_health_override = 5000;
    s.steps = {at(2.0F, Command::Hold, QStringLiteral("spear")),
               at(2.0F, Command::Hold, QStringLiteral("archer")),
               at(2.0F, Command::Hold, QStringLiteral("sword"))};
    add_visual_stability(
        s,
        {QStringLiteral("spear"), QStringLiteral("archer"), QStringLiteral("sword")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("threat")));
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("spear"), {}, 0.0F, 4.5F));
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("archer"), {}, 0.0F, 4.5F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_hold_toggle_cycle_id),
        QStringLiteral("Hold Toggle Cycle"),
        QStringLiteral("Archers and spearmen enter and leave the hold stance three "
                       "times in a row, including re-entry while they are still "
                       "standing up, so the kneel blend can be reviewed for pops."),
        11.0F,
        {7.2F, 26.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.groups = {
        group(QStringLiteral("spear"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -1.6F}, 1),
        group(QStringLiteral("archer"), Troop::Archer, 1, 1, {0.0F, 0.0F, 1.6F}, 1)};
    const float toggle_times[] = {0.4F, 2.4F, 5.0F, 7.0F, 7.4F, 9.4F};
    bool enable = true;
    for (float time : toggle_times) {
      for (auto const& name : {QStringLiteral("spear"), QStringLiteral("archer")}) {
        auto step = at(time, Command::Hold, name);
        step.enabled = enable;
        s.steps.push_back(std::move(step));
      }
      enable = !enable;
    }
    add_visual_stability(s, {QStringLiteral("spear"), QStringLiteral("archer")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_hold_transition_interrupts_id),
        QStringLiteral("Hold Transition Interrupts"),
        QStringLiteral("Every interruption of the hold stance in one run: a move "
                       "order during the kneel, an attack order during the kneel, a "
                       "melee push against the kneeling line, and a death while the "
                       "stance is still held."),
        14.0F,
        {12.0F, 32.0F, 60.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 1.0F);
    s.groups = {
        group(QStringLiteral("spear"), Troop::Spearman, 1, 1, {-2.4F, 0.0F, -1.0F}, 1),
        group(QStringLiteral("archer"), Troop::Archer, 1, 1, {2.4F, 0.0F, -1.0F}, 1),
        group(QStringLiteral("doomed"), Troop::Archer, 1, 1, {0.0F, 0.0F, -1.0F}, 1),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 8.0F}, 4)};
    s.groups[2].health_override = 30;

    s.steps = {
        at(0.3F, Command::Hold, QStringLiteral("spear")),
        at(0.3F, Command::Hold, QStringLiteral("archer")),
        at(0.3F, Command::Hold, QStringLiteral("doomed")),

        at(1.0F, Command::Move, QStringLiteral("spear")),

        at(3.0F, Command::Hold, QStringLiteral("spear")),
        at(3.8F,
           Command::AttackMove,
           QStringLiteral("archer"),
           QStringLiteral("enemy")),

        at(5.0F, Command::Hold, QStringLiteral("archer")),
        at(5.4F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("spear")),
        at(8.0F,
           Command::ApplyDamage,
           QStringLiteral("doomed"),
           QStringLiteral("enemy"))};
    s.steps[3].destination = QVector3D(-2.4F, 0.0F, -5.0F);
    s.steps.back().value = 400;

    add_visual_stability(
        s,
        {QStringLiteral("spear"), QStringLiteral("archer"), QStringLiteral("enemy")});
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("doomed")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_lod_switch_id),
                   QStringLiteral("LOD Switch"),
                   QStringLiteral("Dense battle with near/far camera LOD transitions."),
                   8.0F,
                   {34.0F, 52.0F, 24.0F});
    s.groups = {group(QStringLiteral("blue"),
                      Troop::Swordsman,
                      1,
                      5,
                      {0.0F, 0.0F, -10.0F},
                      20,
                      {5.0F, 0.0F, 0.0F}),
                group(QStringLiteral("red"),
                      Troop::Swordsman,
                      2,
                      5,
                      {0.0F, 0.0F, 10.0F},
                      20,
                      {5.0F, 0.0F, 0.0F})};
    s.steps = {
        at(0.0F, Command::SetFullCreatureLod),
        at(0.0F, Command::AttackMove, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::AttackMove, QStringLiteral("red"), QStringLiteral("blue")),
        at(1.5F, Command::SetCamera),
        at(3.0F, Command::SetCamera)};
    s.steps[0].enabled = false;
    s.steps[3].camera_distance = 12.0F;
    s.steps[3].camera_angle = 44.0F;
    s.steps[3].camera_yaw = 28.0F;
    s.steps[4].camera_distance = 34.0F;
    s.steps[4].camera_angle = 52.0F;
    s.steps[4].camera_yaw = 24.0F;
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_three_swords_vs_two_spears_id),
        QStringLiteral("3 Swords vs 2 Spears"),
        QStringLiteral("Unequal groups meet; no eligible unit may stay idle."),
        14.0F,
        {22.0F, 48.0F, 28.0F});
    s.groups = {
        group(
            QStringLiteral("blue_swords"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, -9.0F}),
        group(QStringLiteral("red_spears"), Troop::Spearman, 2, 2, {0.0F, 0.0F, 9.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_swords"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_swords"), QStringLiteral("red_spears")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("blue_swords"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("red_spears"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("blue_swords"),
                                         QStringLiteral("red_spears"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("red_spears"),
                                         QStringLiteral("blue_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_multi_front_melee_id),
        QStringLiteral("Multi-front Melee"),
        QStringLiteral("A durable infantry unit holds its original front while a "
                       "second formation joins from the flank; soldier assignments "
                       "must split without resetting the existing fight."),
        14.0F,
        {18.0F, 50.0F, 28.0F});
    auto defender = group(
        QStringLiteral("defender"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.0F}, 12);
    auto front =
        group(QStringLiteral("front"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -7.0F}, 12);
    auto flank =
        group(QStringLiteral("flank"), Troop::Spearman, 1, 1, {-8.0F, 0.0F, 2.0F}, 12);
    defender.health_override = defender.max_health_override = 2400;
    front.health_override = front.max_health_override = 1800;
    flank.health_override = flank.max_health_override = 1800;
    s.groups = {defender, front, flank};
    s.steps = {
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("front"),
           QStringLiteral("defender")),
        at(0.25F, Command::Attack, QStringLiteral("defender"), QStringLiteral("front")),
        at(3.0F,
           Command::AttackMove,
           QStringLiteral("flank"),
           QStringLiteral("defender")),
    };
    add_visual_stability(
        s,
        {QStringLiteral("defender"), QStringLiteral("front"), QStringLiteral("flank")});
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("defender")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("front")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("flank")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("front"),
                                         QStringLiteral("defender")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("flank"),
                                         QStringLiteral("defender")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_survivor_compaction_id),
        QStringLiteral("Melee Survivor Compaction"),
        QStringLiteral("A formation takes a controlled casualty burst during melee; "
                       "the final two living soldiers must step into a compact pair "
                       "while corpses remain at their impact anchors."),
        10.0F,
        {12.0F, 44.0F, 20.0F});
    auto survivors = group(
        QStringLiteral("survivors"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.0F}, 12);
    auto attacker = group(
        QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -5.0F}, 12);
    survivors.health_override = survivors.max_health_override = 12000;
    attacker.health_override = attacker.max_health_override = 12000;
    s.groups = {survivors, attacker};
    s.steps = {
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("attacker"),
           QStringLiteral("survivors")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("survivors"),
           QStringLiteral("attacker")),
        at(3.0F,
           Command::ApplyDamage,
           QStringLiteral("survivors"),
           QStringLiteral("attacker")),
    };
    s.steps.back().value = 10000;
    add_visual_stability(s, {QStringLiteral("survivors"), QStringLiteral("attacker")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("survivors"),
                                         QStringLiteral("attacker")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("survivors")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_spear_walk_contact_id),
        QStringLiteral("Spear Walk Contact"),
        QStringLiteral("Spearmen walk into swords without root jets or falls."),
        12.0F,
        {19.0F, 46.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("spears"), Troop::Spearman, 1, 2, {0.0F, 0.0F, -8.0F}),
        group(QStringLiteral("swords"), Troop::Swordsman, 2, 2, {0.0F, 0.0F, 4.0F})};
    s.steps = {
        at(0.25F, Command::FormationMove, QStringLiteral("spears")),
        at(2.0F,
           Command::AttackMove,
           QStringLiteral("spears"),
           QStringLiteral("swords")),
        at(2.0F, Command::Attack, QStringLiteral("swords"), QStringLiteral("spears"))};
    s.steps[0].destination = {0.0F, 0.0F, 0.0F};
    add_visual_stability(s, {QStringLiteral("spears"), QStringLiteral("swords")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("spears")));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("spears"), {}, 0.8F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("spears"),
                                         QStringLiteral("swords")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::CombatIndicatorIsContinuous, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::CombatIndicatorIsContinuous, QStringLiteral("swords")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_archer_stability_id),
                   QStringLiteral("Archer Stability"),
                   QStringLiteral("Three standing archers fire without pose epilepsy."),
                   12.0F,
                   {23.0F, 48.0F, 35.0F});
    s.groups = {
        group(QStringLiteral("archers"), Troop::Archer, 1, 3, {0.0F, 0.0F, -7.0F}, 10),
        group(QStringLiteral("infantry"),
              Troop::Swordsman,
              2,
              2,
              {0.0F, 0.0F, 7.0F},
              10)};
    for (auto& infantry : s.groups) {
      if (infantry.name == QStringLiteral("infantry")) {
        infantry.health_override = 2000;
        infantry.max_health_override = 2000;
      }
    }
    s.steps = {at(
        0.5F, Command::Attack, QStringLiteral("archers"), QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("archers"), QStringLiteral("infantry")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("archers"),
                                         QStringLiteral("infantry")));
    s.expectations.push_back(expectation(
        Expect::RepeatedAttackAnimationObserved, QStringLiteral("archers"), {}, 2.0F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("archers"), {}, 0.9F));
    result.push_back(std::move(s));
  }

  {

    auto s = definition(QString::fromLatin1(k_infantry_idle_ambient_id),
                        QStringLiteral("Infantry Idle Ambient"),
                        QStringLiteral("Swordsmen, spearmen and archers stand at ease "
                                       "long enough to breathe and to play ambient "
                                       "idles, easing in and out of the idle cycle."),
                        70.0F,
                        {7.5F, 18.0F, 18.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {-3.0F, 0.0F, 0.0F}, 3),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {0.0F, 0.0F, 0.0F}, 3),
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {3.0F, 0.0F, 0.0F}, 3)};
    s.steps = {at(0.25F, Command::Stand, QStringLiteral("swords")),
               at(0.25F, Command::Stand, QStringLiteral("spears")),
               at(0.25F, Command::Stand, QStringLiteral("archers"))};
    add_visual_stability(s,
                         {QStringLiteral("swords"),
                          QStringLiteral("spears"),
                          QStringLiteral("archers")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_idle_ambient_id),
                        QStringLiteral("Mounted Idle Ambient"),
                        QStringLiteral("Riders sit a long halt: the saddle stays "
                                       "aligned while mounted ambient idles play, and "
                                       "no rider tries to squat on the ground."),
                        70.0F,
                        {17.0F, 26.0F, 20.0F});
    s.groups = {group(QStringLiteral("swordsmans"),
                      Troop::MountedSwordsman,
                      1,
                      1,
                      {-5.0F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("horse_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {5.0F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.25F, Command::Stand, QStringLiteral("swordsmans")),
               at(0.25F, Command::Stand, QStringLiteral("horse_archers"))};
    add_visual_stability(
        s, {QStringLiteral("swordsmans"), QStringLiteral("horse_archers")});
    result.push_back(std::move(s));
  }

  {

    auto s = definition(QString::fromLatin1(k_idle_ambient_interrupt_id),
                        QStringLiteral("Idle Ambient Interrupt"),
                        QStringLiteral("Repeated move orders arrive while ambient "
                                       "idles are running; every interruption has to "
                                       "blend out instead of snapping."),
                        60.0F,
                        {20.0F, 32.0F, 20.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {-4.0F, 0.0F, 0.0F}, 6),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {4.0F, 0.0F, 0.0F}, 6)};
    s.steps = {at(0.25F, Command::Stand, QStringLiteral("swords")),
               at(0.25F, Command::Stand, QStringLiteral("spears")),
               at(14.0F, Command::Move, QStringLiteral("swords")),
               at(24.0F, Command::Move, QStringLiteral("swords")),
               at(34.0F, Command::Move, QStringLiteral("spears")),
               at(44.0F, Command::Move, QStringLiteral("spears")),
               at(52.0F, Command::Move, QStringLiteral("swords"))};
    s.steps[2].destination = {-4.0F, 0.0F, -7.0F};
    s.steps[3].destination = {-4.0F, 0.0F, 3.0F};
    s.steps[4].destination = {4.0F, 0.0F, -7.0F};
    s.steps[5].destination = {4.0F, 0.0F, 3.0F};
    s.steps[6].destination = {-4.0F, 0.0F, -5.0F};
    add_visual_stability(s, {QStringLiteral("swords"), QStringLiteral("spears")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_archer_melee_lock_id),
        QStringLiteral("Archers Forced Into Melee"),
        QStringLiteral("Swordsmen close through bow fire; locked archers stop "
                       "shooting and fight with a distinct two-handed bow strike."),
        12.0F,
        {18.0F, 48.0F, 24.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -7.0F}, 6),
        group(QStringLiteral("archers"), Troop::Archer, 2, 2, {0.0F, 0.0F, 4.0F}, 6)};
    for (auto& formation : s.groups) {
      formation.health_override = 1200;
      formation.max_health_override = 1200;
    }
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("archers")),
               at(0.35F,
                  Command::Attack,
                  QStringLiteral("swords"),
                  QStringLiteral("archers"))};
    add_visual_stability(s, {QStringLiteral("swords"), QStringLiteral("archers")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("archers")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("archers"),
                                         QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("archers")));
    s.expectations.push_back(expectation(
        Expect::RepeatedAttackAnimationObserved, QStringLiteral("archers"), {}, 2.0F));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("archers")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_infantry_charge_id),
                   QStringLiteral("Infantry Charge"),
                   QStringLiteral("Three infantry charge two stationary defenders."),
                   12.0F,
                   {22.0F, 48.0F, 25.0F});
    s.groups = {
        group(QStringLiteral("chargers"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, -11.0F}),
        group(QStringLiteral("defenders"), Troop::Spearman, 2, 2, {0.0F, 0.0F, 6.0F})};
    s.steps = {at(0.5F,
                  Command::Charge,
                  QStringLiteral("chargers"),
                  QStringLiteral("defenders")),
               at(0.5F, Command::Hold, QStringLiteral("defenders")),
               at(4.0F, Command::Hold, QStringLiteral("defenders")),
               at(4.0F,
                  Command::Attack,
                  QStringLiteral("defenders"),
                  QStringLiteral("chargers"))};
    s.steps[2].enabled = false;
    add_visual_stability(s, {QStringLiteral("chargers"), QStringLiteral("defenders")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("chargers"), {}, 0.4F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_flank_ambush_id),
                   QStringLiteral("Flank Ambush"),
                   QStringLiteral("Marching infantry react to a delayed flank ambush."),
                   14.0F,
                   {23.0F, 50.0F, 32.0F});
    auto ambushers = group(QStringLiteral("ambushers"),
                           Troop::Spearman,
                           2,
                           2,
                           {8.0F, 0.0F, 0.0F},
                           10,
                           {0.0F, 0.0F, 2.6F});
    ambushers.spawn_at_start = false;
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      3,
                      {0.0F, 0.0F, -7.0F},
                      10,
                      {0.0F, 0.0F, 2.6F}),
                ambushers};
    s.steps = {at(0.25F, Command::FormationMove, QStringLiteral("column")),
               at(2.5F,
                  Command::SpawnAmbush,
                  QStringLiteral("ambushers"),
                  QStringLiteral("column")),
               at(3.0F,
                  Command::AttackMove,
                  QStringLiteral("column"),
                  QStringLiteral("ambushers"))};
    s.steps[0].destination = {0.0F, 0.0F, 7.0F};
    add_visual_stability(s, {QStringLiteral("column"), QStringLiteral("ambushers")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("column"), {}, 0.55F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_reserve_release_id),
                        QStringLiteral("Reserve Release"),
                        QStringLiteral("Held reserve joins after front-line contact."),
                        15.0F,
                        {25.0F, 50.0F, 28.0F});
    s.groups = {
        group(QStringLiteral("front"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -5.0F}),
        group(QStringLiteral("reserve"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -11.0F}),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 2, {0.0F, 0.0F, 7.0F})};
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("reserve")),
        at(0.5F, Command::AttackMove, QStringLiteral("front"), QStringLiteral("enemy")),
        at(0.5F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("front")),
        at(3.5F, Command::Hold, QStringLiteral("reserve")),
        at(3.5F,
           Command::ReleaseReserve,
           QStringLiteral("reserve"),
           QStringLiteral("enemy"))};
    s.steps[3].enabled = false;
    add_visual_stability(
        s,
        {QStringLiteral("front"), QStringLiteral("reserve"), QStringLiteral("enemy")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("reserve"), {}, 0.55F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("reserve")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("reserve")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("reserve")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mixed_roles_id),
        QStringLiteral("Mixed Roles"),
        QStringLiteral("Two melee groups and archers fight three melee groups."),
        15.0F,
        {27.0F, 52.0F, 32.0F});
    s.groups = {
        group(
            QStringLiteral("blue_melee"), Troop::Swordsman, 1, 2, {-3.0F, 0.0F, -8.0F}),
        group(QStringLiteral("blue_archer"), Troop::Archer, 1, 1, {4.0F, 0.0F, -10.0F}),
        group(QStringLiteral("red_melee"), Troop::Spearman, 2, 3, {0.0F, 0.0F, 8.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_melee"),
                  QStringLiteral("red_melee")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_archer"),
                  QStringLiteral("red_melee")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_melee"),
                  QStringLiteral("blue_melee"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_melee"),
                          QStringLiteral("blue_archer"),
                          QStringLiteral("red_melee")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_bot_skirmish_id),
        QStringLiteral("Bot Skirmish"),
        QStringLiteral("AI-controlled group must make a useful battlefield move."),
        18.0F,
        {24.0F, 50.0F, 28.0F});
    auto bot = group(QStringLiteral("bot"), Troop::Spearman, 2, 3, {0.0F, 0.0F, 8.0F});
    bot.ai_controlled = true;
    s.groups = {
        group(QStringLiteral("player"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -8.0F}),
        bot};
    s.steps = {
        at(0.5F, Command::AttackMove, QStringLiteral("player"), QStringLiteral("bot"))};
    add_visual_stability(s, {QStringLiteral("player"), QStringLiteral("bot")});
    s.expectations.push_back(
        expectation(Expect::BotIssuesUsefulCommand, QStringLiteral("bot")));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
