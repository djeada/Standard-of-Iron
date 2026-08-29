#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

#include "animation/melee_swing_manifest.h"
#include "game/core/component.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/melee_intent_solver.h"

namespace {

using Engine::Core::AttackDirection;
using Engine::Core::classify_attack_direction;
using Engine::Core::melee_intent_from_attack_direction;
using Engine::Core::MeleeIntent;
using Game::Systems::CombatActions::MeleeIntentInputs;
using Game::Systems::CombatActions::resolve_melee_intent;
using Game::Systems::CombatActions::select_melee_action;
using Game::Systems::CombatActions::steer_melee_intent;

constexpr float k_full_sweep = 1.0F;

TEST(MeleeIntentTest, ClassificationRoundTripsTheCanonicalSwings) {
  for (auto direction : {AttackDirection::LeftSlash,
                         AttackDirection::RightSlash,
                         AttackDirection::Overhead,
                         AttackDirection::Thrust}) {
    EXPECT_EQ(classify_attack_direction(melee_intent_from_attack_direction(direction)),
              direction);
  }

  auto const heavy = melee_intent_from_attack_direction(AttackDirection::HeavyOverhead);
  EXPECT_EQ(classify_attack_direction(heavy), AttackDirection::HeavyOverhead);
}

TEST(MeleeIntentTest, TheSwingFollowsTheLineThePlayerDraws) {
  auto const drag = [](float x, float y) {
    return resolve_melee_intent({.aim_delta_x = x, .aim_delta_y = y});
  };

  EXPECT_EQ(classify_attack_direction(drag(-0.7F * k_full_sweep, -0.7F * k_full_sweep)),
            AttackDirection::LeftSlash);
  EXPECT_EQ(classify_attack_direction(drag(0.7F * k_full_sweep, -0.7F * k_full_sweep)),
            AttackDirection::RightSlash);
  EXPECT_EQ(classify_attack_direction(drag(0.0F, -k_full_sweep)),
            AttackDirection::Overhead);
}

TEST(MeleeIntentTest, SwingsAreContinuousNotSnappedToFiveDirections) {

  auto const shallow =
      resolve_melee_intent({.aim_delta_x = -0.90F, .aim_delta_y = -0.20F});
  auto const steeper =
      resolve_melee_intent({.aim_delta_x = -0.80F, .aim_delta_y = -0.45F});

  EXPECT_EQ(classify_attack_direction(shallow), classify_attack_direction(steeper));
  EXPECT_GT(Engine::Core::melee_intent_strike_delta(shallow, steeper), 0.05F);
  EXPECT_NE(shallow.weapon_target_y, steeper.weapon_target_y);
}

TEST(MeleeIntentTest, AFlickAsksForAFasterSwingThanADrag) {
  auto const drag =
      resolve_melee_intent({.aim_delta_x = -k_full_sweep, .aim_rate = 0.4F});
  auto const flick =
      resolve_melee_intent({.aim_delta_x = -k_full_sweep, .aim_rate = 2.0F});
  EXPECT_GT(flick.swing_speed, drag.swing_speed);
}

TEST(MeleeIntentTest, HoldingTheStrikeChargesItAndCarriesItFurther) {
  auto const quick = resolve_melee_intent({.held_duration = 0.0F});
  auto const held = resolve_melee_intent({.held_duration = 0.6F});
  EXPECT_LT(quick.charge, held.charge);
  EXPECT_LT(quick.follow_through, held.follow_through);
}

TEST(MeleeIntentTest, AForwardStepThrustsAndASweepOverridesIt) {
  auto const stepping = resolve_melee_intent({.move_forward_axis = 1});
  EXPECT_EQ(classify_attack_direction(stepping), AttackDirection::Thrust);

  auto const cutting = resolve_melee_intent(
      {.aim_delta_x = -k_full_sweep, .aim_delta_y = -0.4F, .move_forward_axis = 1});
  EXPECT_NE(classify_attack_direction(cutting), AttackDirection::Thrust);
}

TEST(MeleeIntentTest, TheNextSwingGrowsOutOfWhereTheLastOneFinished) {
  auto const first = resolve_melee_intent({});
  auto const rest = Engine::Core::melee_intent_resting_direction(first);

  auto const second = resolve_melee_intent(
      {.has_rest = true, .rest_dir_x = rest.x, .rest_dir_y = rest.y});

  EXPECT_LT(first.strike_dir_y, 0.0F);
  EXPECT_GT(second.strike_dir_y, 0.0F);
  EXPECT_GT(Engine::Core::melee_intent_strike_delta(first, second), 1.0F);
}

TEST(MeleeIntentTest, SteeringIsRateLimitedSoAFlickBendsTheBladeNotTeleportsIt) {
  auto const current = melee_intent_from_attack_direction(AttackDirection::LeftSlash);
  auto const desired = melee_intent_from_attack_direction(AttackDirection::RightSlash);

  constexpr float k_small_step = 0.10F;
  auto const stepped = steer_melee_intent(current, desired, 1.0F, k_small_step);
  float const moved = Engine::Core::melee_intent_strike_delta(current, stepped);
  EXPECT_GT(moved, 0.0F);
  EXPECT_LE(moved, k_small_step + 1.0e-3F);

  auto const locked = steer_melee_intent(current, desired, 0.0F, 1.0F);
  EXPECT_NEAR(Engine::Core::melee_intent_strike_delta(current, locked), 0.0F, 1.0e-4F);
}

TEST(MeleeIntentTest, ActionSelectionFollowsTheSwingRatherThanACounter) {
  using Game::Systems::CombatActions::CombatActionId;
  auto const left = resolve_melee_intent({.aim_delta_x = -0.7F, .aim_delta_y = -0.7F});
  auto const right = resolve_melee_intent({.aim_delta_x = 0.7F, .aim_delta_y = -0.7F});

  EXPECT_EQ(
      select_melee_action(left, Engine::Core::CombatAttackFamily::Sword, false, false),
      CombatActionId::RpgSwordSlashLeft);
  EXPECT_EQ(
      select_melee_action(right, Engine::Core::CombatAttackFamily::Sword, false, false),
      CombatActionId::RpgSwordSlashRight);

  EXPECT_EQ(
      select_melee_action(left, Engine::Core::CombatAttackFamily::Sword, false, false),
      select_melee_action(left, Engine::Core::CombatAttackFamily::Sword, false, false));
}

TEST(MeleeIntentTest, CommanderSwordGrammarBranchesFromTheSameSemanticInputs) {
  using Engine::Core::CommanderCombatIntentType;
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::resolve_commander_action;
  using Game::Systems::CombatActions::WeaponFamily;

  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::Light,
                                     WeaponFamily::Sword,
                                     false,
                                     false),
            CombatActionId::RpgSwordSlashLeft);
  EXPECT_EQ(resolve_commander_action(CombatActionId::RpgSwordSlashLeft,
                                     CommanderCombatIntentType::Light,
                                     WeaponFamily::Sword,
                                     false,
                                     false),
            CombatActionId::RpgSwordSlashRight);
  EXPECT_EQ(resolve_commander_action(CombatActionId::RpgSwordSlashLeft,
                                     CommanderCombatIntentType::Heavy,
                                     WeaponFamily::Sword,
                                     false,
                                     false),
            CombatActionId::CommanderSwordLauncher);
  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::Heavy,
                                     WeaponFamily::Sword,
                                     false,
                                     true),
            CombatActionId::CommanderSwordGapCloser);
  EXPECT_EQ(resolve_commander_action(CombatActionId::CommanderSwordAirLight,
                                     CommanderCombatIntentType::Heavy,
                                     WeaponFamily::Sword,
                                     true,
                                     false),
            CombatActionId::CommanderSwordDive);
}

TEST(MeleeIntentTest, CommanderSpearAndBowGrammarsKeepTheirWeaponIdentity) {
  using Engine::Core::CommanderCombatIntentType;
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::resolve_commander_action;
  using Game::Systems::CombatActions::WeaponFamily;

  EXPECT_EQ(resolve_commander_action(CombatActionId::RpgSpearThrust,
                                     CommanderCombatIntentType::Light,
                                     WeaponFamily::Spear,
                                     false,
                                     false),
            CombatActionId::CommanderSpearStepThrust);
  EXPECT_EQ(resolve_commander_action(CombatActionId::CommanderSpearStepThrust,
                                     CommanderCombatIntentType::Heavy,
                                     WeaponFamily::Spear,
                                     false,
                                     false),
            CombatActionId::CommanderSpearLauncher);
  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::Heavy,
                                     WeaponFamily::Bow,
                                     false,
                                     false),
            CombatActionId::CommanderBowPowerShot);
  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::Special,
                                     WeaponFamily::Bow,
                                     false,
                                     false),
            CombatActionId::CommanderBowEvasiveShot);
  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::WeaponSwitch,
                                     WeaponFamily::Sword,
                                     false,
                                     false),
            CombatActionId::CommanderSwordSpin);
  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::WeaponSwitch,
                                     WeaponFamily::Spear,
                                     false,
                                     false),
            CombatActionId::RpgSpearSweep);
  EXPECT_EQ(resolve_commander_action(CombatActionId::None,
                                     CommanderCombatIntentType::WeaponSwitch,
                                     WeaponFamily::Bow,
                                     false,
                                     false),
            CombatActionId::CommanderBowEvasiveShot);
}

TEST(MeleeIntentTest, EveryAdvancedActionIsExplicitlyCommanderOnly) {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::find_combat_action_definition;

  for (auto id : {CombatActionId::CommanderSwordSpin,
                  CombatActionId::CommanderSwordLauncher,
                  CombatActionId::CommanderSwordGapCloser,
                  CombatActionId::CommanderSwordAirLight,
                  CombatActionId::CommanderSwordAirReverse,
                  CombatActionId::CommanderSwordDive,
                  CombatActionId::CommanderSpearStepThrust,
                  CombatActionId::CommanderSpearLauncher,
                  CombatActionId::CommanderSpearGapCloser,
                  CombatActionId::CommanderSpearAirThrust,
                  CombatActionId::CommanderSpearDive,
                  CombatActionId::CommanderBowPowerShot,
                  CombatActionId::CommanderBowEvasiveShot}) {
    auto const* definition = find_combat_action_definition(id);
    ASSERT_NE(definition, nullptr);
    EXPECT_TRUE(definition->commander_only);
  }

  auto const* ordinary = find_combat_action_definition(CombatActionId::RtsSwordStrike);
  ASSERT_NE(ordinary, nullptr);
  EXPECT_FALSE(ordinary->commander_only);
}

TEST(MeleeIntentTest, GroundComboLinksAdvanceAndHeaviesCommitLonger) {
  using Game::Systems::CombatActions::CombatActionId;
  using Game::Systems::CombatActions::find_combat_action_definition;

  for (auto id : {CombatActionId::RpgSwordSlashLeft,
                  CombatActionId::RpgSwordSlashRight,
                  CombatActionId::CommanderSwordSpin,
                  CombatActionId::RpgSwordFinisher,
                  CombatActionId::RpgSpearThrust,
                  CombatActionId::CommanderSpearStepThrust,
                  CombatActionId::RpgSpearSweep,
                  CombatActionId::RpgSpearFinisher}) {
    auto const* link = find_combat_action_definition(id);
    ASSERT_NE(link, nullptr);
    EXPECT_GT(link->movement.distance, 0.0F) << static_cast<int>(id);
  }

  auto const* sword_light =
      find_combat_action_definition(CombatActionId::RpgSwordSlashLeft);
  auto const* sword_heavy =
      find_combat_action_definition(CombatActionId::RpgSwordOverhead);
  auto const* spear_light =
      find_combat_action_definition(CombatActionId::RpgSpearThrust);
  auto const* spear_heavy =
      find_combat_action_definition(CombatActionId::RpgSpearFinisher);
  ASSERT_NE(sword_light, nullptr);
  ASSERT_NE(sword_heavy, nullptr);
  ASSERT_NE(spear_light, nullptr);
  ASSERT_NE(spear_heavy, nullptr);
  EXPECT_GT(sword_heavy->duration_seconds, sword_light->duration_seconds);
  EXPECT_GT(sword_heavy->damage.base_multiplier, sword_light->damage.base_multiplier);
  EXPECT_GT(spear_heavy->duration_seconds, spear_light->duration_seconds);
  EXPECT_GT(spear_heavy->damage.base_multiplier, spear_light->damage.base_multiplier);
}

TEST(MeleeIntentTest, OneSwingCarriedOntoDifferentAnchorsKeepsItsDeviation) {

  auto const shared =
      resolve_melee_intent({.aim_delta_x = -0.55F, .aim_delta_y = -0.83F});
  auto const own = Animation::melee_intent_for_attack_variant(
      Animation::nearest_attack_variant(shared));
  float const deviation = Engine::Core::melee_intent_strike_delta(own, shared);
  EXPECT_GT(deviation, 0.02F);

  for (std::uint8_t variant = 0U; variant < 3U; ++variant) {
    auto const anchor = Animation::melee_intent_for_attack_variant(variant);
    auto const carried = Animation::melee_intent_about_anchor(shared, anchor);
    EXPECT_NEAR(
        Engine::Core::melee_intent_strike_delta(anchor, carried), deviation, 1.0e-3F);
    EXPECT_FLOAT_EQ(carried.charge, shared.charge);
    EXPECT_FLOAT_EQ(carried.swing_speed, shared.swing_speed);
  }

  auto const left = Animation::melee_intent_about_anchor(
      shared, Animation::melee_intent_for_attack_variant(0U));
  auto const right = Animation::melee_intent_about_anchor(
      shared, Animation::melee_intent_for_attack_variant(1U));
  EXPECT_GT(Engine::Core::melee_intent_strike_delta(left, right), 1.0F);
}

TEST(MeleeSwingTest, TheArcPassesThroughTheWeaponObjectiveAndKeepsMoving) {
  Animation::MeleeSwingInputs inputs{};
  inputs.intent = melee_intent_from_attack_direction(AttackDirection::LeftSlash);
  inputs.phase = Animation::k_melee_contact_time;

  auto const contact = Animation::resolve_melee_swing(inputs);
  EXPECT_NEAR(contact.grip.x, inputs.intent.weapon_target_x, 1.0e-3F);
  EXPECT_NEAR(contact.grip.z, inputs.intent.weapon_target_z, 1.0e-3F);
  EXPECT_NEAR(
      contact.grip.y, inputs.shoulder_y + inputs.intent.weapon_target_y, 1.0e-3F);

  EXPECT_GT(contact.speed, 0.0F);
  EXPECT_NEAR(contact.commitment, 1.0F, 1.0e-3F);
}

TEST(MeleeSwingTest, TheChamberIsBehindTheFrontalPlaneAndTheStrikeIsInFrontOfIt) {
  Animation::MeleeSwingInputs inputs{};
  inputs.intent = melee_intent_from_attack_direction(AttackDirection::LeftSlash);

  inputs.phase = Animation::k_melee_chamber_time;
  auto const chamber = Animation::resolve_melee_swing(inputs);
  inputs.phase = Animation::k_melee_contact_time;
  auto const contact = Animation::resolve_melee_swing(inputs);

  EXPECT_LT(chamber.grip.z, 0.0F);
  EXPECT_GT(contact.grip.z, chamber.grip.z);

  EXPECT_GT(chamber.grip.x, contact.grip.x);
}

TEST(MeleeSwingTest, AFasterSwingCoversTheSameArcMoreQuickly) {
  Animation::MeleeSwingInputs inputs{};
  inputs.intent = melee_intent_from_attack_direction(AttackDirection::LeftSlash);
  inputs.phase = 0.5F;

  auto const nominal = Animation::resolve_melee_swing(inputs);
  inputs.intent.swing_speed = 2.0F;
  auto const quick = Animation::resolve_melee_swing(inputs);

  EXPECT_NEAR(nominal.grip.x, quick.grip.x, 1.0e-4F);
  EXPECT_GT(quick.speed, nominal.speed * 1.5F);
}

TEST(MeleeBodySolveTest, TheTorsoWindsUpAgainstTheChamberAndUnwindsThroughContact) {
  Animation::MeleeBodySolveInputs inputs{};
  inputs.swing.intent = melee_intent_from_attack_direction(AttackDirection::LeftSlash);

  inputs.swing.phase = Animation::k_melee_chamber_time;
  auto const loaded = Animation::resolve_melee_body_solve(inputs);
  inputs.swing.phase = Animation::k_melee_contact_time;
  auto const landed = Animation::resolve_melee_body_solve(inputs);

  EXPECT_LT(loaded.spine_twist, 0.0F);
  EXPECT_GT(landed.spine_twist, 0.0F);

  EXPECT_LT(loaded.pelvis_twist * loaded.spine_twist, 0.0F);
  EXPECT_GT(landed.pelvis_twist * landed.spine_twist, 0.0F);
}

TEST(MeleeBodySolveTest, WeightTravelsFromTheBackFootToTheFrontOne) {
  Animation::MeleeBodySolveInputs inputs{};
  inputs.swing.intent = melee_intent_from_attack_direction(AttackDirection::Overhead);

  inputs.swing.phase = Animation::k_melee_chamber_time;
  auto const loading = Animation::resolve_melee_body_solve(inputs);
  inputs.swing.phase = Animation::k_melee_contact_time;
  auto const landing = Animation::resolve_melee_body_solve(inputs);

  EXPECT_LT(loading.weight_shift, 0.0F);
  EXPECT_GT(landing.weight_shift, 0.0F);
  EXPECT_GT(loading.back_foot_brace, 0.0F);
  EXPECT_GT(landing.front_foot_advance, 0.0F);
  EXPECT_LT(loading.forward_lean, landing.forward_lean);
}

TEST(MeleeBodySolveTest, ATwoHandedGripPinsTheOffHandToTheWeapon) {
  Animation::MeleeBodySolveInputs inputs{};
  inputs.swing.intent = melee_intent_from_attack_direction(AttackDirection::Thrust);
  inputs.swing.phase = Animation::k_melee_contact_time;

  inputs.offhand_along_weapon = 0.20F;
  auto const paired = Animation::resolve_melee_body_solve(inputs);
  inputs.offhand_along_weapon = 0.0F;
  auto const free_hand = Animation::resolve_melee_body_solve(inputs);

  auto const gap = [](const Animation::PoseVec3& a, const Animation::PoseVec3& b) {
    return std::sqrt(((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)) +
                     ((a.z - b.z) * (a.z - b.z)));
  };
  EXPECT_NEAR(gap(paired.offhand, paired.grip), 0.20F, 1.0e-3F);
  EXPECT_GT(gap(free_hand.offhand, free_hand.grip), 0.4F);
}

TEST(MeleeBodySolveTest, OnlyAnObjectiveBeyondTheArmDrivesTheShoulderAfterIt) {
  Animation::MeleeBodySolveInputs inputs{};
  inputs.swing.arm_reach = 0.60F;
  inputs.swing.phase = Animation::k_melee_contact_time;

  inputs.swing.intent = Engine::Core::melee_intent_from_strike_angle(
      Animation::k_melee_thrust_angle, 1.0F, 0.40F);
  auto const within_reach = Animation::resolve_melee_body_solve(inputs);
  EXPECT_NEAR(within_reach.shoulder_drive, 0.0F, 1.0e-4F);

  inputs.swing.intent = Engine::Core::melee_intent_from_strike_angle(
      Animation::k_melee_thrust_angle, 1.0F, 2.40F);
  auto const beyond_reach = Animation::resolve_melee_body_solve(inputs);
  EXPECT_GT(beyond_reach.shoulder_drive, within_reach.shoulder_drive);
}

} // namespace
