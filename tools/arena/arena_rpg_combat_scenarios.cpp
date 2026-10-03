#include "arena_rpg_combat_scenarios.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "game/systems/combat_actions/combat_action_definition.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto commander_action_expectation(QString group_name,
                                  Game::Systems::CombatActions::CombatActionId action,
                                  float start,
                                  float end) -> ArenaExpectation {
  auto result = expectation(Expect::CommanderActionObserved, std::move(group_name));
  result.combat_action_id = static_cast<int>(action);
  result.start_seconds = start;
  result.end_seconds = end;
  return result;
}

} // namespace

auto build_rpg_combat_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_one_press_one_attack_id),
        QStringLiteral("RPG Press And Hold Combo"),
        QStringLiteral(
            "Three deliberate attack presses, well clear of each other, then the "
            "attack input held down through a complete sword combo. A tap requests "
            "one attack; a hold continues at authored recovery boundaries and "
            "release prevents another link from starting."),
        8.5F);
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
                           {0.0F, 0.0F, -1.8F},
                           1);
    commander.facing_degrees = 0.0F;
    auto enemy = group(QStringLiteral("training_dummy"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 0.5F},
                       1);
    enemy.health_override = enemy.max_health_override = 6000;
    s.groups = {commander, enemy};

    auto hold_attack = [](float time, bool held) {
      auto step = at(time, Command::RpgAttackHold, QStringLiteral("rpg_commander"));
      step.enabled = held;
      return step;
    };
    s.steps = {
        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(2.10F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(3.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        hold_attack(5.00F, true),
        hold_attack(7.60F, false),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("training_dummy")});

    {
      auto three_presses = expectation(Expect::CommanderCombatCounterWithin,
                                       QStringLiteral("rpg_commander"),
                                       {},
                                       3.0F,
                                       0.0F);
      three_presses.counter_key = QStringLiteral("accepted");
      three_presses.end_seconds = 4.90F;
      three_presses.maximum = 3.0F;
      s.expectations.push_back(three_presses);
    }
    {
      auto held_combo = expectation(Expect::CommanderCombatCounterWithin,
                                    QStringLiteral("rpg_commander"),
                                    {},
                                    3.0F,
                                    4.95F);
      held_combo.counter_key = QStringLiteral("accepted");
      held_combo.end_seconds = 8.5F;
      held_combo.maximum = 5.0F;
      s.expectations.push_back(held_combo);
    }
    {
      auto no_expiry = expectation(Expect::CommanderCombatCounterWithin,
                                   QStringLiteral("rpg_commander"));
      no_expiry.counter_key = QStringLiteral("expired");
      no_expiry.maximum = 0.0F;
      s.expectations.push_back(no_expiry);
    }
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_attack_buffer_window_id),
        QStringLiteral("RPG Attack Buffer Window"),
        QStringLiteral(
            "Timed individual presses against the authored action timeline: one "
            "clean press, one pressed late enough in the previous swing that the "
            "buffer carries it into the next, and one pressed so early that the "
            "buffer must let it expire rather than storing a swing the player has "
            "forgotten about. Replaces the held-input combo cadence case."),
        8.0F);
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
                           {0.0F, 0.0F, -1.8F},
                           1);
    commander.facing_degrees = 0.0F;
    auto enemy = group(QStringLiteral("enemy_formation"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 0.5F},
                       6);
    enemy.health_override = enemy.max_health_override = 6000;
    s.groups = {commander, enemy};

    s.steps = {

        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),

        at(1.02F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),

        at(3.50F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),

        at(3.75F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_formation")});

    {
      auto buffered = expectation(Expect::CommanderCombatCounterWithin,
                                  QStringLiteral("rpg_commander"),
                                  {},
                                  1.0F,
                                  1.00F);
      buffered.counter_key = QStringLiteral("buffered");
      buffered.end_seconds = 2.40F;
      s.expectations.push_back(buffered);
    }
    {
      auto carried = expectation(Expect::CommanderCombatCounterWithin,
                                 QStringLiteral("rpg_commander"),
                                 {},
                                 1.0F,
                                 1.00F);
      carried.counter_key = QStringLiteral("accepted");
      carried.end_seconds = 2.40F;
      carried.maximum = 1.0F;
      s.expectations.push_back(carried);
    }
    {
      auto expired = expectation(Expect::CommanderCombatCounterWithin,
                                 QStringLiteral("rpg_commander"),
                                 {},
                                 1.0F,
                                 3.70F);
      expired.counter_key = QStringLiteral("expired");
      expired.end_seconds = 5.60F;
      s.expectations.push_back(expired);

      auto not_carried = expectation(Expect::CommanderCombatCounterWithin,
                                     QStringLiteral("rpg_commander"),
                                     {},
                                     0.0F,
                                     3.70F);
      not_carried.counter_key = QStringLiteral("accepted");
      not_carried.end_seconds = 5.60F;
      not_carried.maximum = 0.0F;
      s.expectations.push_back(not_carried);
    }
    {
      auto overflow = expectation(Expect::CommanderCombatCounterWithin,
                                  QStringLiteral("rpg_commander"));
      overflow.counter_key = QStringLiteral("overflow");
      overflow.maximum = 0.0F;
      s.expectations.push_back(overflow);
    }
    s.expectations.push_back(expectation(Expect::CommanderContactCountAtMost,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         1.0F));
    s.expectations.push_back(expectation(Expect::RpgStrikeAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgDamageContactObserved,
                                         QStringLiteral("enemy_formation")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_attack_whiff_recovery_id),
        QStringLiteral("RPG Attack Whiff Recovery"),
        QStringLiteral(
            "Two attacks swung at empty air with the nearest enemy well outside "
            "reach. A miss still commits to its authored timeline and recovers "
            "from it, and it must never invent a contact to justify itself."),
        6.0F);
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
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto enemy = group(QStringLiteral("distant_enemy"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 22.0F},
                       1);
    enemy.health_override = enemy.max_health_override = 6000;
    s.groups = {commander, enemy};
    s.steps = {
        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(3.20F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    {
      auto accepted = expectation(Expect::CommanderCombatCounterWithin,
                                  QStringLiteral("rpg_commander"),
                                  {},
                                  2.0F);
      accepted.counter_key = QStringLiteral("accepted");
      accepted.maximum = 2.0F;
      s.expectations.push_back(accepted);
    }
    s.expectations.push_back(expectation(Expect::CommanderContactCountAtMost,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         1.0F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::AttackRecoveryObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::NoActiveCombatAtEnd, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthUnchanged, QStringLiteral("distant_enemy")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_attack_wall_contact_id),
        QStringLiteral("RPG Attack Into A Wall"),
        QStringLiteral(
            "The commander closes on a house with an enemy standing on the far "
            "side of it and swings. Authored root motion has to stop at the "
            "facade with the rest of the motor's rules, and the weapon trace may "
            "never reach a body through a structure."),
        8.0F);
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
                           {0.0F, 0.0F, -4.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto home = building(QStringLiteral("wall_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {0.0F, 0.0F, 0.0F});
    home.health_override = home.max_health_override = 8000;
    auto enemy = group(QStringLiteral("sheltered_enemy"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 2.6F},
                       1);
    enemy.health_override = enemy.max_health_override = 6000;
    s.groups = {commander, home, enemy};

    auto move =
        [](float time, QVector3D axes, bool run, std::optional<float> yaw = {}) {
          auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
          step.destination = axes;
          step.value = run ? 1 : 0;
          step.rpg_view_yaw_degrees = yaw;
          return step;
        };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, false, 0.0F),
        move(2.60F, {0.0F, 0.0F, 0.0F}, false),
        at(3.00F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(5.00F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    {
      auto accepted = expectation(Expect::CommanderCombatCounterWithin,
                                  QStringLiteral("rpg_commander"),
                                  {},
                                  2.0F);
      accepted.counter_key = QStringLiteral("accepted");
      accepted.maximum = 2.0F;
      s.expectations.push_back(accepted);
    }
    s.expectations.push_back(
        expectation(Expect::GroupHealthUnchanged, QStringLiteral("sheltered_enemy")));
    s.expectations.push_back(expectation(Expect::CommanderContactCountAtMost,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         1.0F));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_stamina_refusal_id),
        QStringLiteral("RPG Stamina Refusal And Recovery"),
        QStringLiteral(
            "Attacks pressed until the stamina pool cannot pay for one, then a "
            "pause, then one more. Every insufficient-stamina press has to come "
            "back as a counted refusal rather than a dead click, and the pool has "
            "to recover enough to swing again."),
        14.0F);
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
                           {0.0F, 0.0F, -1.8F},
                           1);
    commander.facing_degrees = 0.0F;
    auto enemy = group(QStringLiteral("training_dummy"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 0.5F},
                       1);
    enemy.health_override = enemy.max_health_override = 9000;
    s.groups = {commander, enemy};

    s.steps.reserve(14);
    for (int index = 0; index < 12; ++index) {
      s.steps.push_back(at(0.60F + (0.85F * static_cast<float>(index)),
                           Command::RpgPrimaryAttack,
                           QStringLiteral("rpg_commander")));
    }
    s.steps.push_back(
        at(13.20F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")));
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("training_dummy")});

    {
      auto refused = expectation(Expect::CommanderCombatCounterWithin,
                                 QStringLiteral("rpg_commander"),
                                 {},
                                 1.0F);
      refused.counter_key = QStringLiteral("refused");
      refused.end_seconds = 11.0F;
      s.expectations.push_back(refused);
    }
    {
      auto recovered = expectation(Expect::CommanderCombatCounterWithin,
                                   QStringLiteral("rpg_commander"),
                                   {},
                                   1.0F,
                                   13.10F);
      recovered.counter_key = QStringLiteral("accepted");
      recovered.end_seconds = 14.0F;
      s.expectations.push_back(recovered);
    }
    s.expectations.push_back(expectation(Expect::CommanderContactCountAtMost,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         1.0F));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_skirmish_three_attackers_id),
        QStringLiteral("RPG Three Attacker Skirmish"),
        QStringLiteral(
            "Three swordsmen close on a guarding commander from three sides. The "
            "engagement ring decides who may press, and the commander has to keep "
            "getting readable openings: a crowd may not chain unavoidable contacts "
            "or hold him staggered until he dies."),
        12.0F);
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
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto front = group(QStringLiteral("attacker_front"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 3.2F},
                       1);
    auto left = group(QStringLiteral("attacker_left"),
                      Troop::Swordsman,
                      2,
                      1,
                      {-3.0F, 0.0F, 1.4F},
                      1);
    auto right = group(QStringLiteral("attacker_right"),
                       Troop::Swordsman,
                       2,
                       1,
                       {3.0F, 0.0F, 1.4F},
                       1);
    front.health_override = front.max_health_override = 4000;
    left.health_override = left.max_health_override = 4000;
    right.health_override = right.max_health_override = 4000;
    s.groups = {commander, front, left, right};

    auto guard = [](float time, bool enabled) {
      auto step = at(time, Command::RpgGuard, QStringLiteral("rpg_commander"));
      step.enabled = enabled;
      return step;
    };
    s.steps = {
        guard(0.20F, true),
        at(0.40F,
           Command::Attack,
           QStringLiteral("attacker_front"),
           QStringLiteral("rpg_commander")),
        at(0.40F,
           Command::Attack,
           QStringLiteral("attacker_left"),
           QStringLiteral("rpg_commander")),
        at(0.40F,
           Command::Attack,
           QStringLiteral("attacker_right"),
           QStringLiteral("rpg_commander")),
        at(4.00F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(6.00F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(8.00F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(s,
                         {QStringLiteral("rpg_commander"),
                          QStringLiteral("attacker_front"),
                          QStringLiteral("attacker_left"),
                          QStringLiteral("attacker_right")});

    {
      auto swings = expectation(Expect::CommanderCombatCounterWithin,
                                QStringLiteral("rpg_commander"),
                                {},
                                2.0F,
                                3.90F);
      swings.counter_key = QStringLiteral("accepted");
      swings.end_seconds = 12.0F;
      s.expectations.push_back(swings);
    }
    s.expectations.push_back(expectation(Expect::CommanderContactCountAtMost,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         1.0F));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_lock_cycle_id),
        QStringLiteral("RPG Lock Cycle, Occlusion And Death"),
        QStringLiteral(
            "A third enemy stands directly behind a house on the view centre while "
            "two others stand clear of it. Lock-on must refuse the occluded one, "
            "cycle across the screen between the two it can see, and drop the lock "
            "when the locked target dies rather than falling through to the one it "
            "cannot see."),
        7.0F);
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
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto shelter = building(QStringLiteral("lock_shelter"),
                            Game::Units::SpawnType::Home,
                            Nation::RomanRepublic,
                            1,
                            1,
                            {0.0F, 0.0F, 5.0F});
    shelter.health_override = shelter.max_health_override = 8000;
    auto hidden = group(
        QStringLiteral("hidden_enemy"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 7.6F}, 1);
    auto left = group(
        QStringLiteral("left_enemy"), Troop::Swordsman, 2, 1, {-1.6F, 0.0F, 6.0F}, 1);
    auto right = group(
        QStringLiteral("right_enemy"), Troop::Swordsman, 2, 1, {4.0F, 0.0F, 6.0F}, 1);
    hidden.health_override = hidden.max_health_override = 4000;
    left.health_override = left.max_health_override = 4000;
    right.health_override = right.max_health_override = 4000;
    s.groups = {commander, shelter, hidden, left, right};

    auto kill = [](float time, QString target) {
      auto step = at(time, Command::SetHealth, std::move(target));
      step.value = 0;
      return step;
    };
    s.steps = {
        at(0.50F, Command::RpgCycleLockOn, QStringLiteral("rpg_commander")),
        at(1.60F, Command::RpgCycleLockOn, QStringLiteral("rpg_commander")),
        kill(3.00F, QStringLiteral("left_enemy")),
        kill(3.00F, QStringLiteral("right_enemy")),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    {
      auto held = expectation(Expect::CommanderLockStateWithin,
                              QStringLiteral("rpg_commander"),
                              {},
                              0.0F,
                              0.70F);
      held.counter_key = QStringLiteral("held");
      held.end_seconds = 2.90F;
      s.expectations.push_back(held);
    }
    {
      auto changed = expectation(Expect::CommanderLockStateWithin,
                                 QStringLiteral("rpg_commander"),
                                 {},
                                 0.0F,
                                 1.50F);
      changed.counter_key = QStringLiteral("changed");
      changed.end_seconds = 2.90F;
      s.expectations.push_back(changed);
    }
    {
      auto cleared = expectation(Expect::CommanderLockStateWithin,
                                 QStringLiteral("rpg_commander"),
                                 {},
                                 0.0F,
                                 4.00F);
      cleared.counter_key = QStringLiteral("cleared");
      cleared.end_seconds = 7.00F;
      s.expectations.push_back(cleared);
    }
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_bow_volley_id),
        QStringLiteral("RPG Bow Volley"),
        QStringLiteral(
            "Behind-head bow commander against nine charging swordsmen. Every shot "
            "is drawn, held at full draw, and loosed along the aim ray with the "
            "crosshair solved from the live chase camera, so each arrow has to "
            "leave the bow where the reticle was sitting and drop one attacker "
            "before the line reaches him."),
        14.6F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.arena_floor_half_extent = 30.0F;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 16.2F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanFieldCommander,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;

    auto charger = [](QString name, QVector3D origin) {
      auto enemy = group(std::move(name), Troop::Swordsman, 2, 1, origin, 1);
      enemy.health_override = enemy.max_health_override = 70;
      return enemy;
    };
    std::array<QString, 9> const charger_names{QStringLiteral("charger_centre"),
                                               QStringLiteral("charger_left"),
                                               QStringLiteral("charger_right"),
                                               QStringLiteral("charger_far_left"),
                                               QStringLiteral("charger_far_right"),
                                               QStringLiteral("charger_deep_left"),
                                               QStringLiteral("charger_deep_right"),
                                               QStringLiteral("charger_flank_left"),
                                               QStringLiteral("charger_flank_right")};

    std::array<QVector3D, 9> const charger_origins{QVector3D{0.4F, 0.0F, 12.0F},
                                                   QVector3D{-4.2F, 0.0F, 12.0F},
                                                   QVector3D{4.4F, 0.0F, 12.0F},
                                                   QVector3D{-7.6F, 0.0F, 11.0F},
                                                   QVector3D{7.8F, 0.0F, 11.0F},
                                                   QVector3D{-2.6F, 0.0F, 14.5F},
                                                   QVector3D{2.8F, 0.0F, 14.5F},
                                                   QVector3D{-9.5F, 0.0F, 13.0F},
                                                   QVector3D{9.6F, 0.0F, 13.0F}};
    s.groups = {commander};
    for (std::size_t i = 0; i < charger_names.size(); ++i) {
      s.groups.push_back(charger(charger_names[i], charger_origins[i]));
    }

    auto draw = [](float time, bool held) {
      auto step = at(time, Command::RpgAttackHold, QStringLiteral("rpg_commander"));
      step.enabled = held;
      return step;
    };
    auto aim_at = [](float time, const QString& target) {
      return at(time, Command::RpgAim, QStringLiteral("rpg_commander"), target);
    };

    constexpr float k_first_shot = 0.55F;
    constexpr float k_shot_interval = 1.45F;
    for (std::size_t i = 0; i < charger_names.size(); ++i) {
      float const shot = k_first_shot + (k_shot_interval * static_cast<float>(i));
      s.steps.push_back(at(std::max(0.10F, shot - 3.20F),
                           Command::Attack,
                           charger_names[i],
                           QStringLiteral("rpg_commander")));

      s.steps.push_back(aim_at(shot, charger_names[i]));
      s.steps.push_back(draw(shot + 0.16F, true));
      s.steps.push_back(draw(shot + 0.86F, false));
    }

    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("rpg_commander"),
                                         charger_names[0]));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("rpg_commander"),
                                         charger_names[0]));
    for (auto const& name : charger_names) {
      s.expectations.push_back(expectation(Expect::GroupDestroyed, name));
    }
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_strike_lunge_id),
        QStringLiteral("RPG Strike Lunge"),
        QStringLiteral(
            "Behind-head commander attacks an enemy standing just outside his "
            "planted reach, with no movement input at all. A swing has to carry "
            "the body into the target: from a planted stance the strike whiffs "
            "at the edge of reach and the fight reads as two puppets waving at "
            "each other."),
        6.0F);
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
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;

    auto enemy = group(
        QStringLiteral("enemy_target"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.6F}, 1);
    enemy.facing_degrees = 180.0F;
    enemy.health_override = enemy.max_health_override = 4000;
    s.groups = {commander, enemy};

    s.steps = {
        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(2.20F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(3.80F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_target")});

    auto lunge = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 0.30F, 0.50F);
    lunge.end_seconds = 5.50F;
    s.expectations.push_back(lunge);

    auto carry =
        expectation(Expect::RpgSwingCarriesBody, QStringLiteral("rpg_commander"));
    carry.threshold = 0.30F;
    carry.distance = 3.0F;
    s.expectations.push_back(carry);
    s.expectations.push_back(
        expectation(Expect::RpgDamageContactObserved, QStringLiteral("enemy_target")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_target")));
    s.expectations.push_back(expectation(Expect::RpgStrikeAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_commander_sword_grammar_id),
        QStringLiteral("RPG Commander Sword Grammar"),
        QStringLiteral(
            "A durable close formation gives the direct-control sword commander "
            "room to demonstrate his gap closer, launcher branch, radial special, "
            "air attack, dive, defensive cancel, and full light chain."),
        11.8F);
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
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    commander.stamina_override = 600.0F;
    commander.max_stamina_override = 600.0F;
    auto targets = group(
        QStringLiteral("enemy_crowd"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 3.8F}, 5);
    targets.facing_degrees = 180.0F;
    targets.health_override = targets.max_health_override = 12000;
    targets.attacks_disabled = true;
    s.groups = {commander, targets};
    s.steps = {
        at(0.10F,
           Command::RpgAim,
           QStringLiteral("rpg_commander"),
           QStringLiteral("enemy_crowd")),
        at(0.45F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(1.08F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(1.66F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(2.75F, Command::RpgSpecial, QStringLiteral("rpg_commander")),
        at(3.85F, Command::RpgJump, QStringLiteral("rpg_commander")),
        at(4.52F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        [] {
          auto step = at(5.35F, Command::RpgDodge, QStringLiteral("rpg_commander"));
          step.destination = {1.0F, 0.0F, 0.0F};
          return step;
        }(),
        at(6.10F, Command::RpgGuard, QStringLiteral("rpg_commander")),
        at(6.18F, Command::RpgSpecial, QStringLiteral("rpg_commander")),
        [] {
          auto step = at(6.55F, Command::RpgGuard, QStringLiteral("rpg_commander"));
          step.enabled = false;
          return step;
        }(),
        at(7.05F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(8.23F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(8.95F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(9.55F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(10.18F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_crowd")));
    {
      auto advance = expectation(
          Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 1.0F, 0.55F);
      advance.end_seconds = 9.6F;
      s.expectations.push_back(advance);
    }
    {

      auto carry =
          expectation(Expect::RpgSwingCarriesBody, QStringLiteral("rpg_commander"));
      carry.threshold = 0.45F;
      carry.distance = 4.0F;
      s.expectations.push_back(carry);
    }
    using Action = Game::Systems::CombatActions::CombatActionId;
    s.expectations.push_back(
        commander_action_expectation(QStringLiteral("rpg_commander"),
                                     Action::CommanderSwordGapCloser,
                                     0.35F,
                                     1.35F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordLauncher, 1.45F, 2.80F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordSpin, 2.60F, 3.85F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordAirLight, 3.80F, 4.75F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordDive, 4.35F, 5.75F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSwordSlashLeft, 6.95F, 10.80F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSwordSlashRight, 6.95F, 10.80F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordSpin, 6.95F, 10.80F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSwordFinisher, 6.95F, 10.80F));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_commander_duel_standoff_id),
        QStringLiteral("RPG Commander Duel Standoff"),
        QStringLiteral(
            "Scipio in direct control, locked on to Hannibal, runs his whole sword "
            "grammar -- gap closer, light chain, launcher, radial special, air "
            "attack and dive -- while Hannibal fights back. The two bodies have "
            "to keep a sword's reach apart the whole time instead of standing "
            "inside each other."),
        13.0F,
        {7.5F, 30.0F, 90.0F});
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.9F, 0.0F);
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, -3.0F},
                           1);
    commander.facing_degrees = 0.0F;
    commander.health_override = commander.max_health_override = 9000;
    auto hannibal = group(QStringLiteral("hannibal"),
                          Troop::CarthageSwordCommander,
                          2,
                          1,
                          {0.0F, 0.0F, 3.0F},
                          1);
    hannibal.facing_degrees = 180.0F;
    hannibal.health_override = hannibal.max_health_override = 9000;
    s.groups = {commander, hannibal};
    s.steps = {
        at(0.30F,
           Command::Attack,
           QStringLiteral("hannibal"),
           QStringLiteral("rpg_commander")),
        at(0.60F, Command::RpgCycleLockOn, QStringLiteral("rpg_commander")),

        at(1.00F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(1.85F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(6.00F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(6.75F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(7.35F, Command::RpgJump, QStringLiteral("rpg_commander")),
        at(7.80F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(11.20F, Command::RpgSpecial, QStringLiteral("rpg_commander")),
    };
    {

      auto apart = expectation(Expect::GroupPairKeepsApart,
                               QStringLiteral("rpg_commander"),
                               QStringLiteral("hannibal"),
                               0.0F,
                               1.0F,
                               1.10F);
      s.expectations.push_back(apart);
    }
    s.expectations.push_back(expectation(Expect::RpgApproachWithin,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("hannibal"),
                                         0.0F,
                                         0.0F,
                                         2.0F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("hannibal")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("hannibal")));
    using Action = Game::Systems::CombatActions::CombatActionId;
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordGapCloser, 0.9F, 1.9F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSwordSlashLeft, 1.8F, 2.7F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordLauncher, 6.6F, 7.7F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordAirLight, 7.2F, 8.2F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordDive, 7.6F, 9.2F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSwordSpin, 11.1F, 12.4F));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_commander_spear_grammar_id),
        QStringLiteral("RPG Commander Spear Grammar"),
        QStringLiteral(
            "A direct-control spear commander demonstrates the step-thrust chain, "
            "long gap closer, launcher, crowd sweep, aerial thrust, and diving "
            "finisher against a durable formation."),
        10.8F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanLegionOrganizer,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    commander.stamina_override = 600.0F;
    commander.max_stamina_override = 600.0F;
    auto targets = group(
        QStringLiteral("enemy_crowd"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 4.8F}, 5);
    targets.facing_degrees = 180.0F;
    targets.health_override = targets.max_health_override = 12000;
    targets.attacks_disabled = true;
    s.groups = {commander, targets};
    s.steps = {
        at(0.10F,
           Command::RpgAim,
           QStringLiteral("rpg_commander"),
           QStringLiteral("enemy_crowd")),
        at(0.45F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(1.10F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(1.68F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(2.26F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(3.30F, Command::RpgSpecial, QStringLiteral("rpg_commander")),
        at(4.35F, Command::RpgJump, QStringLiteral("rpg_commander")),
        at(4.44F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(5.02F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        [] {
          auto step = at(5.85F, Command::RpgDodge, QStringLiteral("rpg_commander"));
          step.destination = {-1.0F, 0.0F, 0.0F};
          return step;
        }(),
        at(6.35F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(6.88F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(7.48F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(8.18F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_crowd")));
    {

      auto advance = expectation(
          Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 1.1F, 0.70F);
      advance.end_seconds = 5.0F;
      s.expectations.push_back(advance);
    }
    {

      auto carry =
          expectation(Expect::RpgSwingCarriesBody, QStringLiteral("rpg_commander"));
      carry.threshold = 0.45F;
      carry.distance = 4.0F;
      s.expectations.push_back(carry);
    }
    using Action = Game::Systems::CombatActions::CombatActionId;
    s.expectations.push_back(
        commander_action_expectation(QStringLiteral("rpg_commander"),
                                     Action::CommanderSpearGapCloser,
                                     0.35F,
                                     1.40F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSpearThrust, 0.95F, 1.90F));
    s.expectations.push_back(
        commander_action_expectation(QStringLiteral("rpg_commander"),
                                     Action::CommanderSpearStepThrust,
                                     1.45F,
                                     2.45F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSpearLauncher, 2.05F, 3.25F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSpearSweep, 3.10F, 4.30F));
    s.expectations.push_back(
        commander_action_expectation(QStringLiteral("rpg_commander"),
                                     Action::CommanderSpearAirThrust,
                                     4.25F,
                                     5.20F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::CommanderSpearDive, 4.85F, 6.10F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSpearThrust, 6.25F, 8.95F));
    s.expectations.push_back(
        commander_action_expectation(QStringLiteral("rpg_commander"),
                                     Action::CommanderSpearStepThrust,
                                     6.25F,
                                     8.95F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSpearSweep, 6.25F, 8.95F));
    s.expectations.push_back(commander_action_expectation(
        QStringLiteral("rpg_commander"), Action::RpgSpearFinisher, 6.25F, 8.95F));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_commander_bow_grammar_id),
        QStringLiteral("RPG Commander Bow Grammar"),
        QStringLiteral(
            "A bow commander alternates power shots and evasive specials, then "
            "switches into and out of his melee stance while the direct-control "
            "camera tracks the authored movement."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanFieldCommander,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto targets = group(
        QStringLiteral("enemy_line"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 10.0F}, 5);
    targets.facing_degrees = 180.0F;
    targets.health_override = targets.max_health_override = 6000;
    s.groups = {commander, targets};
    s.steps = {
        at(0.10F,
           Command::RpgAim,
           QStringLiteral("rpg_commander"),
           QStringLiteral("enemy_line")),
        at(0.55F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(2.05F, Command::RpgSpecial, QStringLiteral("rpg_commander")),
        at(3.20F, Command::RpgWeaponSwitch, QStringLiteral("rpg_commander")),
        at(4.35F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(5.60F, Command::RpgWeaponSwitch, QStringLiteral("rpg_commander")),
        at(6.75F, Command::RpgHeavyAttack, QStringLiteral("rpg_commander")),
        at(8.10F, Command::RpgSpecial, QStringLiteral("rpg_commander")),
    };
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
