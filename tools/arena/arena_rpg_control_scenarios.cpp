#include "arena_rpg_control_scenarios.h"

#include <initializer_list>
#include <optional>
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

auto sidestep_along_the_line(float time) -> ArenaScenarioStep {
  auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
  step.destination = {1.0F, 0.0F, 0.0F};
  step.value = 0;
  step.rpg_view_yaw_degrees = 180.0F;
  return step;
}

void add_animation_quality(ArenaScenarioDefinition& scenario,
                           std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    scenario.expectations.push_back(expectation(Expect::NoPlantedFootSliding, name));
    scenario.expectations.push_back(expectation(Expect::NoWeaponTeleport, name));
    scenario.expectations.push_back(expectation(Expect::NoPelvisSnap, name));
  }
}

} // namespace

auto build_rpg_control_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_commander_aura_pulse_id),
        QStringLiteral("Commander Aura Pulse"),
        QStringLiteral(
            "Explicitly activates a short command aura, verifies nearby "
            "troops receive its visible timed bonus, verifies distant troops "
            "do not, and waits for cooldown."),
        5.0F,
        {26.0F, 55.0F, 25.0F});
    auto commander = group(QStringLiteral("commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.nation_id = Nation::RomanRepublic;
    auto near_troops = group(QStringLiteral("near_troops"),
                             Troop::Swordsman,
                             1,
                             2,
                             {5.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 3.0F});
    auto distant_troops = group(QStringLiteral("distant_troops"),
                                Troop::Swordsman,
                                1,
                                1,
                                {21.0F, 0.0F, 0.0F},
                                4);
    s.groups = {commander, near_troops, distant_troops};
    auto activate =
        at(0.5F, Command::TriggerCommanderAura, QStringLiteral("commander"));
    activate.value = 2;
    s.steps = {activate};
    s.expectations = {
        expectation(Expect::CommanderAuraActivated, QStringLiteral("commander")),
        expectation(Expect::CommanderAuraBuffObserved, QStringLiteral("near_troops")),
        expectation(Expect::NoCommanderAuraBuffObserved,
                    QStringLiteral("distant_troops")),
        expectation(Expect::CommanderAuraExpired, QStringLiteral("commander")),
        expectation(Expect::GroupIsRendered, QStringLiteral("commander")),
        expectation(Expect::GroupIsRendered, QStringLiteral("near_troops")),
        expectation(Expect::GroupIsRendered, QStringLiteral("distant_troops")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_melee_contact_id),
        QStringLiteral("RPG Exact Melee Contact"),
        QStringLiteral(
            "Behind-head commander combat against a six-soldier formation. "
            "Validates exact in-range soldier highlighting, authored blade "
            "contact, exact hit reaction, and visible incoming weapon damage."),
        5.4F);
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

    enemy.health_override = enemy.max_health_override = 4000;
    s.groups = {commander, enemy};
    s.steps = {
        at(0.15F,
           Command::Attack,
           QStringLiteral("enemy_formation"),
           QStringLiteral("rpg_commander")),
        at(0.55F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(3.05F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_formation")});
    add_animation_quality(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_formation")});
    s.expectations.push_back(
        expectation(Expect::AttackHasTorsoRotation, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::ExactRpgTargetObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::HitReactionObserved, QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::RpgDamageContactObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgStrikeAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgHealthReduced, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_defense_contact_id),
        QStringLiteral("RPG Block and Dodge Contact"),
        QStringLiteral(
            "Behind-head defensive sequence with a frontal sword block followed "
            "by a timed dodge against a newly joining attacker. Health must stay "
            "unchanged while the block contact and dodge window remain visible."),
        3.3F);
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
    auto guard_attacker = group(QStringLiteral("guard_attacker"),
                                Troop::Swordsman,
                                2,
                                1,
                                {0.0F, 0.0F, 1.45F},
                                1);
    auto dodge_attacker = group(QStringLiteral("dodge_attacker"),
                                Troop::Swordsman,
                                2,
                                1,
                                {0.45F, 0.0F, 1.45F},
                                1);
    dodge_attacker.spawn_at_start = false;
    s.groups = {commander, guard_attacker, dodge_attacker};
    auto enable_guard = at(0.05F, Command::RpgGuard, QStringLiteral("rpg_commander"));
    enable_guard.enabled = true;
    auto remove_guard_attacker =
        at(1.55F, Command::SetHealth, QStringLiteral("guard_attacker"));
    remove_guard_attacker.value = 0;
    auto disable_guard = at(1.62F, Command::RpgGuard, QStringLiteral("rpg_commander"));
    disable_guard.enabled = false;
    s.steps = {
        enable_guard,
        at(0.15F,
           Command::Attack,
           QStringLiteral("guard_attacker"),
           QStringLiteral("rpg_commander")),
        remove_guard_attacker,
        disable_guard,
        at(1.72F,
           Command::SpawnAmbush,
           QStringLiteral("dodge_attacker"),
           QStringLiteral("rpg_commander")),
        [] {
          auto dodge = at(2.12F, Command::RpgDodge, QStringLiteral("rpg_commander"));
          dodge.destination = {0.0F, 0.0F, -1.0F};
          return dodge;
        }(),
    };
    add_visual_stability(s,
                         {QStringLiteral("rpg_commander"),
                          QStringLiteral("guard_attacker"),
                          QStringLiteral("dodge_attacker")});
    s.expectations.push_back(
        expectation(Expect::NoWeaponTeleport, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::NoPelvisSnap, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::ExactRpgTargetObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgBlockContactObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgDodgeWindowObserved, QStringLiteral("rpg_commander")));
    {

      auto health_unchanged =
          expectation(Expect::RpgHealthUnchanged, QStringLiteral("rpg_commander"));
      health_unchanged.end_seconds = 2.60F;
      s.expectations.push_back(health_unchanged);
    }
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_projectile_block_id),
        QStringLiteral("RPG Projectile Block"),
        QStringLiteral(
            "Behind-head commander faces an authored enemy arrow flight. The "
            "projectile must visibly arrive at the guard, publish a block contact, "
            "and leave RPG health unchanged."),
        4.2F);
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
    auto archer = group(
        QStringLiteral("enemy_archer"), Troop::Archer, 2, 1, {0.0F, 0.0F, 6.0F}, 1);
    s.groups = {commander, archer};
    auto enable_guard = at(0.05F, Command::RpgGuard, QStringLiteral("rpg_commander"));
    enable_guard.enabled = true;
    s.steps = {
        enable_guard,
        at(0.15F,
           Command::Attack,
           QStringLiteral("enemy_archer"),
           QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_archer")});
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("enemy_archer"),
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("enemy_archer"),
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgBlockContactObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgHealthUnchanged, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_escort_crowd_id),
        QStringLiteral("RPG Escort Crowd"),
        QStringLiteral(
            "Behind-head commander standing inside his own escort, with a rank of "
            "friendly spearmen between him and the lens. The chase camera must stay "
            "readable: bodies that crowd the gap in front of the lens are dropped "
            "rather than filling the frame or shoving the camera into first person."),
        3.6F);
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

    auto escort_rear = group(
        QStringLiteral("escort_rear"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -1.7F}, 4);
    escort_rear.facing_degrees = 0.0F;

    auto escort_flank = group(
        QStringLiteral("escort_flank"), Troop::Swordsman, 1, 1, {2.4F, 0.0F, 0.2F}, 2);
    escort_flank.facing_degrees = 0.0F;
    auto enemy = group(
        QStringLiteral("enemy_line"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 3.2F}, 3);
    enemy.health_override = enemy.max_health_override = 500;
    s.groups = {commander, escort_rear, escort_flank, enemy};
    s.steps = {
        at(0.20F,
           Command::Attack,
           QStringLiteral("enemy_line"),
           QStringLiteral("rpg_commander")),
        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };

    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_line")});
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("escort_flank")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_locomotion_id),
        QStringLiteral("RPG Locomotion"),
        QStringLiteral(
            "Behind-head commander walks forward, breaks into a run, backs up, "
            "strafes, then halts on open ground without ever turning. The rendered "
            "locomotion has to follow the simulated one frame for frame, with no "
            "root teleporting and no idle pose while the body is still travelling."),
        9.0F);
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
                           {0.0F, 0.0F, -6.0F},
                           1);
    commander.facing_degrees = 0.0F;
    s.groups = {commander};

    auto move = [](float time, QVector3D axes, bool run) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = run ? 1 : 0;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, false),
        move(2.20F, {0.0F, 0.0F, 1.0F}, true),

        move(4.40F, {0.0F, 0.0F, -1.0F}, false),
        move(6.20F, {1.0F, 0.0F, 0.0F}, false),
        move(8.00F, {0.0F, 0.0F, 0.0F}, false),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgRunObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));

    auto hitched = s;
    hitched.id = QString::fromLatin1(k_rpg_locomotion_hitch_id);
    hitched.label = QStringLiteral("RPG Locomotion Under Hitches");
    hitched.description = QStringLiteral(
        "The locomotion script again, with four deliberately long presented "
        "frames injected while the commander is mid-stride. A planted foot that "
        "keeps travelling while the body reports as idle only appears when a "
        "frame runs long, so the hitches are authored rather than waited for: "
        "the defect reproduces on every run instead of once in five on a busy "
        "machine.");
    hitched.presentation_hitches = {
        {1.20F, 33.0F}, {3.00F, 50.0F}, {5.00F, 100.0F}, {6.80F, 100.0F}};
    result.push_back(std::move(s));
    result.push_back(std::move(hitched));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_motor_start_stop_id),
        QStringLiteral("RPG Motor Start And Stop"),
        QStringLiteral(
            "Walk, run, halt, and reverse on open ground. The motor contract is "
            "that a held direction settles at the configured speed, a release "
            "decelerates rather than snapping, and a reversal does not teleport "
            "the body through the turn."),
        9.0F);
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
                           {0.0F, 0.0F, -8.0F},
                           1);
    commander.facing_degrees = 0.0F;
    s.groups = {commander};

    auto move = [](float time, QVector3D axes, bool run) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = run ? 1 : 0;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, false),
        move(1.80F, {0.0F, 0.0F, 0.0F}, false),
        move(3.00F, {0.0F, 0.0F, 1.0F}, true),
        move(5.00F, {0.0F, 0.0F, -1.0F}, true),
        move(7.00F, {0.0F, 0.0F, 0.0F}, false),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgRunObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_motor_diagonal_id),
        QStringLiteral("RPG Motor Diagonal"),
        QStringLiteral(
            "All eight input directions in turn, each held long enough to settle. "
            "A diagonal is two axes at once and must not travel faster than a "
            "single axis; the same script also walks every backpedal and strafe "
            "scale past the locomotion contract."),
        9.0F);
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
    s.groups = {commander};

    auto move = [](float time, QVector3D axes) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}),
        move(1.30F, {1.0F, 0.0F, 1.0F}),
        move(2.30F, {1.0F, 0.0F, 0.0F}),
        move(3.30F, {1.0F, 0.0F, -1.0F}),
        move(4.30F, {0.0F, 0.0F, -1.0F}),
        move(5.30F, {-1.0F, 0.0F, -1.0F}),
        move(6.30F, {-1.0F, 0.0F, 0.0F}),
        move(7.30F, {-1.0F, 0.0F, 1.0F}),
        move(8.30F, {0.0F, 0.0F, 0.0F}),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_motor_figure_eight_id),
        QStringLiteral("RPG Motor Figure Eight"),
        QStringLiteral(
            "Continuous camera-relative direction changes: the view yaw sweeps "
            "while a forward input is held, so the movement basis rotates under "
            "the motor every tick. The body must trace a smooth path rather than "
            "stutter as the basis turns."),
        9.0F);
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
    s.groups = {commander};

    auto move = [](float time, QVector3D axes, float yaw) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      step.rpg_view_yaw_degrees = yaw;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, 0.0F),
        move(1.10F, {0.0F, 0.0F, 1.0F}, 45.0F),
        move(1.90F, {0.0F, 0.0F, 1.0F}, 90.0F),
        move(2.70F, {0.0F, 0.0F, 1.0F}, 135.0F),
        move(3.50F, {0.0F, 0.0F, 1.0F}, 180.0F),
        move(4.30F, {0.0F, 0.0F, 1.0F}, 225.0F),
        move(5.10F, {0.0F, 0.0F, 1.0F}, 270.0F),
        move(5.90F, {0.0F, 0.0F, 1.0F}, 315.0F),
        move(6.70F, {0.0F, 0.0F, 1.0F}, 0.0F),
        move(8.00F, {0.0F, 0.0F, 0.0F}, 0.0F),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_close_quarters_id),
        QStringLiteral("RPG Close Quarters"),
        QStringLiteral(
            "Behind-head commander walks straight into a house wall, backs off, and "
            "closes on it again from the flank. The RTS navigation grid keeps whole "
            "formations a pad away from every structure; a person-sized commander "
            "has to reach the facade itself, so this is the close-quarters "
            "clearance contract."),
        9.0F);
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
                           {0.0F, 0.0F, -5.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto home = building(QStringLiteral("close_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {0.0F, 0.0F, 0.0F});
    home.health_override = home.max_health_override = 4000;
    s.groups = {commander, home};

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
        move(2.40F, {0.0F, 0.0F, 0.0F}, false),

        move(2.70F, {0.0F, 0.0F, 1.0F}, false, 180.0F),
        move(4.20F, {0.0F, 0.0F, 0.0F}, false),

        move(4.50F, {0.0F, 0.0F, 1.0F}, false, 90.0F),
        move(6.30F, {0.0F, 0.0F, 0.0F}, false),

        move(6.60F, {0.0F, 0.0F, 1.0F}, false, 0.0F),
        move(8.60F, {0.0F, 0.0F, 0.0F}, false),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    s.expectations.push_back(expectation(Expect::RpgApproachWithin,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("close_home"),
                                         0.0F,
                                         0.0F,
                                         1.90F));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_obstacle_slide_id),
        QStringLiteral("RPG Obstacle Slide"),
        QStringLiteral(
            "Behind-head commander walks diagonally into a house facade and keeps "
            "going. Direct control has to behave like a body against a wall: the "
            "blocked axis is dropped and the commander slides along the facade "
            "instead of stopping dead in front of it with the stick still pushed."),
        9.0F);
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
                           {-2.0F, 0.0F, -6.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto home = building(QStringLiteral("slide_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {0.0F, 0.0F, 0.0F});
    home.health_override = home.max_health_override = 4000;
    s.groups = {commander, home};

    auto move = [](float time, QVector3D axes, std::optional<float> yaw = {}) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      step.rpg_view_yaw_degrees = yaw;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, 30.0F),
        move(8.20F, {0.0F, 0.0F, 0.0F}),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    s.expectations.push_back(expectation(Expect::RpgApproachWithin,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("slide_home"),
                                         0.0F,
                                         0.0F,
                                         1.90F));
    auto slide = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 1.50F, 2.30F);
    slide.end_seconds = 4.00F;
    s.expectations.push_back(slide);
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_camera_prop_gauntlet_id),
        QStringLiteral("RPG Camera Prop Gauntlet"),
        QStringLiteral(
            "Behind-head commander sidesteps past a line of props with the view "
            "held across his path, so the boom trails straight through each of them "
            "in turn: ruins, a pine, a boulder, a fallen dead tree and a tent. The "
            "lens "
            "has to shorten toward him at the ruins, the pine and the tent, and it "
            "has to ignore the boulder and the log it is already two metres above -- "
            "collision the commander's own body would report is not the same as "
            "collision a camera at head-and-a-half height can see."),
        14.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.arena_floor_half_extent = 20.0F;
    s.ground_type = QStringLiteral("soil_rocky");
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {-13.0F, 0.0F, -3.8F},
                           1);
    commander.facing_degrees = 180.0F;
    s.groups = {commander};

    s.resource_patches = {
        patch("ruins", 1, {-9.0F, 0.0F, 0.8F}),
        patch("pine_tree", 1, {-4.2F, 0.0F, -0.3F}),
        patch("boulder", 1, {-1.0F, 0.0F, -0.3F}),
        patch("dead_tree", 1, {3.2F, 0.0F, -0.3F}),
        patch("tent", 1, {10.0F, 0.0F, 1.2F}, {2.5F, 0.0F, 0.0F}, 1.15F),
    };

    s.steps = {
        sidestep_along_the_line(0.30F),
        stop_moving(12.60F),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_commander_control_metrics(s, QStringLiteral("rpg_commander"));

    s.expectations.push_back(expectation(Expect::CommanderCameraKeepsCommanderInSight,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         0.35F));
    auto crossing = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 18.0F, 0.40F);
    crossing.end_seconds = 12.50F;
    s.expectations.push_back(std::move(crossing));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_camera_wall_pocket_id),
        QStringLiteral("RPG Camera Wall Pocket"),
        QStringLiteral(
            "The same sidestep with the view held across the path, this time along "
            "a wall run that ends at a house. The boom trails into masonry "
            "for the whole crossing. Shortening is the only move allowed: pushing the "
            "lens sideways out of a wall puts it on the far side with the commander "
            "hidden behind the thing it just left."),
        14.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.arena_floor_half_extent = 20.0F;
    s.ground_type = QStringLiteral("soil_rocky");
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {-13.0F, 0.0F, -3.4F},
                           1);
    commander.facing_degrees = 180.0F;

    auto wall = building(QStringLiteral("pocket_wall"),
                         Game::Units::SpawnType::WallSegment,
                         Nation::RomanRepublic,
                         1,
                         9,
                         {-4.0F, 0.0F, 0.0F},
                         {2.0F, 0.0F, 0.0F});
    wall.health_override = wall.max_health_override = 6000;

    auto home = building(QStringLiteral("pocket_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {10.0F, 0.0F, 1.0F});
    home.health_override = home.max_health_override = 6000;
    s.groups = {commander, wall, home};

    s.steps = {
        sidestep_along_the_line(0.30F),
        stop_moving(12.60F),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_commander_control_metrics(s, QStringLiteral("rpg_commander"));

    s.expectations.push_back(expectation(Expect::CommanderCameraKeepsCommanderInSight,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         0.35F));
    auto crossing = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 18.0F, 0.40F);
    crossing.end_seconds = 12.50F;
    s.expectations.push_back(std::move(crossing));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_camera_hill_bank_id),
        QStringLiteral("RPG Camera Hill Bank"),
        QStringLiteral(
            "The same sidestep along the foot of a steep bank, so the boom trails "
            "into rising ground for the whole run. The ground is "
            "sampled along the entire boom rather than under the eye alone: the lens "
            "rides up the bank far enough that the whole sight line clears it, which "
            "is what keeps the commander in front of the crest instead of behind "
            "it."),
        11.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.arena_floor_half_extent = 20.0F;
    s.ground_type = QStringLiteral("soil_rocky");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches = {{{0.0F, 0.0F, 3.0F}, 5.0F, 5.0F, 2.0F}};

    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {-8.0F, 0.0F, -2.2F},
                           1);
    commander.facing_degrees = 180.0F;
    s.groups = {commander};

    s.steps = {
        sidestep_along_the_line(0.30F),
        stop_moving(9.60F),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_commander_control_metrics(s, QStringLiteral("rpg_commander"));

    s.expectations.push_back(expectation(Expect::CommanderCameraKeepsCommanderInSight,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         0.35F));
    auto crossing = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 11.0F, 0.40F);
    crossing.end_seconds = 9.50F;
    s.expectations.push_back(std::move(crossing));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
