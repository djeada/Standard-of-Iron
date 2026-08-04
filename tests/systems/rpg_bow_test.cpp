#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/world.h"
#include "systems/combat_actions/combat_action_definition.h"
#include "systems/combat_system/combat_action_processor.h"
#include "systems/owner_registry.h"
#include "systems/projectile_system.h"
#include "systems/rpg_combat_system/rpg_bow_aim.h"
#include "systems/rpg_combat_system/rpg_bow_draw.h"
#include "systems/rpg_combat_system/rpg_bow_shot.h"

using namespace Engine::Core;

namespace {

using Game::Systems::CombatActions::CombatActionId;
using Game::Systems::CombatActions::find_combat_action_definition;

auto bow_definition() -> const Game::Systems::CombatActions::CombatActionDefinition& {
  auto const* definition = find_combat_action_definition(CombatActionId::RpgBowShot);
  EXPECT_NE(definition, nullptr);
  return *definition;
}

auto release_gate() -> float {
  auto const& definition = bow_definition();
  return definition.duration_seconds *
         Game::Systems::CombatActions::action_event_normalized_time(
             definition,
             Game::Systems::CombatActions::CombatActionEventType::ProjectileRelease,
             0.46F);
}

auto draw_for(RpgCommanderAimComponent& aim,
              RpgCommanderActionComponent& action,
              float seconds,
              float step = 1.0F / 60.0F) -> Game::Systems::RpgCombat::BowDrawTick {
  Game::Systems::RpgCombat::BowDrawTick tick;
  for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
    auto const frame = Game::Systems::RpgCombat::update_bow_draw(
        aim, action, bow_definition(), 1.0F, step);
    action.action_elapsed_time += frame.allowed_delta;

    bool const loosed = tick.loosed || frame.loosed;
    bool const relaxed = tick.relaxed || frame.relaxed;
    bool const reached_full_draw = tick.reached_full_draw || frame.reached_full_draw;
    bool const started_straining = tick.started_straining || frame.started_straining;
    tick = frame;
    tick.loosed = loosed;
    tick.relaxed = relaxed;
    tick.reached_full_draw = reached_full_draw;
    tick.started_straining = started_straining;
  }
  return tick;
}

auto make_archer_commander(World& world, float x, float z) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  entity->add_component<CommanderComponent>()->fpv_controlled = true;
  auto* attack = entity->add_component<AttackComponent>();
  attack->can_ranged = true;
  attack->can_melee = true;
  attack->range = 14.0F;
  attack->damage = 40;
  attack->melee_damage = 12;
  return entity;
}

auto make_enemy(World& world, float x, float z) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;
  unit->owner_id = 2;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  return entity;
}

class RpgBowTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().register_owner_with_id(
        1, Game::Systems::OwnerType::Player, "Player");
    Game::Systems::OwnerRegistry::instance().register_owner_with_id(
        2, Game::Systems::OwnerType::AI, "Enemy");
  }

  World world;
};

TEST_F(RpgBowTest, HeldStringStopsTheShotAtFullDraw) {
  RpgCommanderAimComponent aim;
  RpgCommanderActionComponent action;
  aim.draw_held = true;
  action.action_duration = bow_definition().duration_seconds;

  auto const tick = draw_for(aim, action, release_gate() + 0.5F);

  EXPECT_TRUE(tick.at_full_draw);
  EXPECT_FALSE(tick.loosed);
  EXPECT_FLOAT_EQ(tick.allowed_delta, 0.0F);
  EXPECT_FLOAT_EQ(aim.draw_progress, 1.0F);
  EXPECT_EQ(aim.draw_stage, BowDrawStage::FullDraw);
  EXPECT_LT(action.action_elapsed_time, release_gate());
  EXPECT_GT(aim.full_draw_hold, 0.4F);
}

TEST_F(RpgBowTest, ReleasingEarlyLoosesAWeakerArrowThanAFullDraw) {
  RpgCommanderAimComponent snap;
  RpgCommanderActionComponent snap_action;
  snap.draw_held = true;
  snap_action.action_duration = bow_definition().duration_seconds;
  draw_for(snap, snap_action, release_gate() * 0.3F);
  snap.draw_held = false;
  auto const snap_tick = draw_for(snap, snap_action, 0.5F);

  RpgCommanderAimComponent full;
  RpgCommanderActionComponent full_action;
  full.draw_held = true;
  full_action.action_duration = bow_definition().duration_seconds;
  draw_for(full, full_action, release_gate() + 0.2F);
  full.draw_held = false;
  auto const full_tick = draw_for(full, full_action, 0.2F);

  EXPECT_TRUE(snap_tick.loosed);
  EXPECT_TRUE(full_tick.loosed);
  EXPECT_GT(snap.shot_power, 0.0F);
  EXPECT_LT(snap.shot_power, full.shot_power);
  EXPECT_FLOAT_EQ(full.shot_power, 1.0F);
  EXPECT_GE(snap.shot_power, RpgCommanderAimComponent::k_min_shot_power);
}

TEST_F(RpgBowTest, HoldingPastTheLimitRelaxesTheStringInsteadOfShooting) {
  RpgCommanderAimComponent aim;
  RpgCommanderActionComponent action;
  aim.draw_held = true;
  action.action_duration = bow_definition().duration_seconds;

  auto const tick =
      draw_for(aim,
               action,
               release_gate() + RpgCommanderAimComponent::k_max_hold_seconds + 0.5F);

  EXPECT_TRUE(tick.relaxed);
  EXPECT_FALSE(tick.loosed);
  EXPECT_EQ(aim.draw_stage, BowDrawStage::None);
  EXPECT_LT(action.action_elapsed_time, release_gate());
}

TEST_F(RpgBowTest, ALongHoldTiresTheArmAndCostsTheShotPower) {
  RpgCommanderAimComponent steady;
  RpgCommanderActionComponent steady_action;
  steady.draw_held = true;
  steady_action.action_duration = bow_definition().duration_seconds;
  draw_for(steady, steady_action, release_gate() + 0.2F);
  float const steady_spread = steady.spread_degrees;
  steady.draw_held = false;
  draw_for(steady, steady_action, 0.2F);

  RpgCommanderAimComponent tired;
  RpgCommanderActionComponent tired_action;
  tired.draw_held = true;
  tired_action.action_duration = bow_definition().duration_seconds;
  draw_for(tired,
           tired_action,
           release_gate() + RpgCommanderAimComponent::k_steady_hold_seconds + 1.5F);
  float const tired_spread = tired.spread_degrees;
  tired.draw_held = false;
  draw_for(tired, tired_action, 0.2F);

  EXPECT_GT(tired_spread, steady_spread);
  EXPECT_LT(tired.shot_power, steady.shot_power);
}

TEST_F(RpgBowTest, EachAudibleDrawMomentIsAnnouncedExactlyOnce) {
  RpgCommanderAimComponent aim;
  RpgCommanderActionComponent action;
  aim.draw_held = true;
  action.action_duration = bow_definition().duration_seconds;

  constexpr float k_step = 1.0F / 60.0F;
  int started = 0;
  int reached_full = 0;
  int strained = 0;
  float const run =
      release_gate() + RpgCommanderAimComponent::k_steady_hold_seconds + 1.0F;
  for (float elapsed = 0.0F; elapsed < run; elapsed += k_step) {
    auto const frame = Game::Systems::RpgCombat::update_bow_draw(
        aim, action, bow_definition(), 1.0F, k_step);
    action.action_elapsed_time += frame.allowed_delta;
    started += frame.started_draw ? 1 : 0;
    reached_full += frame.reached_full_draw ? 1 : 0;
    strained += frame.started_straining ? 1 : 0;
  }

  EXPECT_EQ(started, 1) << "the draw creak would retrigger every frame";
  EXPECT_EQ(reached_full, 1);
  EXPECT_EQ(strained, 1) << "the strain warning would machine-gun once past the hold";
  EXPECT_GE(aim.full_draw_hold, RpgCommanderAimComponent::k_steady_hold_seconds);
}

TEST_F(RpgBowTest, AShortDrawNeverReportsStrain) {
  RpgCommanderAimComponent aim;
  RpgCommanderActionComponent action;
  aim.draw_held = true;
  action.action_duration = bow_definition().duration_seconds;

  auto const tick = draw_for(aim, action, release_gate() + 0.3F);
  EXPECT_TRUE(tick.reached_full_draw);
  EXPECT_FALSE(tick.started_straining);
}

TEST_F(RpgBowTest, MovementOpensTheAimConeAndAFullDrawClosesIt) {
  RpgCommanderAimComponent planted;
  planted.draw_progress = 1.0F;

  RpgCommanderAimComponent half_drawn;
  half_drawn.draw_progress = 0.4F;

  RpgCommanderAimComponent sprinting;
  sprinting.draw_progress = 1.0F;
  sprinting.move_speed = 4.0F;
  sprinting.running = true;

  float const planted_spread =
      Game::Systems::RpgCombat::aim_spread_degrees(planted, 1.0F);
  EXPECT_LT(planted_spread,
            Game::Systems::RpgCombat::aim_spread_degrees(half_drawn, 1.0F));
  EXPECT_LT(planted_spread,
            Game::Systems::RpgCombat::aim_spread_degrees(sprinting, 1.0F));
  EXPECT_LT(planted_spread,
            Game::Systems::RpgCombat::aim_spread_degrees(planted, 0.1F));
}

TEST_F(RpgBowTest, TheAimRayHitsWhatTheCrosshairCoversAndMissesWhatItDoesNot) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* enemy = make_enemy(world, 0.0F, 8.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  RpgCommanderAimComponent aim;
  aim.view_yaw_degrees = 0.0F;
  aim.view_pitch_degrees = -3.0F;
  auto const forward = Game::Systems::RpgCombat::crosshair_ray(*commander, aim);
  auto const hit = Game::Systems::RpgCombat::raycast_enemy_bodies(
      world, *commander, forward, 20.0F, 0.05F);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->entity_id, enemy->get_id());

  EXPECT_GT(hit->distance, 3.0F);
  EXPECT_LT(hit->distance, 8.5F);

  aim.view_yaw_degrees = 90.0F;
  auto const sideways = Game::Systems::RpgCombat::crosshair_ray(*commander, aim);
  EXPECT_FALSE(Game::Systems::RpgCombat::raycast_enemy_bodies(
                   world, *commander, sideways, 20.0F, 0.05F)
                   .has_value());

  aim.view_yaw_degrees = 0.0F;
  aim.view_pitch_degrees = 45.0F;
  auto const skyward = Game::Systems::RpgCombat::crosshair_ray(*commander, aim);
  EXPECT_FALSE(Game::Systems::RpgCombat::raycast_enemy_bodies(
                   world, *commander, skyward, 20.0F, 0.05F)
                   .has_value());
}

TEST_F(RpgBowTest, TheArrowFliesAtWhatTheChaseCameraHasTheCrosshairOn) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* enemy = make_enemy(world, 2.4F, 9.0F);
  ASSERT_NE(commander, nullptr);

  QVector3D const camera(1.1F, 2.6F, -2.9F);
  QVector3D const chest(2.4F, 1.1F, 9.0F);
  QVector3D const to_target = chest - camera;
  float const flat = std::hypot(to_target.x(), to_target.z());

  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  ASSERT_NE(aim, nullptr);
  aim->view_yaw_degrees =
      std::atan2(to_target.x(), to_target.z()) * 180.0F / 3.14159265F;
  aim->view_pitch_degrees = std::atan2(to_target.y(), flat) * 180.0F / 3.14159265F;
  aim->camera_origin_x = camera.x();
  aim->camera_origin_y = camera.y();
  aim->camera_origin_z = camera.z();
  aim->camera_origin_valid = true;

  auto const ray = Game::Systems::RpgCombat::commander_aim_ray(*commander, *aim);
  auto const shot =
      Game::Systems::RpgCombat::resolve_bow_shot(world, *commander, *aim, ray, 26.0F);
  EXPECT_TRUE(shot.hit_body);
  EXPECT_EQ(shot.hit.entity_id, enemy->get_id());

  auto const sight = Game::Systems::RpgCombat::crosshair_ray(*commander, *aim);

  EXPECT_GT(QVector3D::dotProduct(ray.direction, sight.direction), 0.99999F);

  QVector3D const to_origin = ray.origin - sight.origin;
  EXPECT_LT(to_origin.length() - QVector3D::dotProduct(to_origin, sight.direction),
            1.0e-3F);

  auto const muzzle = Game::Systems::RpgCombat::bow_muzzle(*commander, *aim);
  EXPECT_LT((shot.start - muzzle).length(), 1.0e-3F);
  EXPECT_LT((shot.end - shot.impact).length(), 1.0e-3F);
}

TEST_F(RpgBowTest, TheAimRayIgnoresBodiesBehindTheCommander) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* behind = make_enemy(world, 0.0F, -3.5F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(behind, nullptr);

  QVector3D const camera(0.0F, 2.6F, -6.0F);
  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  ASSERT_NE(aim, nullptr);
  aim->view_yaw_degrees = 0.0F;
  aim->view_pitch_degrees = -25.0F;
  aim->camera_origin_x = camera.x();
  aim->camera_origin_y = camera.y();
  aim->camera_origin_z = camera.z();
  aim->camera_origin_valid = true;

  auto const sight = Game::Systems::RpgCombat::crosshair_ray(*commander, *aim);
  EXPECT_TRUE(Game::Systems::RpgCombat::raycast_enemy_bodies(
                  world, *commander, sight, 26.0F, 0.05F)
                  .has_value());

  auto const ray = Game::Systems::RpgCombat::commander_aim_ray(*commander, *aim);
  EXPECT_FALSE(Game::Systems::RpgCombat::raycast_enemy_bodies(
                   world, *commander, ray, 26.0F, 0.05F)
                   .has_value());
}

TEST_F(RpgBowTest, TheCameraForwardOverridesRawYawAndPitch) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* enemy = make_enemy(world, 0.0F, 9.0F);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);

  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  ASSERT_NE(aim, nullptr);

  aim->view_yaw_degrees = 137.0F;
  aim->view_pitch_degrees = 41.0F;

  QVector3D const camera(0.0F, 2.6F, -3.1F);
  QVector3D const chest(0.0F, 1.1F, 9.0F);
  QVector3D const forward = (chest - camera).normalized();
  aim->camera_origin_x = camera.x();
  aim->camera_origin_y = camera.y();
  aim->camera_origin_z = camera.z();
  aim->camera_origin_valid = true;
  aim->camera_forward_x = forward.x();
  aim->camera_forward_y = forward.y();
  aim->camera_forward_z = forward.z();
  aim->camera_forward_valid = true;

  auto const ray = Game::Systems::RpgCombat::commander_aim_ray(*commander, *aim);
  auto const shot =
      Game::Systems::RpgCombat::resolve_bow_shot(world, *commander, *aim, ray, 26.0F);
  EXPECT_TRUE(shot.hit_body);
  EXPECT_EQ(shot.hit.entity_id, enemy->get_id());
}

TEST_F(RpgBowTest, TheAimRayStopsAtTheNearerOfTwoBodies) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* near_enemy = make_enemy(world, 0.0F, 4.0F);
  make_enemy(world, 0.0F, 9.0F);

  RpgCommanderAimComponent aim;
  auto const hit = Game::Systems::RpgCombat::raycast_enemy_bodies(
      world,
      *commander,
      Game::Systems::RpgCombat::crosshair_ray(*commander, aim),
      20.0F,
      0.05F);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->entity_id, near_enemy->get_id());
}

TEST_F(RpgBowTest, ScatterStaysInsideTheConeAndRepeatsForTheSameShot) {
  Game::Systems::RpgCombat::AimRay ray;
  ray.direction = QVector3D(0.0F, 0.0F, 1.0F);

  auto const first = Game::Systems::RpgCombat::scatter_ray(ray, 4.0F, 7U);
  auto const again = Game::Systems::RpgCombat::scatter_ray(ray, 4.0F, 7U);
  auto const other = Game::Systems::RpgCombat::scatter_ray(ray, 4.0F, 8U);

  EXPECT_FLOAT_EQ(first.direction.x(), again.direction.x());
  EXPECT_FLOAT_EQ(first.direction.y(), again.direction.y());
  EXPECT_NE(first.direction.x(), other.direction.x());

  float const cone_cos = std::cos(4.0F * 3.14159265F / 180.0F);
  EXPECT_GE(QVector3D::dotProduct(first.direction, ray.direction), cone_cos - 1.0e-4F);
  EXPECT_GE(QVector3D::dotProduct(other.direction, ray.direction), cone_cos - 1.0e-4F);
  EXPECT_FLOAT_EQ(Game::Systems::RpgCombat::scatter_ray(ray, 0.0F, 3U).direction.z(),
                  1.0F);
}

TEST_F(RpgBowTest, ABowCommanderStartsWithTheBowOutAndCanSwapToSteel) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  EXPECT_EQ(Game::Systems::RpgCombat::default_weapon_stance(*commander),
            FpvWeaponStance::Bow);

  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  ASSERT_NE(aim, nullptr);
  EXPECT_EQ(aim->stance, FpvWeaponStance::Bow);

  ASSERT_TRUE(Game::Systems::RpgCombat::toggle_weapon_stance(*commander));
  EXPECT_EQ(aim->stance, FpvWeaponStance::Melee);
  ASSERT_TRUE(Game::Systems::RpgCombat::toggle_weapon_stance(*commander));
  EXPECT_EQ(aim->stance, FpvWeaponStance::Bow);
}

TEST_F(RpgBowTest, ASwordCommanderHasNothingToSwapTo) {
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* attack = commander->get_component<AttackComponent>();
  auto* unit = commander->get_component<UnitComponent>();
  attack->can_ranged = false;
  unit->spawn_type = Game::Units::SpawnType::CarthageSwordCommander;

  EXPECT_EQ(Game::Systems::RpgCombat::default_weapon_stance(*commander),
            FpvWeaponStance::Melee);
  Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  EXPECT_FALSE(Game::Systems::RpgCombat::toggle_weapon_stance(*commander));
}

TEST_F(RpgBowTest, AnArrowFliesEvenWhenTheShotHitsNothing) {
  world.add_system(std::make_unique<Game::Systems::ProjectileSystem>());
  auto* projectiles = world.get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectiles, nullptr);

  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  ASSERT_NE(aim, nullptr);
  aim->shot_power = 1.0F;

  auto const empty_shot =
      Game::Systems::RpgCombat::loose_aimed_arrow(world, *commander, bow_definition());
  EXPECT_TRUE(empty_shot.released);
  EXPECT_EQ(empty_shot.target_id, 0U);
  EXPECT_EQ(empty_shot.damage, 0);
  ASSERT_EQ(projectiles->projectiles().size(), 1U);
  EXPECT_GT((projectiles->projectiles().front()->get_end() -
             projectiles->projectiles().front()->get_start())
                .length(),
            10.0F);

  auto* enemy = make_enemy(world, 0.0F, 6.0F);
  auto const aimed_shot =
      Game::Systems::RpgCombat::loose_aimed_arrow(world, *commander, bow_definition());
  EXPECT_TRUE(aimed_shot.released);
  EXPECT_EQ(aimed_shot.target_id, enemy->get_id());
  EXPECT_GT(aimed_shot.damage, 0);
}

TEST_F(RpgBowTest, TheShotWaitsOnTheHeldButtonAndLeavesOnTheRelease) {
  world.add_system(std::make_unique<Game::Systems::ProjectileSystem>());
  auto* projectiles = world.get_system<Game::Systems::ProjectileSystem>();
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  make_enemy(world, 0.0F, 7.0F);

  auto* aim =
      Game::Systems::RpgCombat::sync_commander_aim(*commander, {.primary_held = true});
  ASSERT_NE(aim, nullptr);
  auto* action = commander->add_component<RpgCommanderActionComponent>();
  action->combat_action_id = static_cast<std::uint8_t>(CombatActionId::RpgBowShot);
  action->action_duration = bow_definition().duration_seconds;
  action->action_running = true;

  auto* presentation = commander->add_component<CombatStateComponent>();
  for (int frame = 0; frame < 120; ++frame) {
    Game::Systems::Combat::process_authored_combat_action(
        &world, *commander, presentation, 1.0F / 60.0F);
  }

  EXPECT_EQ(aim->draw_stage, BowDrawStage::FullDraw);
  EXPECT_TRUE(projectiles->projectiles().empty())
      << "the arrow left the bow while the string was still held";

  aim->draw_held = false;
  for (int frame = 0; frame < 60; ++frame) {
    Game::Systems::Combat::process_authored_combat_action(
        &world, *commander, presentation, 1.0F / 60.0F);
  }

  EXPECT_EQ(projectiles->projectiles().size(), 1U);
}

TEST_F(RpgBowTest, DrawPowerScalesTheDamageTheArrowCarries) {
  world.add_system(std::make_unique<Game::Systems::ProjectileSystem>());
  auto* commander = make_archer_commander(world, 0.0F, 0.0F);
  make_enemy(world, 0.0F, 6.0F);
  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(*commander, {});
  ASSERT_NE(aim, nullptr);

  aim->shot_power = 1.0F;
  auto const strong =
      Game::Systems::RpgCombat::loose_aimed_arrow(world, *commander, bow_definition());
  aim->shot_power = RpgCommanderAimComponent::k_min_shot_power;
  auto const weak =
      Game::Systems::RpgCombat::loose_aimed_arrow(world, *commander, bow_definition());

  EXPECT_GT(strong.damage, weak.damage);
  EXPECT_GT(weak.damage, 0);
}

} // namespace
