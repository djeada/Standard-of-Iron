#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <numbers>

#include "animation/clip_manifest.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "systems/arrow_system.h"
#include "systems/cleanup_system.h"
#include "systems/combat_actions/body_impact.h"
#include "systems/combat_actions/combat_action_definition.h"
#include "systems/combat_actions/combat_action_events.h"
#include "systems/combat_actions/weapon_trace.h"
#include "systems/combat_status_effect_system.h"
#include "systems/combat_system/attack_processor.h"
#include "systems/combat_system/auto_engagement.h"
#include "systems/combat_system/combat_action_processor.h"
#include "systems/combat_system/combat_animation_timing.h"
#include "systems/combat_system/combat_hit_resolver.h"
#include "systems/combat_system/combat_mode_processor.h"
#include "systems/combat_system/combat_state_processor.h"
#include "systems/combat_system/combat_status_effect_processor.h"
#include "systems/combat_system/combat_types.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/combat_system/mounted_charge_processor.h"
#include "systems/combat_system/spear_brace_processor.h"
#include "systems/command_service.h"
#include "systems/healing_system.h"
#include "systems/movement_system.h"
#include "systems/owner_registry.h"
#include "systems/projectile_system.h"
#include "systems/rpg_combat_system/rpg_commander_damage.h"
#include "tests/support/movement_test_access.h"
#include "units/troop_config.h"

using namespace Engine::Core;
using namespace Game::Systems;

class CombatModeTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
    CommandService::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

void advance_projectiles(World& world, float total_time = 1.5F, float step = 0.1F) {
  auto* projectile_system = world.get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);
  for (float elapsed = 0.0F; elapsed < total_time; elapsed += step) {
    (void)Game::Systems::Combat::process_combat_status_effects(&world, step);
    projectile_system->update(&world, step);
  }
}

void use_single_body_combat_geometry(std::initializer_list<Entity*> entities) {
  for (auto* entity : entities) {
    if (entity != nullptr) {
      if (auto* unit = entity->get_component<UnitComponent>()) {
        unit->render_individuals_per_unit_override = 1;
      }
    }
  }
}

TEST_F(CombatModeTest, NoAttackModeWhenMovingNearEnemy) {

  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->render_individuals_per_unit_override = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;

  auto* enemy = world->create_entity();
  auto* enemy_transform = enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  enemy_unit->render_individuals_per_unit_override = 1;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Melee);
  EXPECT_FALSE(attacker_attack->in_melee_lock);
}

TEST_F(CombatModeTest, AttackModeTriggersWhenEngaged) {

  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;
  attacker_attack->melee_range = 3.0F;
  attacker_attack->range = 10.0F;

  auto* enemy = world->create_entity();
  auto* enemy_transform = enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Melee);
}

TEST_F(CombatModeTest, UndeadHelpersMapCombatFamiliesAndAutoEngageRoles) {
  EXPECT_EQ(resolve_combat_attack_family(Game::Units::SpawnType::SkeletonSwordsman,
                                         AttackComponent::CombatMode::Melee),
            CombatAttackFamily::Sword);
  EXPECT_EQ(resolve_combat_attack_family(Game::Units::SpawnType::SkeletonArcher,
                                         AttackComponent::CombatMode::Ranged),
            CombatAttackFamily::Bow);
  EXPECT_EQ(resolve_combat_attack_family(Game::Units::SpawnType::GravePriest,
                                         AttackComponent::CombatMode::Ranged),
            CombatAttackFamily::Bow);

  auto* skeleton_swordsman = world->create_entity();
  auto* swordsman_unit =
      skeleton_swordsman->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(swordsman_unit, nullptr);
  swordsman_unit->spawn_type = Game::Units::SpawnType::SkeletonSwordsman;

  auto* skeleton_archer = world->create_entity();
  auto* archer_unit =
      skeleton_archer->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(archer_unit, nullptr);
  archer_unit->spawn_type = Game::Units::SpawnType::SkeletonArcher;

  auto* grave_priest = world->create_entity();
  auto* priest_unit = grave_priest->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(priest_unit, nullptr);
  priest_unit->spawn_type = Game::Units::SpawnType::GravePriest;

  EXPECT_TRUE(Game::Systems::Combat::should_auto_engage_melee(skeleton_swordsman));
  EXPECT_FALSE(Game::Systems::Combat::should_auto_engage_melee(skeleton_archer));
  EXPECT_FALSE(Game::Systems::Combat::should_auto_engage_melee(grave_priest));
}

TEST_F(CombatModeTest, MoveCommandScalesHoldExitToCurrentKneelDepth) {
  auto* unit = world->create_entity();
  unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  unit->add_component<MovementComponent>();
  auto* hold_mode = unit->add_component<HoldModeComponent>();
  hold_mode->active = true;
  hold_mode->stand_up_duration = 2.0F;
  hold_mode->kneel_entry_progress = 0.5F;

  CommandService::move_unit(*world, unit->get_id(), QVector3D(8.0F, 0.0F, 6.0F));

  EXPECT_FALSE(hold_mode->active);
  EXPECT_FLOAT_EQ(hold_mode->exit_cooldown, 1.0F);
}

TEST_F(CombatModeTest, ManualMoveSuppressesRangedOpportunisticAttackStop) {
  auto* archer = world->create_entity();
  archer->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* archer_unit = archer->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  archer_unit->owner_id = 1;
  archer_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto* archer_attack = archer->add_component<AttackComponent>();
  archer_attack->can_melee = false;
  archer_attack->can_ranged = true;
  archer_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  archer_attack->current_mode = AttackComponent::CombatMode::Ranged;
  archer_attack->range = 10.0F;
  archer_attack->melee_range = 1.0F;
  archer_attack->cooldown = 0.0F;
  archer_attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  CommandService::move_unit(*world, archer->get_id(), QVector3D(7.0F, 0.0F, 0.0F));

  auto* movement = archer->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_TRUE(movement->get_has_target());
  EXPECT_TRUE(Game::Systems::Combat::suppresses_opportunistic_combat(archer));

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_TRUE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_goal_x(), 7.0F);
  EXPECT_FALSE(archer->has_component<AttackTargetComponent>());
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(CombatModeTest, ManualMultiUnitMoveSuppressesAutoEngagementWhileMoving) {
  auto* first = world->create_entity();
  first->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* first_unit = first->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  first_unit->owner_id = 1;
  first_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* first_attack = first->add_component<AttackComponent>();
  first_attack->can_melee = true;
  first_attack->can_ranged = false;
  first_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  first_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto* flank = world->create_entity();
  flank->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* flank_unit = flank->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  flank_unit->owner_id = 1;
  flank_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* flank_attack = flank->add_component<AttackComponent>();
  flank_attack->can_melee = true;
  flank_attack->can_ranged = false;
  flank_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  flank_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  CommandService::MoveOptions const options;
  CommandService::move_units(
      *world,
      {first->get_id(), flank->get_id()},
      {QVector3D(18.0F, 0.0F, 0.0F), QVector3D(18.0F, 0.0F, 1.0F)},
      options);

  auto* first_movement = first->get_component<MovementComponent>();
  ASSERT_NE(first_movement, nullptr);
  EXPECT_TRUE(first_movement->get_has_target());
  EXPECT_TRUE(Game::Systems::Combat::suppresses_opportunistic_combat(first));

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::AutoEngagement auto_engagement;
  auto_engagement.process(world.get(), query_context, 0.016F);

  EXPECT_FALSE(first->has_component<AttackTargetComponent>());
  EXPECT_FALSE(flank->has_component<AttackTargetComponent>());
}

TEST_F(CombatModeTest, AutoEngagementResumesAfterManualMoveArrives) {
  auto* spearman = world->create_entity();
  spearman->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* spearman_unit = spearman->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  spearman_unit->owner_id = 1;
  spearman_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* spearman_attack = spearman->add_component<AttackComponent>();
  spearman_attack->can_melee = true;
  spearman_attack->can_ranged = false;
  spearman_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  spearman_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  CommandService::move_unit(*world, spearman->get_id(), QVector3D(0.02F, 0.0F, 0.0F));

  auto* movement = spearman->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_TRUE(Game::Systems::Combat::suppresses_opportunistic_combat(spearman));

  MovementSystem movement_system;
  movement_system.update(world.get(), 0.016F);

  EXPECT_FALSE(movement->get_has_target());
  EXPECT_FALSE(Game::Systems::Combat::suppresses_opportunistic_combat(spearman));

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::AutoEngagement auto_engagement;
  auto_engagement.process(world.get(), query_context, 0.016F);

  auto* attack_target = spearman->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, enemy->get_id());
}

TEST_F(CombatModeTest, HoldModeSpearmanStillLocksEnemyInMelee) {
  auto* spearman = world->create_entity();
  auto* spearman_transform =
      spearman->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* spearman_unit = spearman->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  spearman_unit->owner_id = 1;
  spearman_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* spearman_attack = spearman->add_component<AttackComponent>();
  spearman_attack->can_melee = true;
  spearman_attack->can_ranged = false;
  spearman_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  spearman_attack->current_mode = AttackComponent::CombatMode::Melee;
  spearman_attack->melee_range = 1.0F;
  spearman_attack->time_since_last = 1.0F;
  auto* hold_mode = spearman->add_component<HoldModeComponent>();
  hold_mode->active = true;

  auto* enemy = world->create_entity();
  auto* enemy_transform = enemy->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  enemy_unit->spawn_type = Game::Units::SpawnType::Knight;
  auto* enemy_attack = enemy->add_component<AttackComponent>();
  enemy_attack->can_melee = true;
  enemy_attack->can_ranged = false;
  enemy_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  enemy_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_TRUE(hold_mode->active);
  EXPECT_TRUE(spearman_attack->in_melee_lock);
  EXPECT_EQ(spearman_attack->melee_lock_target_id, enemy->get_id());
  EXPECT_TRUE(enemy_attack->in_melee_lock);
  EXPECT_EQ(enemy_attack->melee_lock_target_id, spearman->get_id());
  EXPECT_FLOAT_EQ(spearman_transform->position.x, 0.0F);
  EXPECT_FLOAT_EQ(enemy_transform->position.x, 1.5F);
}

TEST_F(CombatModeTest, BuildingsExcludedFromCombatMode) {

  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;
  attacker_attack->melee_range = 3.0F;
  attacker_attack->range = 10.0F;

  auto* building = world->create_entity();
  auto* building_transform =
      building->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto* building_unit = building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = building->get_id();
  attack_target->should_chase = false;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Ranged);
}

TEST_F(CombatModeTest, RangedUnitUsesRangedModeWhenNotEngaged) {

  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = false;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;

  auto* enemy = world->create_entity();
  auto* enemy_transform = enemy->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Ranged);
  EXPECT_FALSE(attacker_attack->in_melee_lock);
}

TEST_F(CombatModeTest, CombatDoesNotDisplaceBuildingMeleeLockParticipants) {

  auto* unit = world->create_entity();
  auto* unit_transform = unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto* unit_attack = unit->add_component<AttackComponent>();
  unit_attack->can_melee = true;
  unit_attack->can_ranged = false;

  auto* building = world->create_entity();
  auto* building_transform =
      building->add_component<TransformComponent>(10.0F, 0.0F, 0.0F);
  auto* building_comp = building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_comp->owner_id = 2;
  auto* building_attack = building->add_component<AttackComponent>();
  building_attack->can_melee = false;
  building_attack->can_ranged = true;
  building->add_component<BuildingComponent>();

  float const initial_unit_x = unit_transform->position.x;
  float const initial_building_x = building_transform->position.x;

  unit_attack->in_melee_lock = true;
  unit_attack->melee_lock_target_id = building->get_id();
  building_attack->in_melee_lock = true;
  building_attack->melee_lock_target_id = unit->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_EQ(unit_transform->position.x, initial_unit_x);
  EXPECT_EQ(building_transform->position.x, initial_building_x);
}

TEST_F(CombatModeTest, CombatDoesNotDisplaceHomeMeleeLockParticipants) {

  auto* unit = world->create_entity();
  auto* unit_transform = unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto* unit_attack = unit->add_component<AttackComponent>();
  unit_attack->can_melee = true;
  unit_attack->can_ranged = false;

  auto* home = world->create_entity();
  auto* home_transform = home->add_component<TransformComponent>(10.0F, 0.0F, 0.0F);
  auto* home_comp = home->add_component<UnitComponent>(1000, 1000, 0.0F, 15.0F);
  home_comp->owner_id = 2;
  auto* home_attack = home->add_component<AttackComponent>();
  home_attack->can_melee = false;
  home_attack->can_ranged = false;
  home->add_component<BuildingComponent>();

  float const initial_unit_x = unit_transform->position.x;
  float const initial_home_x = home_transform->position.x;

  unit_attack->in_melee_lock = true;
  unit_attack->melee_lock_target_id = home->get_id();
  home_attack->in_melee_lock = true;
  home_attack->melee_lock_target_id = unit->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_EQ(unit_transform->position.x, initial_unit_x);
  EXPECT_EQ(home_transform->position.x, initial_home_x);
}

TEST_F(CombatModeTest, InitialMeleeLockDoesNotDirectlyDisplaceTransforms) {
  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->melee_range = 1.5F;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->melee_cooldown = 0.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_TRUE(attacker_attack->in_melee_lock);
  EXPECT_EQ(attacker_attack->melee_lock_target_id, enemy->get_id());
  EXPECT_FLOAT_EQ(attacker_transform->position.x, 0.0F);
}

TEST_F(CombatModeTest, FpvCommanderUsesRpgMeleeRulesInsteadOfRtsLock) {
  auto* commander = world->create_entity();
  commander->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* commander_unit = commander->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  commander_unit->owner_id = 1;
  auto* commander_attack = commander->add_component<AttackComponent>();
  commander_attack->can_melee = true;
  commander_attack->can_ranged = false;
  commander_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  commander_attack->current_mode = AttackComponent::CombatMode::Melee;
  auto* commander_data = commander->add_component<CommanderComponent>();
  commander_data->fpv_controlled = true;
  auto* commander_rpg = commander->add_component<RpgHealthComponent>();
  commander_rpg->active = true;
  commander_rpg->rpg_hp = 120;
  commander_rpg->rpg_max_hp = 120;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.6F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({commander, enemy});
  auto* enemy_attack = enemy->add_component<AttackComponent>();
  enemy_attack->can_melee = true;
  enemy_attack->can_ranged = false;
  enemy_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  enemy_attack->current_mode = AttackComponent::CombatMode::Melee;
  enemy_attack->cooldown = 0.0F;
  enemy_attack->melee_cooldown = 0.0F;
  enemy_attack->time_since_last = 1.0F;

  auto* enemy_target = enemy->add_component<AttackTargetComponent>();
  enemy_target->target_id = commander->get_id();
  enemy_target->should_chase = true;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_FALSE(commander_attack->in_melee_lock);
  EXPECT_EQ(commander_attack->melee_lock_target_id, 0U);
  EXPECT_FALSE(enemy_attack->in_melee_lock);
  EXPECT_EQ(enemy_attack->melee_lock_target_id, 0U);
  EXPECT_LT(commander_rpg->rpg_hp, commander_rpg->rpg_max_hp);
}

TEST_F(CombatModeTest, RpgAttackerIgnoresStaleRtsAttackTargetComponent) {
  auto* commander = world->create_entity();
  commander->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* commander_unit = commander->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  commander_unit->owner_id = 1;
  auto* commander_attack = commander->add_component<AttackComponent>();
  commander_attack->can_melee = true;
  commander_attack->can_ranged = false;
  commander_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  commander_attack->current_mode = AttackComponent::CombatMode::Melee;
  commander_attack->cooldown = 0.0F;
  commander_attack->melee_cooldown = 0.0F;
  commander_attack->time_since_last = 1.0F;
  auto* commander_data = commander->add_component<CommanderComponent>();
  commander_data->fpv_controlled = true;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* stale_target = commander->add_component<AttackTargetComponent>();
  stale_target->target_id = enemy->get_id();
  stale_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_EQ(enemy_unit->health, 100);
  EXPECT_TRUE(commander->has_component<AttackTargetComponent>());
}

TEST_F(CombatModeTest, MeleeAttackUsesElephantCombatRadius) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 0.75F;
  attack->melee_cooldown = 0.0F;

  auto* elephant = world->create_entity();
  elephant->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* elephant_unit = elephant->add_component<UnitComponent>(300, 300, 1.0F, 12.0F);
  elephant_unit->owner_id = 2;
  elephant_unit->spawn_type = Game::Units::SpawnType::Elephant;
  auto* elephant_comp = elephant->add_component<ElephantComponent>();
  elephant_comp->trample_radius = 2.5F;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = elephant->get_id();
  attack_target->should_chase = true;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_TRUE(attack->in_melee_lock);
  EXPECT_EQ(attack->melee_lock_target_id, elephant->get_id());

  auto* combat_state = attacker->get_component<CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  EXPECT_NE(combat_state->animation_state, CombatAnimationState::Idle);
  EXPECT_EQ(combat_state->attack_family, CombatAttackFamily::Spear);
}

TEST_F(CombatModeTest, SurroundedElephantKeepsStableMeleeFacing) {
  auto* front_attacker = world->create_entity();
  front_attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* front_unit =
      front_attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  front_unit->owner_id = 1;
  auto* front_attack = front_attacker->add_component<AttackComponent>();
  front_attack->can_melee = true;
  front_attack->can_ranged = false;
  front_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  front_attack->current_mode = AttackComponent::CombatMode::Melee;
  front_attack->in_melee_lock = true;

  auto* elephant = world->create_entity();
  auto* elephant_transform =
      elephant->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* elephant_unit = elephant->add_component<UnitComponent>(300, 300, 1.0F, 12.0F);
  elephant_unit->owner_id = 2;
  elephant_unit->spawn_type = Game::Units::SpawnType::Elephant;
  elephant->add_component<ElephantComponent>();
  auto* elephant_attack = elephant->add_component<AttackComponent>();
  elephant_attack->can_melee = true;
  elephant_attack->can_ranged = false;
  elephant_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  elephant_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto* side_attacker = world->create_entity();
  side_attacker->add_component<TransformComponent>(3.0F, 0.0F, 3.0F);
  auto* side_unit = side_attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  side_unit->owner_id = 1;
  use_single_body_combat_geometry({front_attacker, side_attacker});
  auto* side_attack = side_attacker->add_component<AttackComponent>();
  side_attack->can_melee = true;
  side_attack->can_ranged = false;
  side_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  side_attack->current_mode = AttackComponent::CombatMode::Melee;
  side_attack->in_melee_lock = true;

  front_attack->melee_lock_target_id = elephant->get_id();
  side_attack->melee_lock_target_id = elephant->get_id();
  elephant_attack->in_melee_lock = true;
  elephant_attack->melee_lock_target_id = front_attacker->get_id();

  float const initial_x = elephant_transform->position.x;
  float const initial_z = elephant_transform->position.z;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_FLOAT_EQ(elephant_transform->position.x, initial_x);
  EXPECT_FLOAT_EQ(elephant_transform->position.z, initial_z);

  EXPECT_FALSE(elephant_transform->has_desired_yaw);
  EXPECT_FLOAT_EQ(elephant_transform->desired_yaw, -90.0F);
}

TEST_F(CombatModeTest, NearestEnemyPrefersClosestValidUnit) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto* building = world->create_entity();
  building->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* building_unit = building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto* pending_enemy = world->create_entity();
  pending_enemy->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto* pending_unit =
      pending_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  pending_unit->owner_id = 2;
  pending_enemy->add_component<PendingRemovalComponent>();

  auto* nearest_enemy = world->create_entity();
  nearest_enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* nearest_unit =
      nearest_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  nearest_unit->owner_id = 2;

  auto* far_enemy = world->create_entity();
  far_enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto* far_unit = far_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  far_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  auto* target =
      Game::Systems::Combat::find_nearest_enemy(attacker, query_context, 10.0F);

  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->get_id(), nearest_enemy->get_id());
}

TEST_F(CombatModeTest, NearestEnemyIgnoresAlliesDeadAndRemovedUnits) {
  auto& owners = OwnerRegistry::instance();
  owners.register_owner_with_id(1, OwnerType::Player, "Player");
  owners.register_owner_with_id(2, OwnerType::AI, "Ally");
  owners.register_owner_with_id(3, OwnerType::AI, "Enemy");
  owners.set_owner_team(1, 1);
  owners.set_owner_team(2, 1);
  owners.set_owner_team(3, 2);

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto* ally = world->create_entity();
  ally->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* ally_unit = ally->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ally_unit->owner_id = 2;

  auto* dead_enemy = world->create_entity();
  dead_enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* dead_unit = dead_enemy->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
  dead_unit->owner_id = 3;

  auto* removed_enemy = world->create_entity();
  removed_enemy->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* removed_unit =
      removed_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  removed_unit->owner_id = 3;
  removed_enemy->add_component<PendingRemovalComponent>();

  auto* valid_enemy = world->create_entity();
  valid_enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto* valid_unit = valid_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  valid_unit->owner_id = 3;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  auto* target =
      Game::Systems::Combat::find_nearest_enemy(attacker, query_context, 10.0F);

  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->get_id(), valid_enemy->get_id());
}

TEST_F(CombatModeTest, CombatQueryContextSkipsRemovedAndDeadSpatialEntries) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto* removed_enemy = world->create_entity();
  removed_enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* removed_unit =
      removed_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  removed_unit->owner_id = 2;
  removed_enemy->add_component<PendingRemovalComponent>();

  auto* dead_enemy = world->create_entity();
  dead_enemy->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto* dead_unit = dead_enemy->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
  dead_unit->owner_id = 2;

  auto* building = world->create_entity();
  building->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* building_unit = building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto* valid_enemy = world->create_entity();
  valid_enemy->add_component<TransformComponent>(2.5F, 0.0F, 0.0F);
  auto* valid_unit = valid_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  valid_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());

  EXPECT_EQ(query_context.entities_by_id.count(removed_enemy->get_id()), 0U);
  EXPECT_EQ(query_context.entities_by_id.count(dead_enemy->get_id()), 0U);
  EXPECT_FALSE(std::any_of(query_context.units.begin(),
                           query_context.units.end(),
                           [removed_enemy, dead_enemy](Entity* entity) {
                             return entity == removed_enemy || entity == dead_enemy;
                           }));

  std::uint64_t scan_iterations = 0;
  auto* target = Game::Systems::Combat::find_nearest_enemy(
      attacker, query_context, 10.0F, &scan_iterations);

  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->get_id(), valid_enemy->get_id());
  EXPECT_EQ(scan_iterations, 2U);
}

TEST_F(CombatModeTest, EnemyValidityHelperAllowsBuildingsOnlyWhenRequested) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto* building = world->create_entity();
  building->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* building_unit = building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  EXPECT_FALSE(
      Game::Systems::Combat::is_valid_enemy_unit(attacker_unit, building, false));
  EXPECT_TRUE(
      Game::Systems::Combat::is_valid_enemy_unit(attacker_unit, building, true));
}

TEST_F(CombatModeTest, AutoEngagementUsesNearestEnemyInsideGuardRadius) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  auto* guard_mode = attacker->add_component<GuardModeComponent>();
  guard_mode->active = true;
  guard_mode->guard_position_x = 0.0F;
  guard_mode->guard_position_z = 0.0F;
  guard_mode->guard_radius = 3.0F;

  auto* inside_enemy = world->create_entity();
  inside_enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* inside_unit = inside_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  inside_unit->owner_id = 2;

  auto* outside_enemy = world->create_entity();
  outside_enemy->add_component<TransformComponent>(5.0F, 0.0F, 0.0F);
  auto* outside_unit =
      outside_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  outside_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::AutoEngagement auto_engagement;
  auto_engagement.process(world.get(), query_context, 0.016F);

  auto* attack_target = attacker->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, inside_enemy->get_id());
}

TEST_F(CombatModeTest, AutoEngagementIgnoresCloserBuildingAndTargetsEnemyUnit) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;

  auto* building = world->create_entity();
  building->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* building_unit = building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::AutoEngagement auto_engagement;
  auto_engagement.process(world.get(), query_context, 0.016F);

  auto* attack_target = attacker->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, enemy->get_id());
  EXPECT_TRUE(attack_target->should_chase);
}

TEST_F(CombatModeTest, ChasingUnitDoesNotRequestNewPathEveryFrame) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(12.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context, 0.016F);

  auto* movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  float const first_goal_x = movement->get_goal_x();
  float const first_goal_y = movement->get_goal_y();
  float const first_target_x = movement->get_target_x();
  float const first_target_y = movement->get_target_y();

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context, 0.016F);

  EXPECT_TRUE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_goal_x(), first_goal_x);
  EXPECT_FLOAT_EQ(movement->get_goal_y(), first_goal_y);
  EXPECT_FLOAT_EQ(movement->get_target_x(), first_target_x);
  EXPECT_FLOAT_EQ(movement->get_target_y(), first_target_y);
}

TEST_F(CombatModeTest, ShortUnobstructedChaseUsesDirectMovement) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_TRUE(movement->get_path().empty());
}

TEST_F(CombatModeTest, MeleeChaseHasNoTooFarToStrikeTooCloseToMoveDeadZone) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->render_individuals_per_unit_override = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 1.6F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(2.2F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  enemy_unit->render_individuals_per_unit_override = 1;

  ASSERT_FALSE(Combat::is_in_range(attacker, enemy, attack->melee_range));
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const query_context = Combat::build_combat_query_context(world.get());
  Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_LT(movement->get_goal_x(), 2.2F);
  EXPECT_GT(movement->get_goal_x(), 0.0F);
}

TEST_F(CombatModeTest, RangedUnitHonorsExplicitTargetOutsideAutoEngagementRange) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = true;
  attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attack->current_mode = AttackComponent::CombatMode::Ranged;
  attack->range = 6.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(16.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const query_context = Combat::build_combat_query_context(world.get());
  Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_EQ(attack_target->target_id, enemy->get_id());
}

TEST_F(CombatModeTest, ChasingUnitRequestsNewPathWhenTargetMovesFarEnough) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto* enemy = world->create_entity();
  auto* enemy_transform = enemy->add_component<TransformComponent>(12.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context, 0.016F);

  auto* movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  float const first_goal_x = movement->get_goal_x();

  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, movement->get_goal_x());
  MovementTestAccess::set_target_y(*movement, movement->get_goal_y());

  enemy_transform->position.x = 16.0F;
  enemy_transform->position.z = 0.0F;

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context, 0.016F);

  EXPECT_TRUE(movement->get_has_target());
  EXPECT_NE(movement->get_goal_x(), first_goal_x);
}

TEST_F(CombatModeTest, EquivalentChaseKeepsSameGoal) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(12.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context, 0.016F);

  auto* movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  float const first_goal_x = movement->get_goal_x();
  float const first_goal_y = movement->get_goal_y();

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context, 0.016F);

  EXPECT_FLOAT_EQ(movement->get_goal_x(), first_goal_x);
  EXPECT_FLOAT_EQ(movement->get_goal_y(), first_goal_y);
}

TEST_F(CombatModeTest, UnitStillAttacksWhenTargetReachesRange) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.0F;
  attack->damage = 10;
  attack->melee_damage = 10;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_LT(enemy_unit->health, 100);
  auto* movement = attacker->get_component<MovementComponent>();
  EXPECT_TRUE((movement == nullptr) || !movement->get_has_target());
}

TEST_F(CombatModeTest, RtsMeleeDamageComesFromVisibleActionContact) {
  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  attacker_transform->rotation.y = 90.0F;
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.0F;
  attack->damage = 10;
  attack->melee_damage = 10;
  attack->melee_cooldown = 1.0F;
  attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  Game::Systems::Combat::process_attacks(
      world.get(),
      Game::Systems::Combat::build_combat_query_context(world.get()),
      0.016F);
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_TRUE(action->action_running);
  EXPECT_EQ(enemy_unit->health, 100);
  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.39F);
  EXPECT_EQ(enemy_unit->health, 100);
  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.02F);
  EXPECT_LT(enemy_unit->health, 100);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
}

TEST_F(CombatModeTest, DeferredMeleeStrikeCancelsWhenAttackerFacesAway) {
  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  attacker_transform->rotation.y = -90.0F;
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.0F;
  attack->melee_damage = 10;
  attack->melee_cooldown = 1.0F;
  attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});
  auto* target = attacker->add_component<AttackTargetComponent>();
  target->target_id = enemy->get_id();
  target->should_chase = false;

  Game::Systems::Combat::process_attacks(
      world.get(),
      Game::Systems::Combat::build_combat_query_context(world.get()),
      0.016F);
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  attacker_transform->rotation.y = -90.0F;
  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.45F);

  EXPECT_TRUE(action->action_completed);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(CombatModeTest, DeferredMeleeStrikeCancelsWhenTargetDies) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.0F;
  attack->damage = 10;
  attack->melee_damage = 10;
  attack->melee_cooldown = 1.0F;
  attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  Game::Systems::Combat::process_attacks(
      world.get(),
      Game::Systems::Combat::build_combat_query_context(world.get()),
      0.016F);
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);

  enemy_unit->health = 0;
  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.45F);
  EXPECT_TRUE(action->action_completed);
}

TEST_F(CombatModeTest, RtsMeleeActionInterruptionPreventsContactDamage) {
  auto* attacker = world->create_entity();
  auto* transform = attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  transform->rotation.y = 90.0F;
  auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_ranged = false;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->time_since_last = 2.0F;
  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});
  auto* target = attacker->add_component<AttackTargetComponent>();
  target->target_id = enemy->get_id();

  Game::Systems::Combat::process_attacks(
      world.get(),
      Game::Systems::Combat::build_combat_query_context(world.get()),
      0.016F);
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  attacker->add_component<StaggerComponent>(0.5F);
  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.5F);

  EXPECT_TRUE(action->action_completed);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(CombatModeTest, RtsSpearMeleeUsesAuthoredContactAction) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.0F;
  attack->damage = 10;
  attack->melee_damage = 10;
  attack->melee_cooldown = 1.25F;
  attack->time_since_last = 1.25F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  Game::Systems::Combat::process_attacks(
      world.get(),
      Game::Systems::Combat::build_combat_query_context(world.get()),
      0.016F);

  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RtsSpearThrust));
  auto const* combat_state = attacker->get_component<CombatStateComponent>();
  ASSERT_NE(combat_state, nullptr);
  EXPECT_EQ(combat_state->attack_family, CombatAttackFamily::Spear);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RtsSpearThrust);
  ASSERT_NE(definition, nullptr);
  EXPECT_TRUE(std::any_of(
      definition->events.begin(), definition->events.end(), [](auto const& event) {
        return event.type ==
               Game::Systems::CombatActions::CombatActionEventType::ActiveStart;
      }));
}

TEST_F(CombatModeTest, FormationRearRankDoesNotJoinContactBeforeVacancy) {
  auto make_member = [&](float z, int rank) {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(0.0F, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    unit->render_individuals_per_unit_override = 1;
    auto* attack = entity->add_component<AttackComponent>();
    attack->can_ranged = false;
    attack->current_mode = AttackComponent::CombatMode::Melee;
    attack->time_since_last = 2.0F;
    auto* mode = entity->add_component<FormationModeComponent>();
    mode->active = true;
    mode->formation_id = 77;
    mode->stable_rank = rank;
    mode->stable_file = 0;
    return entity;
  };
  auto* front = make_member(0.0F, 0);
  auto* rear = make_member(-1.0F, 1);
  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  enemy_unit->render_individuals_per_unit_override = 1;
  for (auto* member : {front, rear}) {
    auto* target = member->add_component<AttackTargetComponent>();
    target->target_id = enemy->get_id();
    target->should_chase = true;
  }

  Game::Systems::Combat::process_attacks(
      world.get(),
      Game::Systems::Combat::build_combat_query_context(world.get()),
      0.016F);

  EXPECT_NE(front->get_component<RpgCommanderActionComponent>(), nullptr);
  EXPECT_EQ(rear->get_component<RpgCommanderActionComponent>(), nullptr);
  EXPECT_EQ(rear->get_component<AttackTargetComponent>(), nullptr);
}

TEST_F(CombatModeTest, FrontFormationVacancyRefillsFromSameFile) {
  auto* front = world->create_entity();
  front->add_component<TransformComponent>(4.0F, 0.0F, 6.0F);
  auto* front_unit = front->add_component<UnitComponent>(20, 20, 1.0F, 12.0F);
  front_unit->owner_id = 1;
  auto* front_mode = front->add_component<FormationModeComponent>();
  front_mode->active = true;
  front_mode->formation_id = 91;
  front_mode->stable_slot_id = 0;
  front_mode->stable_rank = 0;
  front_mode->stable_file = 2;
  front_mode->stable_slot_x = 4.0F;
  front_mode->stable_slot_z = 6.0F;

  auto* reserve = world->create_entity();
  reserve->add_component<TransformComponent>(4.0F, 0.0F, 3.0F);
  auto* reserve_unit = reserve->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  reserve_unit->owner_id = 1;
  reserve->add_component<MovementComponent>();
  auto* reserve_mode = reserve->add_component<FormationModeComponent>();
  reserve_mode->active = true;
  reserve_mode->formation_id = 91;
  reserve_mode->stable_slot_id = 4;
  reserve_mode->stable_rank = 1;
  reserve_mode->stable_file = 2;

  Game::Systems::Combat::deal_damage(world.get(), front, 20, 0);

  EXPECT_EQ(reserve_mode->stable_rank, 0);
  EXPECT_EQ(reserve_mode->stable_slot_id, 0);
  auto* movement = reserve->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_NEAR(movement->get_goal_x(), 4.0F, 0.01F);
  EXPECT_NEAR(movement->get_goal_y(), 6.0F, 0.01F);
}

TEST_F(CombatModeTest, MeleeContactTimingComesFromAnimationManifest) {
  constexpr float k_cooldown = 1.5F;

  EXPECT_NEAR(
      Game::Systems::Combat::melee_contact_time_for_unit(
          Game::Units::SpawnType::Knight, CombatAttackFamily::Sword, 0U, k_cooldown),
      Animation::authored_humanoid_clip_markers(
          Animation::k_humanoid_attack_sword_a_clip,
          Animation::HumanoidClipProfile::SwordReady)
              .contact *
          k_cooldown,
      0.0001F);
  EXPECT_NEAR(Game::Systems::Combat::melee_contact_time_for_unit(
                  Game::Units::SpawnType::MountedKnight,
                  CombatAttackFamily::Sword,
                  2U,
                  k_cooldown),
              Animation::authored_humanoid_clip_markers(
                  Animation::k_humanoid_riding_sword_strike_clip,
                  Animation::HumanoidClipProfile::SwordReady)
                      .contact *
                  k_cooldown,
              0.0001F);
  EXPECT_FLOAT_EQ(
      Game::Systems::Combat::melee_contact_time_for_unit(
          Game::Units::SpawnType::Archer, CombatAttackFamily::Bow, 0U, k_cooldown),
      CombatStateComponent::k_melee_contact_fraction * k_cooldown);
}

TEST_F(CombatModeTest, MeleeContactFractionIsWithinSwing) {
  EXPECT_GT(CombatStateComponent::k_melee_contact_fraction, 0.0F);
  EXPECT_LT(CombatStateComponent::k_melee_contact_fraction, 1.0F);
}

TEST_F(CombatModeTest, RepeatedMeleeLockRestartsAttackCycleWithoutResettingSeed) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->melee_cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  attacker_attack->in_melee_lock = true;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.5F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  auto* enemy_attack = enemy->add_component<AttackComponent>();
  enemy_attack->in_melee_lock = true;
  enemy_attack->melee_lock_target_id = attacker->get_id();

  attacker_attack->melee_lock_target_id = enemy->get_id();

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto* combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::Idle;
  combat_state->attack_offset = 0.123F;
  combat_state->attack_family = CombatAttackFamily::Spear;
  combat_state->attack_variant = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_EQ(combat_state->animation_state, CombatAnimationState::Advance);
  EXPECT_FLOAT_EQ(combat_state->state_time, 0.0F);
  EXPECT_FLOAT_EQ(combat_state->state_duration,
                  CombatStateComponent::k_advance_duration);
  EXPECT_FLOAT_EQ(combat_state->attack_offset, 0.123F);
  EXPECT_EQ(combat_state->attack_family, CombatAttackFamily::Spear);
  EXPECT_EQ(combat_state->attack_variant, 2);
}

TEST_F(CombatModeTest, RtsAttackProcessorDoesNotDriveCommanderFpvAttacks) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Knight;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->melee_cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  attacker_attack->in_melee_lock = true;
  auto* commander = attacker->add_component<CommanderComponent>();
  commander->fpv_controlled = true;
  commander->combo_step = 3;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.8F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  auto* enemy_attack = enemy->add_component<AttackComponent>();
  enemy_attack->in_melee_lock = true;
  enemy_attack->melee_lock_target_id = attacker->get_id();

  attacker_attack->melee_lock_target_id = enemy->get_id();

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_EQ(attacker->get_component<CombatStateComponent>(), nullptr);
  EXPECT_TRUE(attacker->has_component<AttackTargetComponent>());
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(CombatModeTest, CommanderFinisherFlagControlsFollowupPhaseDurations) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(attacker_unit, nullptr);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Knight;
  auto* commander = attacker->add_component<CommanderComponent>();
  ASSERT_NE(commander, nullptr);
  commander->fpv_controlled = true;

  auto* normal_state = attacker->add_component<CombatStateComponent>();
  ASSERT_NE(normal_state, nullptr);
  normal_state->animation_state = CombatAnimationState::Advance;
  normal_state->state_duration = CombatStateComponent::k_advance_duration;
  normal_state->state_time = normal_state->state_duration;
  normal_state->finisher_attack = false;

  Game::Systems::Combat::process_combat_state(world.get(), 0.0F);

  EXPECT_EQ(normal_state->animation_state, CombatAnimationState::WindUp);
  EXPECT_FLOAT_EQ(normal_state->state_duration,
                  CombatStateComponent::k_wind_up_duration * 1.65F);

  auto* finisher = world->create_entity();
  finisher->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* finisher_unit = finisher->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(finisher_unit, nullptr);
  finisher_unit->owner_id = 1;
  finisher_unit->spawn_type = Game::Units::SpawnType::Knight;
  auto* finisher_commander = finisher->add_component<CommanderComponent>();
  ASSERT_NE(finisher_commander, nullptr);
  finisher_commander->fpv_controlled = true;

  auto* finisher_state = finisher->add_component<CombatStateComponent>();
  ASSERT_NE(finisher_state, nullptr);
  finisher_state->animation_state = CombatAnimationState::Advance;
  finisher_state->state_duration = CombatStateComponent::k_advance_duration;
  finisher_state->state_time = finisher_state->state_duration;
  finisher_state->finisher_attack = true;

  Game::Systems::Combat::process_combat_state(world.get(), 0.0F);

  EXPECT_EQ(finisher_state->animation_state, CombatAnimationState::WindUp);
  EXPECT_FLOAT_EQ(finisher_state->state_duration,
                  CombatStateComponent::k_wind_up_duration * 2.10F);
}

TEST_F(CombatModeTest, MeleeLockKeepsCurrentTargetEvenWhenCloserEnemyExists) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->melee_cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;

  auto* locked_enemy = world->create_entity();
  locked_enemy->add_component<TransformComponent>(0.7F, 0.0F, 0.0F);
  auto* locked_enemy_unit =
      locked_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  locked_enemy_unit->owner_id = 2;
  auto* locked_enemy_attack = locked_enemy->add_component<AttackComponent>();

  auto* closer_enemy = world->create_entity();
  closer_enemy->add_component<TransformComponent>(0.35F, 0.0F, 0.0F);
  auto* closer_enemy_unit =
      closer_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  closer_enemy_unit->owner_id = 2;

  attacker_attack->in_melee_lock = true;
  attacker_attack->melee_lock_target_id = locked_enemy->get_id();
  locked_enemy_attack->in_melee_lock = true;
  locked_enemy_attack->melee_lock_target_id = attacker->get_id();

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = closer_enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_EQ(attack_target->target_id, locked_enemy->get_id());
  EXPECT_LT(locked_enemy_unit->health, 100);
  EXPECT_EQ(closer_enemy_unit->health, 100);
}

TEST_F(CombatModeTest, AdditionalAttackerDoesNotStealDefenderMeleeLock) {
  auto* primary = world->create_entity();
  primary->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* primary_unit = primary->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  primary_unit->owner_id = 1;
  auto* primary_attack = primary->add_component<AttackComponent>();
  primary_attack->can_melee = true;
  primary_attack->can_ranged = false;
  primary_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto* defender = world->create_entity();
  defender->add_component<TransformComponent>(0.55F, 0.0F, 0.0F);
  auto* defender_unit = defender->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  defender_unit->owner_id = 2;
  auto* defender_attack = defender->add_component<AttackComponent>();
  defender_attack->can_melee = true;
  defender_attack->can_ranged = false;
  defender_attack->current_mode = AttackComponent::CombatMode::Melee;

  primary_attack->in_melee_lock = true;
  primary_attack->melee_lock_target_id = defender->get_id();
  defender_attack->in_melee_lock = true;
  defender_attack->melee_lock_target_id = primary->get_id();

  auto* second_attacker = world->create_entity();
  second_attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.55F);
  auto* second_unit =
      second_attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  second_unit->owner_id = 1;
  use_single_body_combat_geometry({primary, defender, second_attacker});
  auto* second_attack = second_attacker->add_component<AttackComponent>();
  second_attack->can_melee = true;
  second_attack->can_ranged = false;
  second_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  second_attack->current_mode = AttackComponent::CombatMode::Melee;
  second_attack->melee_cooldown = 0.0F;
  second_attack->time_since_last = 1.0F;
  auto* second_target = second_attacker->add_component<AttackTargetComponent>();
  second_target->target_id = defender->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_TRUE(second_attack->in_melee_lock);
  EXPECT_EQ(second_attack->melee_lock_target_id, defender->get_id());
  EXPECT_EQ(defender_attack->melee_lock_target_id, primary->get_id());
}

TEST_F(CombatModeTest, RepeatedMeleeAttacksUsePerPairCooldownStagger) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->melee_cooldown = 1.0F;
  attacker_attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.5F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  use_single_body_combat_geometry({attacker, enemy});

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_LT(attacker_attack->time_since_last, 0.0F);
  EXPECT_GT(attacker_attack->time_since_last, -0.29F);
}

TEST_F(CombatModeTest, SingleBodyMeleeLockedUnitsTurnToFaceWhileStopped) {
  auto* attacker = world->create_entity();
  auto* attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->render_individuals_per_unit_override = 1;
  attacker->add_component<MovementComponent>();
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->in_melee_lock = true;

  auto* enemy = world->create_entity();
  auto* enemy_transform = enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  enemy_unit->render_individuals_per_unit_override = 1;
  enemy->add_component<MovementComponent>();
  auto* enemy_attack = enemy->add_component<AttackComponent>();
  enemy_attack->can_melee = true;
  enemy_attack->can_ranged = false;
  enemy_attack->in_melee_lock = true;

  attacker_attack->melee_lock_target_id = enemy->get_id();
  enemy_attack->melee_lock_target_id = attacker->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_FALSE(attacker_transform->has_desired_yaw);
  EXPECT_FALSE(enemy_transform->has_desired_yaw);

  MovementSystem movement_system;
  movement_system.update(world.get(), 0.25F);

  EXPECT_FLOAT_EQ(attacker_transform->rotation.y, 90.0F);
  EXPECT_FLOAT_EQ(enemy_transform->rotation.y, -90.0F);
  EXPECT_FALSE(attacker_transform->has_desired_yaw);
  EXPECT_FALSE(enemy_transform->has_desired_yaw);
}

TEST_F(CombatModeTest, AttackAnimationFamilyFollowsUnitTypeAndMode) {
  auto* spearman = world->create_entity();
  spearman->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* spearman_unit = spearman->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  spearman_unit->owner_id = 1;
  spearman_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto* spearman_attack = spearman->add_component<AttackComponent>();
  spearman_attack->can_melee = true;
  spearman_attack->can_ranged = false;
  spearman_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  spearman_attack->current_mode = AttackComponent::CombatMode::Melee;
  spearman_attack->cooldown = 0.0F;
  spearman_attack->melee_cooldown = 0.0F;
  spearman_attack->time_since_last = 1.0F;

  auto* spearman_target = world->create_entity();
  spearman_target->add_component<TransformComponent>(0.5F, 0.0F, 0.0F);
  auto* spearman_target_unit =
      spearman_target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  spearman_target_unit->owner_id = 2;
  use_single_body_combat_geometry({spearman, spearman_target});

  auto* spearman_target_sel = spearman->add_component<AttackTargetComponent>();
  spearman_target_sel->target_id = spearman_target->get_id();
  spearman_target_sel->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* spearman_state = spearman->get_component<CombatStateComponent>();
  ASSERT_NE(spearman_state, nullptr);
  EXPECT_EQ(spearman_state->attack_family, CombatAttackFamily::Spear);

  auto* horse_archer = world->create_entity();
  horse_archer->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* horse_archer_unit =
      horse_archer->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  horse_archer_unit->owner_id = 1;
  horse_archer_unit->spawn_type = Game::Units::SpawnType::HorseArcher;
  auto* horse_archer_attack = horse_archer->add_component<AttackComponent>();
  horse_archer_attack->can_melee = true;
  horse_archer_attack->can_ranged = true;
  horse_archer_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  horse_archer_attack->current_mode = AttackComponent::CombatMode::Ranged;
  horse_archer_attack->range = 10.0F;
  horse_archer_attack->cooldown = 0.0F;
  horse_archer_attack->time_since_last = 1.0F;

  auto* horse_archer_target = world->create_entity();
  horse_archer_target->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* horse_archer_target_unit =
      horse_archer_target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  horse_archer_target_unit->owner_id = 2;

  auto* horse_archer_target_sel = horse_archer->add_component<AttackTargetComponent>();
  horse_archer_target_sel->target_id = horse_archer_target->get_id();
  horse_archer_target_sel->should_chase = false;

  auto const query_context_ranged =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context_ranged, 0.016F);

  auto* horse_archer_state = horse_archer->get_component<CombatStateComponent>();
  ASSERT_NE(horse_archer_state, nullptr);
  EXPECT_EQ(horse_archer_state->attack_family, CombatAttackFamily::Bow);
}

TEST_F(CombatModeTest, RangedVolleySpawnsOnlyAtAuthoredBowRelease) {
  struct RefreshTroopConfigOnExit {
    ~RefreshTroopConfigOnExit() {
      Game::Units::TroopConfig::instance().refresh_from_catalog();
    }
  } refresh_guard;

  world->add_system(std::make_unique<Game::Systems::ArrowSystem>());
  Game::Units::TroopConfig::instance().register_troop_type(
      Game::Units::TroopType::Archer, 60);

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = false;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 10.0F;
  attacker_attack->damage = 17;
  attacker_attack->cooldown = 1.0F;
  attacker_attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* arrow_system = world->get_system<Game::Systems::ArrowSystem>();
  ASSERT_NE(arrow_system, nullptr);
  EXPECT_TRUE(arrow_system->arrows().empty());
  EXPECT_EQ(enemy_unit->health, 100);
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::RtsBowShot));

  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.45F);
  EXPECT_TRUE(arrow_system->arrows().empty());
  EXPECT_EQ(enemy_unit->health, 100);

  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.02F);
  EXPECT_EQ(arrow_system->arrows().size(),
            static_cast<std::size_t>(
                Game::Systems::Combat::Constants::k_max_visual_arrows_per_volley));
  EXPECT_EQ(enemy_unit->health, 83);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
}

TEST_F(CombatModeTest, InterruptedBowActionCreatesNoProjectileOrDamage) {
  world->add_system(std::make_unique<Game::Systems::ArrowSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = false;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 10.0F;
  attacker_attack->damage = 17;
  attacker_attack->cooldown = 1.0F;
  attacker_attack->time_since_last = 1.0F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  attacker->add_component<StaggerComponent>();
  Game::Systems::Combat::process_authored_combat_action(
      world.get(), *attacker, attacker->get_component<CombatStateComponent>(), 0.5F);

  auto* arrow_system = world->get_system<Game::Systems::ArrowSystem>();
  ASSERT_NE(arrow_system, nullptr);
  EXPECT_TRUE(arrow_system->arrows().empty());
  EXPECT_EQ(enemy_unit->health, 100);
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  ASSERT_NE(action, nullptr);
  EXPECT_TRUE(action->action_completed);
}

TEST_F(CombatModeTest, MultipleArchersReleaseFromTheirOwnVisibleActions) {
  world->add_system(std::make_unique<Game::Systems::ArrowSystem>());

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  std::array<Entity*, 3> archers{};
  for (std::size_t i = 0; i < archers.size(); ++i) {
    auto* archer = world->create_entity();
    archer->add_component<TransformComponent>(0.0F, 0.0F, static_cast<float>(i));
    auto* unit = archer->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Archer;
    auto* attack = archer->add_component<AttackComponent>();
    attack->can_melee = false;
    attack->can_ranged = true;
    attack->preferred_mode = AttackComponent::CombatMode::Ranged;
    attack->current_mode = AttackComponent::CombatMode::Ranged;
    attack->range = 10.0F;
    attack->damage = 10;
    attack->cooldown = 1.0F;
    attack->time_since_last = 1.0F;
    auto* target = archer->add_component<AttackTargetComponent>();
    target->target_id = enemy->get_id();
    archers[i] = archer;
  }

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  auto* arrow_system = world->get_system<Game::Systems::ArrowSystem>();
  ASSERT_NE(arrow_system, nullptr);
  EXPECT_TRUE(arrow_system->arrows().empty());

  for (auto* archer : archers) {
    auto* action = archer->get_component<RpgCommanderActionComponent>();
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->combat_action_id,
              static_cast<std::uint8_t>(
                  Game::Systems::CombatActions::CombatActionId::RtsBowShot));
    Game::Systems::Combat::process_authored_combat_action(
        world.get(), *archer, archer->get_component<CombatStateComponent>(), 0.47F);
    EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  }

  EXPECT_FALSE(arrow_system->arrows().empty());
  EXPECT_EQ(enemy_unit->health, 70);
}

TEST_F(CombatModeTest, HitPauseUsesPerUnitCombatAnimationConstant) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  auto* combat_state = target->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::WindUp;
  combat_state->state_duration = 1.0F;
  combat_state->state_time = 0.25F;

  Game::Systems::Combat::deal_damage(world.get(), target, 5, attacker->get_id());

  EXPECT_TRUE(combat_state->is_hit_paused);
  EXPECT_FLOAT_EQ(combat_state->hit_pause_remaining,
                  CombatStateComponent::k_combat_animation_hit_pause_duration);

  Game::Systems::Combat::process_combat_state(
      world.get(), CombatStateComponent::k_combat_animation_hit_pause_duration * 0.5F);
  EXPECT_TRUE(combat_state->is_hit_paused);
  EXPECT_FLOAT_EQ(combat_state->state_time, 0.25F);

  Game::Systems::Combat::process_combat_state(
      world.get(), CombatStateComponent::k_combat_animation_hit_pause_duration * 0.5F);
  EXPECT_FALSE(combat_state->is_hit_paused);
  EXPECT_FLOAT_EQ(combat_state->hit_pause_remaining, 0.0F);

  Game::Systems::Combat::process_combat_state(world.get(), 0.1F);
  EXPECT_GT(combat_state->state_time, 0.25F);
}

TEST_F(CombatModeTest, LethalDamageStartsDeathSequenceBeforeCleanup) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Knight;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  target->add_component<RenderableComponent>("mesh", "tex");
  auto* movement = target->add_component<MovementComponent>();
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_vx(*movement, 1.0F);
  MovementTestAccess::set_vz(*movement, 1.0F);
  auto* target_unit = target->add_component<UnitComponent>(30, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;

  Game::Systems::Combat::deal_damage(world.get(), target, 120, attacker->get_id());

  EXPECT_EQ(target_unit->health, 0);
  auto* death = target->get_component<DeathAnimationComponent>();
  ASSERT_NE(death, nullptr);
  EXPECT_EQ(death->state, DeathSequenceState::Dying);
  EXPECT_EQ(death->profile, DeathSequenceProfile::Infantry);
  EXPECT_EQ(death->sequence_variant, 0U);
  EXPECT_FALSE(target->has_component<PendingRemovalComponent>());
  EXPECT_FALSE(movement->get_has_target());

  auto blood_stains = world->get_entities_with<BloodStainComponent>();
  ASSERT_EQ(blood_stains.size(), 1U);
  auto* blood_transform = blood_stains.front()->get_component<TransformComponent>();
  auto* blood_stain = blood_stains.front()->get_component<BloodStainComponent>();
  ASSERT_NE(blood_transform, nullptr);
  ASSERT_NE(blood_stain, nullptr);
  EXPECT_FLOAT_EQ(blood_transform->position.x, 1.0F);
  EXPECT_FLOAT_EQ(blood_transform->position.z, 0.0F);
  EXPECT_GT(blood_stain->radius,
            Engine::Core::Defaults::k_blood_stain_default_radius * 0.8F);
  EXPECT_LT(blood_stain->radius,
            Engine::Core::Defaults::k_blood_stain_default_radius * 1.4F);
  EXPECT_GE(blood_stain->rotation, 0.0F);
  EXPECT_LT(blood_stain->rotation, std::numbers::pi_v<float> * 2.0F);
  EXPECT_GE(blood_stain->aspect_ratio, 0.72F);
  EXPECT_LE(blood_stain->aspect_ratio, 1.34F);
  EXPECT_GE(blood_stain->seed, 0.0F);
  EXPECT_LT(blood_stain->seed, 1.0F);
}

TEST_F(CombatModeTest, NonLethalDamageQueuesPerSoldierCasualtyAnimations) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Knight;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  target_unit->spawn_type = Game::Units::SpawnType::Archer;
  target_unit->render_individuals_per_unit_override = 3;

  Game::Systems::Combat::deal_damage(world.get(), target, 40, attacker->get_id());

  auto* casualties = target->get_component<SoldierCasualtyAnimationComponent>();
  ASSERT_NE(casualties, nullptr);
  ASSERT_EQ(casualties->entries.size(), 1U);
  EXPECT_EQ(casualties->entries.front().slot_index, 0U);
  EXPECT_EQ(casualties->entries.front().state, DeathSequenceState::Dying);
  EXPECT_FALSE(target->has_component<DeathAnimationComponent>());

  auto blood_stains = world->get_entities_with<BloodStainComponent>();
  ASSERT_EQ(blood_stains.size(), 1U);
  auto* blood_transform = blood_stains.front()->get_component<TransformComponent>();
  ASSERT_NE(blood_transform, nullptr);
  EXPECT_FLOAT_EQ(blood_transform->position.x, 1.0F);
  EXPECT_FLOAT_EQ(blood_transform->position.z, 0.0F);
}

TEST_F(CombatModeTest, MountedAndElephantVictimsSelectDeathProfiles) {
  auto* mounted = world->create_entity();
  mounted->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* mounted_unit = mounted->add_component<UnitComponent>(120, 120, 1.0F, 12.0F);
  mounted_unit->owner_id = 1;
  mounted_unit->spawn_type = Game::Units::SpawnType::HorseArcher;

  auto* mounted_target = world->create_entity();
  mounted_target->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto* mounted_target_unit =
      mounted_target->add_component<UnitComponent>(40, 100, 1.0F, 12.0F);
  mounted_target_unit->owner_id = 2;
  mounted_target_unit->spawn_type = Game::Units::SpawnType::MountedKnight;

  Game::Systems::Combat::deal_damage(
      world.get(), mounted_target, 80, mounted->get_id());
  auto* mounted_death = mounted_target->get_component<DeathAnimationComponent>();
  ASSERT_NE(mounted_death, nullptr);
  EXPECT_EQ(mounted_death->profile, DeathSequenceProfile::MountedRider);
  EXPECT_EQ(mounted_death->sequence_variant, 0U);

  auto* elephant = world->create_entity();
  elephant->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* elephant_unit = elephant->add_component<UnitComponent>(300, 300, 1.0F, 12.0F);
  elephant_unit->owner_id = 1;
  elephant_unit->spawn_type = Game::Units::SpawnType::Elephant;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(40, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  target_unit->spawn_type = Game::Units::SpawnType::Elephant;

  Game::Systems::Combat::deal_damage(world.get(), target, 80, elephant->get_id());

  auto* death = target->get_component<DeathAnimationComponent>();
  ASSERT_NE(death, nullptr);
  EXPECT_EQ(death->profile, DeathSequenceProfile::Elephant);
  EXPECT_EQ(death->sequence_variant, 0U);
  EXPECT_GE(death->state_duration, 1.2F);
}

TEST_F(CombatModeTest, PerfectGuardNegatesDamageAndDoesNotConsumeCommanderComboState) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_cmd = attacker->add_component<CommanderComponent>();
  attacker_cmd->combo_step = 2;
  attacker_cmd->power_strike_active = true;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  auto* target_cmd = target->add_component<CommanderComponent>();
  auto* guard = target->add_component<CommanderGuardComponent>();
  guard->active = true;
  guard->perfect_guard_remaining = 0.1F;

  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;

  Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
      world.get(), target, 40, attacker->get_id());

  EXPECT_EQ(target_unit->health, 100);
  EXPECT_EQ(target_rpg->rpg_hp, 100);
  EXPECT_TRUE(attacker->has_component<StaggerComponent>());
  EXPECT_EQ(attacker_cmd->combo_step, 2);
  EXPECT_TRUE(attacker_cmd->power_strike_active);
  EXPECT_FALSE(attacker_cmd->just_struck_enemy);
  EXPECT_GT(target_cmd->punish_window_remaining, 0.0F);
}

TEST_F(CombatModeTest, RtsDamageDoesNotReadCommanderGuardOrComboState) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_cmd = attacker->add_component<CommanderComponent>();
  attacker_cmd->combo_step = 3;
  attacker_cmd->power_strike_active = true;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  auto* target_cmd = target->add_component<CommanderComponent>();
  target_cmd->posture = 95.0F;
  auto* guard = target->add_component<CommanderGuardComponent>();
  guard->active = true;
  guard->perfect_guard_remaining = 0.1F;

  Game::Systems::Combat::deal_damage(world.get(), target, 20, attacker->get_id());

  EXPECT_EQ(target_unit->health, 80);
  EXPECT_TRUE(guard->active);
  EXPECT_FLOAT_EQ(guard->perfect_guard_remaining, 0.1F);
  EXPECT_FLOAT_EQ(target_cmd->posture, 95.0F);
  EXPECT_EQ(attacker_cmd->combo_step, 3);
  EXPECT_TRUE(attacker_cmd->power_strike_active);
  EXPECT_FALSE(attacker_cmd->just_struck_enemy);
}

TEST_F(CombatModeTest, RpgCommanderAttackUsesCommanderComboOnFormationDamage) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto* attacker_cmd = attacker->add_component<CommanderComponent>();
  attacker_cmd->combo_step = 3;
  attacker_cmd->power_strike_active = true;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;

  Game::Systems::RpgCombat::deal_commander_attack_damage(
      world.get(), target, 20, attacker->get_id());

  EXPECT_EQ(target_unit->health, 46);
  EXPECT_EQ(attacker_cmd->combo_step, 0);
  EXPECT_FALSE(attacker_cmd->power_strike_active);
  EXPECT_TRUE(attacker_cmd->just_struck_enemy);
}

TEST_F(CombatModeTest, GuardBreakDropsGuardAndLetsTheBreakingHitThrough) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto* target = world->create_entity();
  target->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* target_unit = target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  auto* target_cmd = target->add_component<CommanderComponent>();
  target_cmd->posture = 95.0F;
  target_cmd->posture_max = 100.0F;
  auto* guard = target->add_component<CommanderGuardComponent>();
  guard->active = true;

  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
      world.get(), target, 20, attacker->get_id());

  EXPECT_EQ(target_unit->health, 100);
  EXPECT_EQ(target_rpg->rpg_hp, 80);
  EXPECT_FALSE(guard->active);
  EXPECT_TRUE(guard->rearm_requires_release);
  EXPECT_GT(guard->guard_break_remaining, 0.0F);
  EXPECT_GT(target_cmd->punish_window_remaining, 0.0F);
}

TEST_F(CombatModeTest, CleanupSystemRunsDeathThenDeadHoldBeforeRemoval) {
  auto* target = world->create_entity();
  auto target_id = target->get_id();
  target->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  target->add_component<RenderableComponent>("mesh", "tex");
  target->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
  auto* death = target->add_component<DeathAnimationComponent>();
  death->profile = DeathSequenceProfile::Infantry;
  death->state = DeathSequenceState::Dying;
  death->state_duration = 0.1F;
  death->dead_hold_duration = 0.1F;
  death->state_time = 0.0F;

  CleanupSystem cleanup;
  cleanup.update(world.get(), 0.02F);
  EXPECT_EQ(death->state, DeathSequenceState::Dying);
  EXPECT_NE(world->get_entity(target_id), nullptr);

  for (int i = 0; i < 6; ++i) {
    cleanup.update(world.get(), 0.02F);
  }
  auto* entity_mid = world->get_entity(target_id);
  ASSERT_NE(entity_mid, nullptr);
  auto* death_mid = entity_mid->get_component<DeathAnimationComponent>();
  ASSERT_NE(death_mid, nullptr);
  EXPECT_EQ(death_mid->state, DeathSequenceState::DeadHold);

  for (int i = 0; i < 6; ++i) {
    cleanup.update(world.get(), 0.02F);
  }
  EXPECT_EQ(world->get_entity(target_id), nullptr);
}

TEST_F(CombatModeTest, CleanupSystemExpiresBloodStainsAfterLifetime) {
  auto* blood_stain_entity = world->create_entity();
  auto blood_stain_id = blood_stain_entity->get_id();
  blood_stain_entity->add_component<TransformComponent>(2.0F, 0.0F, 3.0F);
  auto* blood_stain = blood_stain_entity->add_component<BloodStainComponent>();
  blood_stain->lifetime = 0.1F;

  CleanupSystem cleanup;
  cleanup.update(world.get(), 0.05F);
  EXPECT_NE(world->get_entity(blood_stain_id), nullptr);

  cleanup.update(world.get(), 0.05F);
  EXPECT_EQ(world->get_entity(blood_stain_id), nullptr);
}

TEST_F(CombatModeTest, GravePriestHealingTargetsUndeadAndLivingHealersSkipThem) {
  HealingSystem healing_system;

  auto* grave_priest = world->create_entity();
  grave_priest->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* grave_priest_unit =
      grave_priest->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  grave_priest_unit->owner_id = 1;
  auto* grave_priest_healer = grave_priest->add_component<HealerComponent>();
  grave_priest_healer->healing_range = 8.0F;
  grave_priest_healer->healing_amount = 12;
  grave_priest_healer->healing_cooldown = 1.0F;
  grave_priest_healer->time_since_last_heal = 1.0F;
  grave_priest_healer->target_affinity = HealerComponent::TargetAffinity::UndeadAllies;

  auto* living_healer = world->create_entity();
  living_healer->add_component<TransformComponent>(0.0F, 0.0F, 1.0F);
  auto* living_healer_unit =
      living_healer->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  living_healer_unit->owner_id = 1;
  auto* living_healer_comp = living_healer->add_component<HealerComponent>();
  living_healer_comp->healing_range = 8.0F;
  living_healer_comp->healing_amount = 9;
  living_healer_comp->healing_cooldown = 1.0F;
  living_healer_comp->time_since_last_heal = 1.0F;
  living_healer_comp->target_affinity = HealerComponent::TargetAffinity::LivingAllies;

  auto* undead_ally = world->create_entity();
  undead_ally->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* undead_unit = undead_ally->add_component<UnitComponent>(50, 80, 1.0F, 12.0F);
  undead_unit->owner_id = 1;
  undead_ally->add_component<UndeadComponent>();

  auto* living_ally = world->create_entity();
  living_ally->add_component<TransformComponent>(2.0F, 0.0F, 1.0F);
  auto* living_unit = living_ally->add_component<UnitComponent>(45, 90, 1.0F, 12.0F);
  living_unit->owner_id = 1;

  healing_system.update(world.get(), 0.1F);

  EXPECT_EQ(undead_unit->health, 62);
  EXPECT_EQ(living_unit->health, 54);
}

TEST_F(CombatModeTest, GravePriestSuppressesFireballWhileUndeadHealingIsAvailable) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::GravePriest;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 24;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  attacker->add_component<SpecialAttackComponent>()->projectile_kind =
      Game::Systems::ProjectileKind::Fireball;
  auto* special_attack = attacker->get_component<SpecialAttackComponent>();
  special_attack->use_projectile_system = true;
  special_attack->splash_radius = 2.4F;
  auto* healer = attacker->add_component<HealerComponent>();
  healer->healing_range = 8.0F;
  healer->healing_amount = 10;
  healer->healing_cooldown = 0.5F;
  healer->time_since_last_heal = 1.0F;
  healer->target_affinity = HealerComponent::TargetAffinity::UndeadAllies;
  healer->suppress_attack_while_healing = true;

  auto* undead_ally = world->create_entity();
  undead_ally->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto* undead_unit = undead_ally->add_component<UnitComponent>(60, 80, 1.0F, 12.0F);
  undead_unit->owner_id = 1;
  undead_ally->add_component<UndeadComponent>();

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* target = attacker->add_component<AttackTargetComponent>();
  target->target_id = enemy->get_id();
  target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* projectile_system = world->get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);
  EXPECT_TRUE(projectile_system->projectiles().empty());
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(CombatModeTest,
       FireballProjectileDamagesEnemiesInAreaWithoutHittingAlliedUndead) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::GravePriest;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 24;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->splash_radius = 2.5F;
  special_attack->splash_damage_multiplier = 0.5F;
  special_attack->fire_patch_duration = 2.0F;
  special_attack->fire_patch_radius = 2.1F;
  special_attack->burn_duration = 1.2F;
  special_attack->burn_tick_interval = 0.3F;
  special_attack->burn_damage_per_tick = 3;

  auto* primary_enemy = world->create_entity();
  primary_enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* primary_enemy_unit =
      primary_enemy->add_component<UnitComponent>(40, 40, 1.0F, 12.0F);
  primary_enemy_unit->owner_id = 2;

  auto* splash_enemy = world->create_entity();
  splash_enemy->add_component<TransformComponent>(7.5F, 0.0F, 0.2F);
  auto* splash_enemy_unit =
      splash_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  splash_enemy_unit->owner_id = 2;

  auto* allied_undead = world->create_entity();
  allied_undead->add_component<TransformComponent>(6.5F, 0.0F, 0.1F);
  auto* allied_undead_unit =
      allied_undead->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  allied_undead_unit->owner_id = 1;
  allied_undead->add_component<UndeadComponent>();

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = primary_enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* projectile_system = world->get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);
  ASSERT_EQ(projectile_system->projectiles().size(), 1U);
  EXPECT_EQ(primary_enemy_unit->health, 40);

  advance_projectiles(*world);

  EXPECT_LT(primary_enemy_unit->health, 40);
  EXPECT_LT(splash_enemy_unit->health, 100);
  EXPECT_EQ(allied_undead_unit->health, 100);
  EXPECT_EQ(world->get_entities_with<FirePatchComponent>().size(), 1U);
  EXPECT_NE(primary_enemy->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_NE(splash_enemy->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_EQ(allied_undead->get_component<BurningStatusComponent>(), nullptr);

  int const primary_after_impact = primary_enemy_unit->health;
  int const splash_after_impact = splash_enemy_unit->health;
  advance_projectiles(*world, 0.6F, 0.05F);
  EXPECT_LT(primary_enemy_unit->health, primary_after_impact);
  EXPECT_LT(splash_enemy_unit->health, splash_after_impact);
  EXPECT_EQ(allied_undead_unit->health, 100);
}

TEST_F(CombatModeTest, FireballRespectsAttackCooldownBeforeLaunchingProjectile) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::GravePriest;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 24;
  attacker_attack->cooldown = 2.0F;
  attacker_attack->time_since_last = 0.2F;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->splash_radius = 2.5F;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto* projectile_system = world->get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);
  EXPECT_TRUE(projectile_system->projectiles().empty());
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(CombatModeTest, FireballBurnContinuesAfterCasterDies) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::GravePriest;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 24;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->splash_radius = 2.0F;
  special_attack->splash_damage_multiplier = 0.6F;
  special_attack->fire_patch_duration = 1.8F;
  special_attack->fire_patch_radius = 1.8F;
  special_attack->burn_duration = 1.4F;
  special_attack->burn_tick_interval = 0.25F;
  special_attack->burn_damage_per_tick = 2;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  advance_projectiles(*world, 0.8F, 0.08F);

  ASSERT_NE(enemy->get_component<BurningStatusComponent>(), nullptr);
  int const health_after_impact = enemy_unit->health;

  world->destroy_entity(attacker->get_id());
  advance_projectiles(*world, 0.6F, 0.05F);

  EXPECT_LT(enemy_unit->health, health_after_impact);
}

TEST_F(CombatModeTest, FireballBurnKeepsIgnitionAgeWhileFirePatchRefreshesDuration) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::GravePriest;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 24;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->splash_radius = 2.0F;
  special_attack->splash_damage_multiplier = 0.6F;
  special_attack->fire_patch_duration = 1.8F;
  special_attack->fire_patch_radius = 1.8F;
  special_attack->burn_duration = 1.4F;
  special_attack->burn_tick_interval = 0.25F;
  special_attack->burn_damage_per_tick = 2;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  advance_projectiles(*world, 0.8F, 0.08F);

  auto* burning = enemy->get_component<BurningStatusComponent>();
  ASSERT_NE(burning, nullptr);
  float const initial_ignition_elapsed = burning->ignition_elapsed;
  float const refreshed_duration = burning->duration;

  advance_projectiles(*world, 0.4F, 0.05F);

  burning = enemy->get_component<BurningStatusComponent>();
  ASSERT_NE(burning, nullptr);
  EXPECT_GT(burning->ignition_elapsed, initial_ignition_elapsed);
  EXPECT_FLOAT_EQ(burning->duration, refreshed_duration);
  EXPECT_FLOAT_EQ(burning->remaining_duration, burning->duration);
}

TEST_F(CombatModeTest, CursedVolleyAppliesAndRefreshesCurseOnLivingTargets) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::SkeletonArcher;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 14;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::CursedArrow;
  special_attack->use_projectile_system = true;
  special_attack->cursed_duration = 6.0F;
  special_attack->cursed_morale_penalty_per_hit = 8.0F;
  special_attack->cursed_stacks_per_hit = 1;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  auto* morale = enemy->add_component<MoraleComponent>();
  morale->morale = 70.0F;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto query_context = Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  advance_projectiles(*world, 0.8F, 0.08F);

  auto* cursed = enemy->get_component<CursedStatusComponent>();
  ASSERT_NE(cursed, nullptr);
  EXPECT_EQ(cursed->stacks, 1);
  EXPECT_FLOAT_EQ(morale->morale, 62.0F);

  cursed->remaining_duration = 1.5F;
  attacker_attack->time_since_last = 1.0F;
  query_context = Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  advance_projectiles(*world, 0.8F, 0.08F);

  ASSERT_NE(enemy->get_component<CursedStatusComponent>(), nullptr);
  EXPECT_EQ(enemy->get_component<CursedStatusComponent>()->stacks, 2);
  EXPECT_GT(enemy->get_component<CursedStatusComponent>()->remaining_duration, 5.0F);
  EXPECT_FLOAT_EQ(morale->morale, 54.0F);
}

TEST_F(CombatModeTest, CursedVolleyDoesNotAffectUndeadTargets) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 14.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::SkeletonArcher;
  auto* attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 8.0F;
  attacker_attack->damage = 14;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::CursedArrow;
  special_attack->use_projectile_system = true;
  special_attack->cursed_duration = 6.0F;
  special_attack->cursed_morale_penalty_per_hit = 8.0F;
  special_attack->cursed_stacks_per_hit = 1;

  auto* enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto* enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  enemy->add_component<UndeadComponent>();
  auto* morale = enemy->add_component<MoraleComponent>();
  morale->morale = 70.0F;

  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);
  advance_projectiles(*world, 0.8F, 0.08F);

  EXPECT_EQ(enemy_unit->health, 86);
  EXPECT_EQ(morale->morale, 70.0F);
  EXPECT_EQ(enemy->get_component<CursedStatusComponent>(), nullptr);
}

TEST_F(CombatModeTest, BuildingDeathsDoNotCreateBloodStains) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto* building = world->create_entity();
  building->add_component<TransformComponent>(4.0F, 0.0F, 1.0F);
  building->add_component<RenderableComponent>("mesh", "tex");
  auto* building_unit = building->add_component<UnitComponent>(40, 40, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  Game::Systems::Combat::deal_damage(world.get(), building, 40, attacker->get_id());

  EXPECT_TRUE(world->get_entities_with<BloodStainComponent>().empty());
}

TEST_F(CombatModeTest, BloodStainsAreCappedAtTenActiveEntries) {
  auto* attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* attacker_unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  EntityID first_stain_id = 0;
  for (int index = 0; index < 11; ++index) {
    auto* target = world->create_entity();
    target->add_component<TransformComponent>(
        static_cast<float>(index), 0.0F, static_cast<float>(index));
    auto* target_unit = target->add_component<UnitComponent>(10, 10, 1.0F, 12.0F);
    target_unit->owner_id = 2;

    Game::Systems::Combat::deal_damage(world.get(), target, 10, attacker->get_id());

    if (index == 0) {
      auto blood_stains = world->get_entities_with<BloodStainComponent>();
      ASSERT_EQ(blood_stains.size(), 1U);
      first_stain_id = blood_stains.front()->get_id();
    }
  }

  auto blood_stains = world->get_entities_with<BloodStainComponent>();
  ASSERT_EQ(blood_stains.size(), 10U);
  EXPECT_EQ(world->get_entity(first_stain_id), nullptr);
}

namespace {

auto make_fpv_commander(World& world, float x, float z) -> Entity* {
  auto* commander = world.create_entity();
  commander->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = commander->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  unit->render_individuals_per_unit_override = 1;
  auto* commander_data = commander->add_component<CommanderComponent>();
  commander_data->fpv_controlled = true;
  auto* attack = commander->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 1.8F;
  attack->melee_damage = 10;
  return commander;
}

auto make_fpv_spear_commander(World& world, float x, float z) -> Entity* {
  auto* commander = make_fpv_commander(world, x, z);
  auto* unit = commander->get_component<UnitComponent>();
  auto* attack = commander->get_component<AttackComponent>();
  if (unit != nullptr) {
    unit->spawn_type = Game::Units::SpawnType::Spearman;
  }
  if (attack != nullptr) {
    attack->melee_range = 2.6F;
    attack->melee_damage = 10;
  }
  return commander;
}

auto make_fpv_bow_commander(World& world, float x, float z) -> Entity* {
  auto* commander = make_fpv_commander(world, x, z);
  auto* unit = commander->get_component<UnitComponent>();
  auto* attack = commander->get_component<AttackComponent>();
  if (unit != nullptr) {
    unit->spawn_type = Game::Units::SpawnType::Archer;
  }
  if (attack != nullptr) {
    attack->can_ranged = true;
    attack->preferred_mode = AttackComponent::CombatMode::Ranged;
    attack->current_mode = AttackComponent::CombatMode::Ranged;
    attack->range = 8.0F;
    attack->damage = 13;
  }
  return commander;
}

auto begin_commander_strike(Entity* commander,
                            EntityID locked_target_id) -> CombatStateComponent* {
  auto* combat_state = commander->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::Strike;
  combat_state->state_time = 0.0F;
  combat_state->state_duration = 0.01F;
  combat_state->damage_dealt_this_swing = false;
  auto* action = commander->add_component<RpgCommanderActionComponent>();
  action->active_target_id = locked_target_id;
  return combat_state;
}

auto start_authored_action_at(Entity* entity,
                              Game::Systems::CombatActions::CombatActionId id,
                              float normalized_time) -> RpgCommanderActionComponent* {
  auto* action = entity->get_component<RpgCommanderActionComponent>();
  if (action == nullptr) {
    action = entity->add_component<RpgCommanderActionComponent>();
  }
  auto const* definition =
      Game::Systems::CombatActions::find_combat_action_definition(id);
  if (action == nullptr || definition == nullptr) {
    return action;
  }
  action->phase = RpgCommanderActionPhase::Strike;
  action->combat_action_id = static_cast<std::uint8_t>(id);
  action->action_duration = definition->duration_seconds;
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
  (void)Game::Systems::CombatActions::advance_combat_action_events(
      *action,
      std::clamp(normalized_time, 0.0F, 1.0F) * definition->duration_seconds,
      *definition);
  return action;
}

auto make_enemy_soldier(World& world, float x, float z) -> Entity* {
  auto* enemy = world.create_entity();
  enemy->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->render_individuals_per_unit_override = 1;
  unit->owner_id = 2;
  return enemy;
}

auto make_enemy_cavalry(World& world, float x, float z) -> Entity* {
  auto* enemy = make_enemy_soldier(world, x, z);
  auto* unit = enemy != nullptr ? enemy->get_component<UnitComponent>() : nullptr;
  if (unit != nullptr) {
    unit->spawn_type = Game::Units::SpawnType::MountedKnight;
    unit->speed = 7.0F;
  }
  return enemy;
}

auto make_mounted_attacker(World& world, float x, float z) -> Entity* {
  auto* attacker = world.create_entity();
  attacker->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->speed = 7.0F;
  auto* attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->current_mode = AttackComponent::CombatMode::Melee;
  attack->melee_damage = 18;
  return attacker;
}

auto make_fpv_mounted_sword_commander(World& world, float x, float z) -> Entity* {
  auto* attacker = make_mounted_attacker(world, x, z);
  if (attacker == nullptr) {
    return nullptr;
  }
  auto* commander = attacker->add_component<CommanderComponent>();
  if (commander != nullptr) {
    commander->fpv_controlled = true;
  }
  auto* attack = attacker->get_component<AttackComponent>();
  if (attack != nullptr) {
    attack->melee_range = 2.4F;
    attack->melee_damage = 18;
  }
  return attacker;
}

auto make_fpv_mounted_spear_commander(World& world, float x, float z) -> Entity* {
  auto* attacker = make_fpv_mounted_sword_commander(world, x, z);
  if (attacker == nullptr) {
    return nullptr;
  }
  auto* unit = attacker->get_component<UnitComponent>();
  if (unit != nullptr) {
    unit->spawn_type = Game::Units::SpawnType::HorseSpearman;
  }
  auto* attack = attacker->get_component<AttackComponent>();
  if (attack != nullptr) {
    attack->melee_range = 3.2F;
    attack->melee_damage = 20;
  }
  return attacker;
}

} // namespace

TEST_F(CombatModeTest, CommanderStrikeDealsDamageAtImpactContact) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, 1.5F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());

  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 90);
  EXPECT_TRUE(combat_state->damage_dealt_this_swing);
}

TEST_F(CombatModeTest, CommanderStrikeAdvancesAuthoredActionEvents) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* combat_state = begin_commander_strike(commander, 0U);
  combat_state->state_duration = 0.30F;

  auto* action = start_authored_action_at(
      commander,
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft,
      0.36F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);

  EXPECT_TRUE(action->action_active);
  EXPECT_TRUE(action->weapon_trace_active);
  EXPECT_TRUE(action->last_event_valid);
  EXPECT_EQ(action->last_event_type,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionEventType::WeaponTraceStart));

  Game::Systems::Combat::process_combat_state(world.get(), 0.55F);

  EXPECT_FALSE(action->action_active);
  EXPECT_FALSE(action->weapon_trace_active);
  EXPECT_TRUE(action->last_event_valid);
  EXPECT_EQ(action->last_event_type,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionEventType::WeaponTraceEnd));
}

TEST_F(CombatModeTest, BowActiveWindowDoesNotEnableWeaponTrace) {
  auto* commander = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* combat_state = begin_commander_strike(commander, 0U);
  combat_state->state_duration = 0.30F;

  auto* action = start_authored_action_at(
      commander, Game::Systems::CombatActions::CombatActionId::RpgBowShot, 0.295F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.02F);

  EXPECT_TRUE(action->action_active);
  EXPECT_FALSE(action->weapon_trace_active);
  EXPECT_TRUE(action->last_event_valid);
  EXPECT_EQ(action->last_event_type,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionEventType::ActiveStart));
}

TEST_F(CombatModeTest, AuthoredActionTimelineDoesNotReadCompatibilityPhase) {
  RpgCommanderActionComponent action;
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordThrust);
  ASSERT_NE(definition, nullptr);
  action.combat_action_id = static_cast<std::uint8_t>(definition->id);
  action.action_duration = definition->duration_seconds;
  Game::Systems::CombatActions::reset_combat_action_event_runtime(action);

  CombatStateComponent compatibility_state;
  compatibility_state.animation_state = CombatAnimationState::Idle;
  compatibility_state.state_time = 999.0F;

  auto const events = Game::Systems::CombatActions::advance_combat_action_events(
      action, definition->duration_seconds * 0.40F, *definition);

  EXPECT_NEAR(action.normalized_action_time, 0.40F, 0.0001F);
  EXPECT_TRUE(action.weapon_trace_active);
  EXPECT_FALSE(events.empty());
  EXPECT_EQ(compatibility_state.animation_state, CombatAnimationState::Idle);
}

TEST_F(CombatModeTest, SwordTraceRejectsTargetsBehindCommander) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, -1.2F);
  ASSERT_NE(enemy, nullptr);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  ASSERT_NE(definition, nullptr);

  auto const contact = Game::Systems::CombatActions::find_sword_trace_contact(
      *world, *commander, *definition, enemy->get_id());

  EXPECT_EQ(contact.target_id, 0U) << "local_forward=" << contact.local_forward
                                   << " local_right=" << contact.local_right;
}

TEST_F(CombatModeTest, SpearTraceUsesLongNarrowThrustShape) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* forward_enemy = make_enemy_soldier(*world, 0.0F, 2.45F);
  auto* side_enemy = make_enemy_soldier(*world, 2.0F, 2.0F);
  ASSERT_NE(forward_enemy, nullptr);
  ASSERT_NE(side_enemy, nullptr);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSpearThrust);
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->weapon_family,
            Game::Systems::CombatActions::WeaponFamily::Spear);
  EXPECT_EQ(definition->attack_direction, AttackDirection::Thrust);
  EXPECT_FLOAT_EQ(definition->hit_shape.radius, 0.16F);

  auto const forward_contact = Game::Systems::CombatActions::find_weapon_trace_contact(
      *world, *commander, *definition, forward_enemy->get_id());
  EXPECT_EQ(forward_contact.target_id, forward_enemy->get_id());

  std::array<EntityID, 1> const ignored_forward{forward_enemy->get_id()};
  auto const side_contact = Game::Systems::CombatActions::find_weapon_trace_contact(
      *world, *commander, *definition, side_enemy->get_id(), ignored_forward);
  EXPECT_EQ(side_contact.target_id, 0U)
      << "local_forward=" << side_contact.local_forward
      << " local_right=" << side_contact.local_right;
}

TEST_F(CombatModeTest, TimedWeaponTraceHasNoSegmentOutsideAuthoredWindow) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  ASSERT_NE(definition, nullptr);

  auto const segment =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *commander,
          *definition,
          {.previous_normalized_time = 0.12F, .current_normalized_time = 0.24F});

  EXPECT_FALSE(segment.valid);
}

TEST_F(CombatModeTest, TimedSwordTraceUsesBakedSocketSegmentWhenAvailable) {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::k_species_humanoid_sword);
  if (blob == nullptr) {
    GTEST_SKIP() << "baked humanoid sword BPAT asset not loaded";
  }

  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  ASSERT_NE(definition, nullptr);

  auto const segment =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *commander,
          *definition,
          {.previous_normalized_time = 0.44F, .current_normalized_time = 0.52F});

  ASSERT_TRUE(segment.valid);
  EXPECT_EQ(segment.source,
            Game::Systems::CombatActions::WeaponTraceSegmentSource::BakedSocket);
  EXPECT_GT((segment.current_base - segment.previous_base).length(), 0.005F);
  EXPECT_NEAR((segment.current_tip - segment.current_base).length(), 0.84F, 0.08F);
}

TEST_F(CombatModeTest, TimedSpearTraceUsesBakedShaftSegmentWhenAvailable) {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::k_species_humanoid_spear);
  if (blob == nullptr) {
    GTEST_SKIP() << "baked humanoid spear BPAT asset not loaded";
  }

  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* forward_enemy = make_enemy_soldier(*world, -0.30F, 2.45F);
  auto* broad_shape_enemy = make_enemy_soldier(*world, 0.50F, 2.45F);
  ASSERT_NE(forward_enemy, nullptr);
  ASSERT_NE(broad_shape_enemy, nullptr);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSpearThrust);
  ASSERT_NE(definition, nullptr);

  auto const segment =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *commander,
          *definition,
          {.previous_normalized_time = 0.44F, .current_normalized_time = 0.52F});
  ASSERT_TRUE(segment.valid);
  EXPECT_EQ(segment.source,
            Game::Systems::CombatActions::WeaponTraceSegmentSource::BakedSocket);
  EXPECT_NEAR((segment.current_tip - segment.current_base).length(), 1.66F, 0.12F);

  auto const forward_contact = Game::Systems::CombatActions::find_weapon_trace_contact(
      *world,
      *commander,
      *definition,
      {.previous_normalized_time = 0.44F, .current_normalized_time = 0.52F},
      forward_enemy->get_id());
  EXPECT_EQ(forward_contact.target_id, forward_enemy->get_id());

  std::array<EntityID, 1> const ignored_forward{forward_enemy->get_id()};
  auto const broad_shape_contact =
      Game::Systems::CombatActions::find_weapon_trace_contact(
          *world,
          *commander,
          *definition,
          {.previous_normalized_time = 0.44F, .current_normalized_time = 0.52F},
          broad_shape_enemy->get_id(),
          ignored_forward);
  EXPECT_EQ(broad_shape_contact.target_id, 0U)
      << "local_forward=" << broad_shape_contact.local_forward
      << " local_right=" << broad_shape_contact.local_right;
}

TEST_F(CombatModeTest, TimedMountedSwordTraceUsesBakedRidingSocketSegment) {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::k_species_humanoid_sword);
  if (blob == nullptr) {
    GTEST_SKIP() << "baked humanoid sword BPAT asset not loaded";
  }

  auto* commander = make_fpv_mounted_sword_commander(*world, 0.0F, 0.0F);
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedSwordSlash);
  ASSERT_NE(definition, nullptr);

  auto const segment =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *commander,
          *definition,
          {.previous_normalized_time = 0.34F, .current_normalized_time = 0.48F});

  ASSERT_TRUE(segment.valid);
  EXPECT_EQ(segment.source,
            Game::Systems::CombatActions::WeaponTraceSegmentSource::BakedSocket);
  EXPECT_GT((segment.current_base - segment.previous_base).length(), 0.005F);
  EXPECT_NEAR((segment.current_tip - segment.current_base).length(), 0.84F, 0.08F);
}

TEST_F(CombatModeTest, TimedMountedSpearTraceUsesBakedMountedClipSockets) {
  auto* commander = make_fpv_mounted_spear_commander(*world, 0.0F, 0.0F);
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedSpearThrust);
  ASSERT_NE(definition, nullptr);

  auto const segment =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *commander,
          *definition,
          {.previous_normalized_time = 0.30F, .current_normalized_time = 0.48F});

  ASSERT_TRUE(segment.valid);
  EXPECT_EQ(segment.source,
            Game::Systems::CombatActions::WeaponTraceSegmentSource::BakedSocket);
  EXPECT_GT((segment.current_base - segment.previous_base).length(), 0.005F);
  EXPECT_NEAR((segment.current_tip - segment.current_base).length(), 1.66F, 0.12F);
}

TEST_F(CombatModeTest, BakedWeaponTrajectoriesMatchInfantryAndMountedRoles) {
  auto const* infantry_definition =
      Game::Systems::CombatActions::find_combat_action_definition(
          Game::Systems::CombatActions::CombatActionId::RpgSpearThrust);
  auto const* mounted_definition =
      Game::Systems::CombatActions::find_combat_action_definition(
          Game::Systems::CombatActions::CombatActionId::MountedSpearThrust);
  auto const* sword_definition =
      Game::Systems::CombatActions::find_combat_action_definition(
          Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  ASSERT_NE(infantry_definition, nullptr);
  ASSERT_NE(mounted_definition, nullptr);
  ASSERT_NE(sword_definition, nullptr);

  auto* infantry = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* mounted = make_fpv_mounted_spear_commander(*world, 4.0F, 0.0F);
  auto* swordsman = make_fpv_commander(*world, 8.0F, 0.0F);
  auto const infantry_trace =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *infantry,
          *infantry_definition,
          {.previous_normalized_time = 0.32F, .current_normalized_time = 0.50F});
  auto const mounted_trace =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *mounted,
          *mounted_definition,
          {.previous_normalized_time = 0.30F, .current_normalized_time = 0.50F});
  auto const sword_trace =
      Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
          *swordsman,
          *sword_definition,
          {.previous_normalized_time = 0.34F, .current_normalized_time = 0.58F});
  ASSERT_TRUE(infantry_trace.valid);
  ASSERT_TRUE(mounted_trace.valid);
  ASSERT_TRUE(sword_trace.valid);

  QVector3D const infantry_shaft =
      infantry_trace.current_tip - infantry_trace.current_base;
  QVector3D const mounted_shaft =
      mounted_trace.current_tip - mounted_trace.current_base;
  QVector3D const sword_tip_travel = sword_trace.current_tip - sword_trace.previous_tip;
  EXPECT_GT(infantry_shaft.z(), 1.45F)
      << infantry_shaft.x() << "," << infantry_shaft.y() << "," << infantry_shaft.z();
  EXPECT_GT(infantry_shaft.y(), 0.08F)
      << infantry_shaft.x() << "," << infantry_shaft.y() << "," << infantry_shaft.z();
  EXPECT_GT((infantry_trace.current_tip - infantry_trace.previous_tip).length(), 0.30F);
  EXPECT_GT(mounted_shaft.z(), 1.40F)
      << mounted_shaft.x() << "," << mounted_shaft.y() << "," << mounted_shaft.z();
  EXPECT_LT(mounted_shaft.y(), -0.12F)
      << mounted_shaft.x() << "," << mounted_shaft.y() << "," << mounted_shaft.z();
  EXPECT_GT(sword_tip_travel.length(), 0.55F)
      << sword_tip_travel.x() << "," << sword_tip_travel.y() << ","
      << sword_tip_travel.z();
  EXPECT_GT(std::abs(sword_tip_travel.x()), 0.35F)
      << sword_tip_travel.x() << "," << sword_tip_travel.y() << ","
      << sword_tip_travel.z();
}

TEST_F(CombatModeTest, BakedInfantryWeaponTracesMoveThroughoutActiveWindow) {
  auto const* spear_definition =
      Game::Systems::CombatActions::find_combat_action_definition(
          Game::Systems::CombatActions::CombatActionId::RpgSpearThrust);
  auto const* sword_definition =
      Game::Systems::CombatActions::find_combat_action_definition(
          Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  ASSERT_NE(spear_definition, nullptr);
  ASSERT_NE(sword_definition, nullptr);

  auto* spearman = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* swordsman = make_fpv_commander(*world, 4.0F, 0.0F);
  auto moving_intervals = [](Engine::Core::Entity& entity, auto const& definition) {
    int moving = 0;
    int valid = 0;
    for (int sample = 1; sample <= 20; ++sample) {
      float const current = static_cast<float>(sample) / 20.0F;
      float const previous = static_cast<float>(sample - 1) / 20.0F;
      auto const trace =
          Game::Systems::CombatActions::sample_authored_weapon_trace_segment(
              entity,
              definition,
              {.previous_normalized_time = previous,
               .current_normalized_time = current});
      if (!trace.valid) {
        continue;
      }
      ++valid;
      float const tip_travel = (trace.current_tip - trace.previous_tip).length();
      float const base_travel = (trace.current_base - trace.previous_base).length();
      if (tip_travel > 0.025F || base_travel > 0.015F) {
        ++moving;
      }
    }
    return std::pair{moving, valid};
  };

  auto const [spear_moving, spear_valid] =
      moving_intervals(*spearman, *spear_definition);
  auto const [sword_moving, sword_valid] =
      moving_intervals(*swordsman, *sword_definition);
  ASSERT_GE(spear_valid, 4);
  ASSERT_GE(sword_valid, 4);
  EXPECT_GE(spear_moving, spear_valid - 1)
      << "moving/valid intervals=" << spear_moving << "/" << spear_valid;
  EXPECT_GE(sword_moving, sword_valid - 1)
      << "moving/valid intervals=" << sword_moving << "/" << sword_valid;
}

TEST_F(CombatModeTest, TimedSwordTraceRejectsBroadConeTargetOutsideBladeSegment) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* broad_shape_enemy = make_enemy_soldier(*world, 1.50F, 1.20F);
  ASSERT_NE(broad_shape_enemy, nullptr);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  ASSERT_NE(definition, nullptr);

  auto const contact = Game::Systems::CombatActions::find_weapon_trace_contact(
      *world,
      *commander,
      *definition,
      {.previous_normalized_time = 0.44F, .current_normalized_time = 0.52F},
      broad_shape_enemy->get_id());

  EXPECT_EQ(contact.target_id, 0U) << "local_forward=" << contact.local_forward
                                   << " local_right=" << contact.local_right;
}

TEST_F(CombatModeTest, CombatHitResolverRoutesCommanderActionContactToUnitDamage) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* attack = commander->get_component<AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->melee_damage = 14;
  auto* enemy = make_enemy_soldier(*world, 0.0F, 1.4F);

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = enemy->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Sword,
                   .attack_family = CombatAttackFamily::Sword,
                   .attack_direction = AttackDirection::LeftSlash,
                   .contact_point = QVector3D(0.0F, 0.0F, 1.4F),
                   .distance = 1.4F,
                   .local_forward = 1.4F},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_EQ(result.raw_damage, 14);
  EXPECT_FLOAT_EQ(result.contact.relative_speed, 0.0F);
  EXPECT_EQ(result.contact.action_id,
            Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft);
  EXPECT_EQ(result.contact.weapon_family,
            Game::Systems::CombatActions::WeaponFamily::Sword);
  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 86);
}

TEST_F(CombatModeTest, CombatHitResolverAppliesAuthoredDamageMultiplier) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* attack = commander->get_component<AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->melee_damage = 14;
  auto* enemy = make_enemy_soldier(*world, 0.0F, 1.4F);

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher);
  ASSERT_NE(definition, nullptr);

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = enemy->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .attack_family = definition->attack_family,
                   .attack_direction = definition->attack_direction,
                   .contact_point = QVector3D(0.0F, 0.0F, 1.4F),
                   .distance = 1.4F,
                   .local_forward = 1.4F},
       .damage_profile = definition->damage});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_EQ(result.raw_damage, 21);
  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 79);
}

TEST_F(CombatModeTest, CombatHitResolverDerivesRelativeSpeedFromMotionState) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* attacker_motion = commander->add_component<MotionPresentationComponent>();
  attacker_motion->has_velocity = true;
  attacker_motion->velocity_x = 3.0F;
  attacker_motion->velocity_z = 4.0F;

  auto* enemy = make_enemy_soldier(*world, 0.0F, 1.4F);
  auto* target_motion = enemy->add_component<MotionPresentationComponent>();
  target_motion->has_velocity = true;
  target_motion->velocity_x = 0.0F;
  target_motion->velocity_z = 0.0F;

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = enemy->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Sword,
                   .attack_family = CombatAttackFamily::Sword,
                   .attack_direction = AttackDirection::LeftSlash},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_FLOAT_EQ(result.contact.relative_speed, 5.0F);
}

TEST_F(CombatModeTest, SpearThrustInterruptsFastMountedTarget) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* brace = commander->add_component<SpearBraceComponent>();
  brace->active = true;

  auto* cavalry = make_enemy_cavalry(*world, 0.0F, 2.4F);
  auto* movement = cavalry->add_component<MovementComponent>();
  movement->engage_manual_move(0.0F, -6.0F);
  ASSERT_TRUE(movement->get_has_target());

  auto* cavalry_motion = cavalry->add_component<MotionPresentationComponent>();
  cavalry_motion->has_velocity = true;
  cavalry_motion->velocity_x = 0.0F;
  cavalry_motion->velocity_z = -5.5F;

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = cavalry->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSpearThrust,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Spear,
                   .attack_family = CombatAttackFamily::Spear,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 2.4F),
                   .distance = 2.4F,
                   .local_forward = 2.4F},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_TRUE(result.interrupted_charge);
  EXPECT_GE(result.contact.relative_speed, 5.5F);
  EXPECT_FALSE(movement->get_has_target());
  EXPECT_FLOAT_EQ(movement->get_vx(), 0.0F);
  EXPECT_FLOAT_EQ(movement->get_vz(), 0.0F);
  auto* stagger = cavalry->get_component<StaggerComponent>();
  ASSERT_NE(stagger, nullptr);
  EXPECT_EQ(stagger->tier, StaggerTier::Knockdown);
}

TEST_F(CombatModeTest, UnbracedSpearThrustDoesNotInterruptFastMountedTarget) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* cavalry = make_enemy_cavalry(*world, 0.0F, 2.4F);
  auto* movement = cavalry->add_component<MovementComponent>();
  movement->engage_manual_move(0.0F, -6.0F);
  ASSERT_TRUE(movement->get_has_target());

  auto* cavalry_motion = cavalry->add_component<MotionPresentationComponent>();
  cavalry_motion->has_velocity = true;
  cavalry_motion->velocity_x = 0.0F;
  cavalry_motion->velocity_z = -5.5F;

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = cavalry->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSpearThrust,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Spear,
                   .attack_family = CombatAttackFamily::Spear,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 2.4F),
                   .distance = 2.4F,
                   .local_forward = 2.4F},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_FALSE(result.interrupted_charge);
  EXPECT_GE(result.contact.relative_speed, 5.5F);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_EQ(cavalry->get_component<StaggerComponent>(), nullptr);
}

TEST_F(CombatModeTest, HoldModeSpearBraceInterruptsFastMountedTarget) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* hold = commander->add_component<HoldModeComponent>();
  hold->active = true;
  hold->kneel_entry_progress = 1.0F;

  Game::Systems::Combat::process_spear_brace_state(world.get(), 0.0F);
  auto* brace = commander->get_component<SpearBraceComponent>();
  ASSERT_NE(brace, nullptr);
  EXPECT_TRUE(brace->requested);
  EXPECT_TRUE(brace->active);
  EXPECT_EQ(brace->source, SpearBraceSource::HoldMode);

  auto* cavalry = make_enemy_cavalry(*world, 0.0F, 2.4F);
  auto* movement = cavalry->add_component<MovementComponent>();
  movement->engage_manual_move(0.0F, -6.0F);
  ASSERT_TRUE(movement->get_has_target());

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = cavalry->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSpearThrust,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Spear,
                   .attack_family = CombatAttackFamily::Spear,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 2.4F),
                   .distance = 2.4F,
                   .local_forward = 2.4F,
                   .relative_speed = 5.5F},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_TRUE(result.interrupted_charge);
  EXPECT_FALSE(movement->get_has_target());
  auto* stagger = cavalry->get_component<StaggerComponent>();
  ASSERT_NE(stagger, nullptr);
  EXPECT_EQ(stagger->tier, StaggerTier::Knockdown);
}

TEST_F(CombatModeTest, GuardModeSpearBraceRequiresPoseEntryBeforeActive) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* guard = commander->add_component<GuardModeComponent>();
  guard->active = true;

  Game::Systems::Combat::process_spear_brace_state(world.get(), 0.10F);

  auto* brace = commander->get_component<SpearBraceComponent>();
  ASSERT_NE(brace, nullptr);
  EXPECT_TRUE(brace->requested);
  EXPECT_FALSE(brace->active);
  EXPECT_EQ(brace->source, SpearBraceSource::GuardMode);
  EXPECT_GT(brace->pose_progress, 0.0F);
  EXPECT_LT(brace->pose_progress, 0.85F);

  Game::Systems::Combat::process_spear_brace_state(world.get(), 0.20F);
  EXPECT_TRUE(brace->active);
  EXPECT_FLOAT_EQ(brace->pose_progress, 1.0F);
}

TEST_F(CombatModeTest, DerivedSpearBraceExitsWhenGuardIntentEnds) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* guard = commander->add_component<GuardModeComponent>();
  guard->active = true;
  Game::Systems::Combat::process_spear_brace_state(world.get(), 0.30F);

  auto* brace = commander->get_component<SpearBraceComponent>();
  ASSERT_NE(brace, nullptr);
  ASSERT_TRUE(brace->active);

  guard->active = false;
  Game::Systems::Combat::process_spear_brace_state(world.get(), 0.05F);

  EXPECT_FALSE(brace->requested);
  EXPECT_FALSE(brace->active);
  EXPECT_GT(brace->pose_progress, 0.0F);

  Game::Systems::Combat::process_spear_brace_state(world.get(), 0.20F);
  EXPECT_FLOAT_EQ(brace->pose_progress, 0.0F);
}

TEST_F(CombatModeTest, SpearThrustDoesNotInterruptSlowMountedTarget) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* brace = commander->add_component<SpearBraceComponent>();
  brace->active = true;

  auto* cavalry = make_enemy_cavalry(*world, 0.0F, 2.4F);
  auto* movement = cavalry->add_component<MovementComponent>();
  movement->engage_manual_move(0.0F, -6.0F);
  ASSERT_TRUE(movement->get_has_target());

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = commander->get_id(),
                   .target_id = cavalry->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSpearThrust,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Spear,
                   .attack_family = CombatAttackFamily::Spear,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 2.4F),
                   .distance = 2.4F,
                   .local_forward = 2.4F,
                   .relative_speed = 2.0F},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_FALSE(result.interrupted_charge);
  EXPECT_TRUE(movement->get_has_target());
  EXPECT_EQ(cavalry->get_component<StaggerComponent>(), nullptr);
}

TEST_F(CombatModeTest, MountedChargeImpactDefinitionUsesMountFamily) {
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  ASSERT_NE(definition, nullptr);

  EXPECT_EQ(definition->weapon_family,
            Game::Systems::CombatActions::WeaponFamily::Mount);
  EXPECT_EQ(definition->attack_direction, AttackDirection::Thrust);
  EXPECT_GT(definition->damage.base_multiplier, 1.0F);
  EXPECT_GT(definition->max_targets, 1);
  EXPECT_FALSE(definition->events.empty());
  EXPECT_EQ(definition->rider_clip_id, Animation::k_humanoid_riding_charge_clip);
}

TEST_F(CombatModeTest, MountedChargeBodyImpactFindsForwardContact) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* forward_target = make_enemy_soldier(*world, 0.25F, 1.0F);
  auto* side_target = make_enemy_soldier(*world, 20.0F, 1.0F);
  forward_target->get_component<TransformComponent>()->scale = {1.0F, 1.0F, 1.0F};
  side_target->get_component<TransformComponent>()->scale = {1.0F, 1.0F, 1.0F};
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  ASSERT_NE(definition, nullptr);

  auto const forward_contact = Game::Systems::CombatActions::find_body_impact_contact(
      *world, *attacker, *definition, forward_target->get_id());
  EXPECT_EQ(forward_contact.target_id, forward_target->get_id());

  std::array<EntityID, 1> ignored_forward{forward_target->get_id()};
  auto const side_contact = Game::Systems::CombatActions::find_body_impact_contact(
      *world, *attacker, *definition, side_target->get_id(), ignored_forward);
  EXPECT_EQ(side_contact.target_id, 0U)
      << "local_forward=" << side_contact.local_forward
      << " local_right=" << side_contact.local_right
      << " distance=" << side_contact.distance
      << " radius=" << definition->hit_shape.radius;
}

TEST_F(CombatModeTest, MountedChargeImpactKnocksDownAtHighSpeed) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.2F);
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  ASSERT_NE(definition, nullptr);

  auto result = Game::Systems::Combat::resolve_mounted_charge_impact_hit(
      world.get(),
      {.contact =
           {.attacker_id = attacker->get_id(),
            .target_id = target->get_id(),
            .action_id =
                Game::Systems::CombatActions::CombatActionId::MountedChargeImpact,
            .weapon_family = Game::Systems::CombatActions::WeaponFamily::Mount,
            .attack_family = CombatAttackFamily::None,
            .attack_direction = AttackDirection::Thrust,
            .contact_point = QVector3D(0.0F, 0.0F, 1.2F),
            .distance = 1.2F,
            .relative_speed = 6.5F},
       .damage_profile = definition->damage,
       .explicit_raw_damage = 20});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_TRUE(result.interrupted_charge);
  EXPECT_EQ(result.raw_damage, 25);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 75);
  auto* stagger = target->get_component<StaggerComponent>();
  ASSERT_NE(stagger, nullptr);
  EXPECT_EQ(stagger->tier, StaggerTier::Knockdown);
}

TEST_F(CombatModeTest, MountedChargeImpactAtLowSpeedDoesNotStagger) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.2F);
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  ASSERT_NE(definition, nullptr);

  auto result = Game::Systems::Combat::resolve_mounted_charge_impact_hit(
      world.get(),
      {.contact =
           {.attacker_id = attacker->get_id(),
            .target_id = target->get_id(),
            .action_id =
                Game::Systems::CombatActions::CombatActionId::MountedChargeImpact,
            .weapon_family = Game::Systems::CombatActions::WeaponFamily::Mount,
            .attack_family = CombatAttackFamily::None,
            .attack_direction = AttackDirection::Thrust,
            .contact_point = QVector3D(0.0F, 0.0F, 1.2F),
            .distance = 1.2F,
            .relative_speed = 2.0F},
       .damage_profile = definition->damage,
       .explicit_raw_damage = 20});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_FALSE(result.interrupted_charge);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 75);
  EXPECT_EQ(target->get_component<StaggerComponent>(), nullptr);
}

TEST_F(CombatModeTest, RunningRtsCavalryChargeKillsAndLaunchesInfantryOnce) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.2F);
  auto* target_unit = target->get_component<UnitComponent>();
  target_unit->render_individuals_per_unit_override = 8;
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  ASSERT_NE(definition, nullptr);

  auto result = Game::Systems::Combat::resolve_mounted_charge_impact_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .relative_speed = 6.5F},
       .damage_profile = definition->damage,
       .explicit_raw_damage = 18});

  EXPECT_TRUE(result.applied);
  EXPECT_EQ(target_unit->health, 75);
  auto* casualties = target->get_component<SoldierCasualtyAnimationComponent>();
  ASSERT_NE(casualties, nullptr);
  ASSERT_EQ(casualties->entries.size(), 2U);
  EXPECT_TRUE(std::all_of(casualties->entries.begin(),
                          casualties->entries.end(),
                          [](auto const& entry) { return entry.launched; }));
  EXPECT_TRUE(std::all_of(
      casualties->entries.begin(), casualties->entries.end(), [](auto const& entry) {
        return std::hypot(entry.launch_velocity_x, entry.launch_velocity_z) >= 5.0F &&
               entry.launch_velocity_y >= 6.8F && entry.launch_pitch_speed >= 235.0F &&
               std::abs(entry.launch_roll_speed) >= 95.0F;
      }));
  EXPECT_LT(casualties->entries[0].launch_velocity_x *
                casualties->entries[1].launch_velocity_x,
            0.0F);
}

TEST_F(CombatModeTest, HeldSpearsBraceAndKillHalfOfChargingCavalry) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* attacker_unit = attacker->get_component<UnitComponent>();
  attacker_unit->render_individuals_per_unit_override = 10;
  auto* target = make_enemy_soldier(*world, 0.0F, 1.2F);
  auto* target_unit = target->get_component<UnitComponent>();
  target_unit->spawn_type = Game::Units::SpawnType::Spearman;
  target_unit->render_individuals_per_unit_override = 10;
  target->add_component<HoldModeComponent>();
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
  ASSERT_NE(definition, nullptr);

  auto result = Game::Systems::Combat::resolve_mounted_charge_impact_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .relative_speed = 6.5F},
       .damage_profile = definition->damage,
       .explicit_raw_damage = 18});

  EXPECT_TRUE(result.applied);
  EXPECT_EQ(attacker_unit->health, 50);
  EXPECT_EQ(target_unit->health, 100);
  auto* casualties = attacker->get_component<SoldierCasualtyAnimationComponent>();
  ASSERT_NE(casualties, nullptr);
  ASSERT_EQ(casualties->entries.size(), 5U);
  EXPECT_TRUE(std::all_of(casualties->entries.begin(),
                          casualties->entries.end(),
                          [](auto const& entry) { return entry.launched; }));
}

TEST_F(CombatModeTest, HorseArchersCannotEnterMountedChargeImpact) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  attacker->get_component<UnitComponent>()->spawn_type =
      Game::Units::SpawnType::HorseArcher;

  EXPECT_FALSE(Game::Systems::Combat::request_mounted_charge(
      *attacker, MountedChargeIntentSource::Player));
  EXPECT_EQ(attacker->get_component<MountedChargeComponent>(), nullptr);
}

TEST_F(CombatModeTest, MountedChargeActionDealsBodyImpactDuringActiveWindow) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.1F);
  auto* attacker_motion = attacker->add_component<MotionPresentationComponent>();
  attacker_motion->has_velocity = true;
  attacker_motion->velocity_x = 0.0F;
  attacker_motion->velocity_z = 6.5F;

  auto* combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::Strike;
  combat_state->state_time = 0.0F;
  combat_state->state_duration = 0.30F;

  auto* action = start_authored_action_at(
      attacker,
      Game::Systems::CombatActions::CombatActionId::MountedChargeImpact,
      0.18F);
  action->active_target_id = target->get_id();

  Game::Systems::Combat::process_combat_state(world.get(), 0.04F);

  EXPECT_TRUE(action->action_active);
  EXPECT_FALSE(action->weapon_trace_active);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 75);
  EXPECT_EQ(action->last_hit_target_id, target->get_id());
  EXPECT_EQ(action->last_damage, 25);
  EXPECT_EQ(action->hit_target_count, 1U);
  auto* stagger = target->get_component<StaggerComponent>();
  ASSERT_NE(stagger, nullptr);
  EXPECT_EQ(stagger->tier, StaggerTier::Knockdown);

  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  EXPECT_EQ(target->get_component<UnitComponent>()->health, 75);
  EXPECT_EQ(action->hit_target_count, 1U);
}

TEST_F(CombatModeTest, MovingCavalryStartsMountedChargeImpactAction) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* movement = attacker->add_component<MovementComponent>();
  movement->set_manual_velocity(0.0F, 6.5F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.1F);

  EXPECT_EQ(attacker->get_component<CombatStateComponent>(), nullptr);
  EXPECT_EQ(attacker->get_component<RpgCommanderActionComponent>(), nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.12F);

  auto* combat_state = attacker->get_component<CombatStateComponent>();
  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  auto* charge = attacker->get_component<MountedChargeComponent>();
  ASSERT_NE(combat_state, nullptr);
  ASSERT_NE(action, nullptr);
  ASSERT_NE(charge, nullptr);
  EXPECT_EQ(combat_state->animation_state, CombatAnimationState::Strike);
  EXPECT_EQ(action->combat_action_id,
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::MountedChargeImpact));
  EXPECT_EQ(action->active_target_id, target->get_id());
  EXPECT_EQ(action->last_hit_target_id, 0U);
  EXPECT_EQ(action->hit_target_count, 0U);
  EXPECT_EQ(charge->state, MountedChargeState::Charging);
  EXPECT_EQ(charge->active_target_id, target->get_id());

  Game::Systems::Combat::process_combat_state(world.get(), 0.18F);

  EXPECT_EQ(charge->state, MountedChargeState::ImpactActive);
  EXPECT_EQ(action->last_hit_target_id, target->get_id());
  EXPECT_EQ(action->hit_target_count, 1U);
  EXPECT_EQ(charge->last_impact_target_id, target->get_id());
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 75);
}

TEST_F(CombatModeTest, MountedChargeRuntimeBlocksImmediateRestartUntilCooldown) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* movement = attacker->add_component<MovementComponent>();
  movement->set_manual_velocity(0.0F, 6.5F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.1F);
  auto* target_unit = target->get_component<UnitComponent>();
  ASSERT_NE(target_unit, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.12F);
  Game::Systems::Combat::process_combat_state(world.get(), 0.35F);

  auto* charge = attacker->get_component<MountedChargeComponent>();
  ASSERT_NE(charge, nullptr);
  EXPECT_EQ(target_unit->health, 75);
  EXPECT_EQ(charge->state, MountedChargeState::ImpactActive);

  Game::Systems::Combat::process_combat_state(world.get(), 1.20F);

  EXPECT_EQ(target_unit->health, 75);
  EXPECT_EQ(charge->state, MountedChargeState::Cooldown);
  EXPECT_GT(charge->cooldown_remaining, 0.0F);

  Game::Systems::Combat::process_combat_state(world.get(), 0.10F);

  EXPECT_EQ(target_unit->health, 75);
  EXPECT_EQ(charge->state, MountedChargeState::Cooldown);

  for (int i = 0; i < 64 && target_unit->health == 75; ++i) {
    Game::Systems::Combat::process_combat_state(world.get(), 0.02F);
  }

  EXPECT_LT(target_unit->health, 75);
  EXPECT_EQ(charge->last_impact_target_id, target->get_id());
}

TEST_F(CombatModeTest, PlayerChargeIntentWaitsForBodyContact) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* commander = attacker->add_component<CommanderComponent>();
  commander->fpv_controlled = true;
  auto* movement = attacker->add_component<MovementComponent>();
  movement->set_manual_velocity(0.0F, 6.5F);
  auto* charge = attacker->add_component<MountedChargeComponent>();
  charge->auto_contact_enabled = false;

  ASSERT_TRUE(Game::Systems::Combat::request_mounted_charge(
      *attacker, MountedChargeIntentSource::Player));
  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  auto* action = attacker->get_component<RpgCommanderActionComponent>();
  EXPECT_EQ(action, nullptr);
  EXPECT_EQ(charge->state, MountedChargeState::Ready);
  EXPECT_EQ(charge->intent_source, MountedChargeIntentSource::Player);
  EXPECT_EQ(charge->active_target_id, 0U);
  EXPECT_TRUE(charge->intent_requested);
}

TEST_F(CombatModeTest, MountedChargeCancelsAfterSustainedSpeedLoss) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* movement = attacker->add_component<MovementComponent>();
  movement->set_manual_velocity(0.0F, 6.5F);
  auto* charge = attacker->add_component<MountedChargeComponent>();
  charge->auto_contact_enabled = false;
  charge->speed_loss_grace = 0.12F;

  ASSERT_TRUE(Game::Systems::Combat::request_mounted_charge(
      *attacker, MountedChargeIntentSource::AI));
  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);
  ASSERT_EQ(charge->state, MountedChargeState::Ready);

  movement->set_manual_velocity(0.0F, 0.0F);
  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);
  EXPECT_EQ(charge->state, MountedChargeState::Ready);
  Game::Systems::Combat::process_combat_state(world.get(), 0.07F);

  EXPECT_EQ(charge->state, MountedChargeState::Cooldown);
  EXPECT_EQ(charge->last_cancel_reason, MountedChargeCancelReason::SpeedLost);
  EXPECT_EQ(attacker->get_component<RpgCommanderActionComponent>(), nullptr);
}

TEST_F(CombatModeTest, AiChargeIntentArmsAgainstForwardTargetBeforeContact) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* movement = attacker->add_component<MovementComponent>();
  movement->set_manual_velocity(0.0F, 6.5F);
  auto* target = make_enemy_soldier(*world, 0.0F, 6.0F);
  auto* attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = target->get_id();

  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  auto* charge = attacker->get_component<MountedChargeComponent>();
  ASSERT_NE(charge, nullptr);
  EXPECT_EQ(charge->state, MountedChargeState::Ready);
  EXPECT_EQ(charge->intent_source, MountedChargeIntentSource::AI);
  EXPECT_EQ(charge->active_target_id, 0U);
  EXPECT_TRUE(charge->intent_requested);
}

TEST_F(CombatModeTest, MountedChargeRuntimeMinSpeedCanDelayChargeStart) {
  auto* attacker = make_mounted_attacker(*world, 0.0F, 0.0F);
  auto* movement = attacker->add_component<MovementComponent>();
  movement->set_manual_velocity(0.0F, 6.5F);
  auto* charge = attacker->add_component<MountedChargeComponent>();
  charge->min_start_speed = 7.0F;
  auto* target = make_enemy_soldier(*world, 0.0F, 1.1F);

  Game::Systems::Combat::process_combat_state(world.get(), 0.12F);

  EXPECT_EQ(attacker->get_component<CombatStateComponent>(), nullptr);
  EXPECT_EQ(attacker->get_component<RpgCommanderActionComponent>(), nullptr);
  EXPECT_EQ(charge->state, MountedChargeState::Ready);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 100);
}

TEST_F(CombatModeTest, CombatHitResolverUsesCommanderGuardForCommanderTarget) {
  auto* attacker = make_fpv_commander(*world, 0.0F, 1.0F);
  auto* attacker_cmd = attacker->get_component<CommanderComponent>();
  ASSERT_NE(attacker_cmd, nullptr);
  attacker_cmd->combo_step = 2;
  attacker_cmd->power_strike_active = true;

  auto* target = make_enemy_soldier(*world, 0.0F, 0.0F);
  auto* target_cmd = target->add_component<CommanderComponent>();
  ASSERT_NE(target_cmd, nullptr);
  auto* guard = target->add_component<CommanderGuardComponent>();
  guard->active = true;
  guard->perfect_guard_remaining = 0.1F;
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id =
                       Game::Systems::CombatActions::CombatActionId::RpgSwordThrust,
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Sword,
                   .attack_family = CombatAttackFamily::Sword,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 0.0F),
                   .distance = 1.0F,
                   .local_forward = 1.0F},
       .damage_profile = {}});

  EXPECT_TRUE(result.attempted);
  EXPECT_FALSE(result.applied);
  EXPECT_TRUE(result.damage.perfect_guarded);
  EXPECT_EQ(target_rpg->rpg_hp, 100);
  EXPECT_TRUE(attacker->has_component<StaggerComponent>());
  EXPECT_EQ(attacker_cmd->combo_step, 2);
  EXPECT_TRUE(attacker_cmd->power_strike_active);
  EXPECT_GT(target_cmd->punish_window_remaining, 0.0F);
}

TEST_F(CombatModeTest, CombatHitResolverAppliesAuthoredPostureDamageToCommander) {
  auto* attacker = make_fpv_commander(*world, 0.0F, 1.0F);

  auto* target = make_enemy_soldier(*world, 0.0F, 0.0F);
  auto* target_cmd = target->add_component<CommanderComponent>();
  ASSERT_NE(target_cmd, nullptr);
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher);
  ASSERT_NE(definition, nullptr);

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .attack_family = definition->attack_family,
                   .attack_direction = definition->attack_direction,
                   .contact_point = QVector3D(0.0F, 0.0F, 0.0F),
                   .distance = 1.0F,
                   .local_forward = 1.0F},
       .damage_profile = definition->damage});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_EQ(result.raw_damage, 15);
  EXPECT_EQ(target_rpg->rpg_hp, 85);
  EXPECT_FLOAT_EQ(target_cmd->posture, definition->damage.posture_damage);
}

TEST_F(CombatModeTest, CombatHitResolverAppliesAuthoredGuardPressure) {
  auto* attacker = make_fpv_commander(*world, 0.0F, 1.0F);

  auto* target = make_enemy_soldier(*world, 0.0F, 0.0F);
  auto* target_cmd = target->add_component<CommanderComponent>();
  ASSERT_NE(target_cmd, nullptr);
  target_cmd->posture = 80.0F;
  target_cmd->posture_max = 100.0F;
  auto* guard = target->add_component<CommanderGuardComponent>();
  guard->active = true;
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher);
  ASSERT_NE(definition, nullptr);
  ASSERT_GT(definition->damage.guard_pressure, 20.0F);

  auto result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .attack_family = definition->attack_family,
                   .attack_direction = definition->attack_direction,
                   .contact_point = QVector3D(0.0F, 0.0F, 0.0F),
                   .distance = 1.0F,
                   .local_forward = 1.0F},
       .damage_profile = definition->damage});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_TRUE(result.damage.guard_broken);
  EXPECT_FALSE(result.damage.blocked);
  EXPECT_EQ(target_rpg->rpg_hp, 85);
  EXPECT_FALSE(guard->active);
  EXPECT_TRUE(guard->rearm_requires_release);
  EXPECT_GT(guard->guard_break_remaining, 0.0F);
  EXPECT_FLOAT_EQ(target_cmd->posture, target_cmd->posture_max);
  EXPECT_GT(target_cmd->punish_window_remaining, 0.0F);
}

TEST_F(CombatModeTest, CommanderActionContactRespectsDodgeInvulnerability) {
  auto* attacker = make_fpv_commander(*world, 0.0F, 1.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 0.0F);
  target->add_component<CommanderComponent>();
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->dodge_invincible = true;

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordThrust);
  ASSERT_NE(definition, nullptr);
  auto const result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .attack_family = definition->attack_family,
                   .attack_direction = definition->attack_direction},
       .damage_profile = definition->damage});

  EXPECT_TRUE(result.attempted);
  EXPECT_FALSE(result.applied);
  EXPECT_EQ(result.damage.effective_damage, 0);
  EXPECT_EQ(target_rpg->rpg_hp, 100);
}

TEST_F(CombatModeTest, CommanderFinisherUsesPunishOpeningThroughSharedResolver) {
  auto* attacker = make_fpv_commander(*world, 0.0F, 1.0F);
  auto* attacker_commander = attacker->get_component<CommanderComponent>();
  ASSERT_NE(attacker_commander, nullptr);
  attacker_commander->combo_step = 3;

  auto* target = make_enemy_soldier(*world, 0.0F, 0.0F);
  auto* target_commander = target->add_component<CommanderComponent>();
  target_commander->punish_window_remaining = 0.5F;
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      Game::Systems::CombatActions::CombatActionId::RpgSwordFinisher);
  ASSERT_NE(definition, nullptr);
  auto const result = Game::Systems::Combat::resolve_commander_action_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .action_id = definition->id,
                   .weapon_family = definition->weapon_family,
                   .attack_family = definition->attack_family,
                   .attack_direction = definition->attack_direction},
       .damage_profile = definition->damage});

  EXPECT_TRUE(result.applied);
  EXPECT_EQ(result.raw_damage, 15);
  EXPECT_EQ(result.damage.effective_damage, 31);
  EXPECT_EQ(target_rpg->rpg_hp, 69);
  EXPECT_EQ(attacker_commander->combo_step, 0);
}

TEST_F(CombatModeTest, CombatHitResolverRoutesProjectileImpactWithExplicitDamage) {
  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* attack = attacker->get_component<AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->damage = 99;
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::CursedArrow;
  special_attack->use_projectile_system = true;
  special_attack->cursed_duration = 6.0F;
  special_attack->cursed_morale_penalty_per_hit = 8.0F;
  special_attack->cursed_stacks_per_hit = 1;

  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* target_cmd = target->add_component<CommanderComponent>();
  ASSERT_NE(target_cmd, nullptr);
  auto* morale = target->add_component<MoraleComponent>();
  morale->morale = 70.0F;
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  auto result = Game::Systems::Combat::resolve_projectile_impact_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = CombatAttackFamily::Bow,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 5.0F),
                   .from_projectile = true,
                   .projectile_kind = Game::Systems::ProjectileKind::CursedArrow},
       .explicit_raw_damage = 17});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_EQ(result.raw_damage, 17);
  EXPECT_TRUE(result.contact.from_projectile);
  EXPECT_EQ(result.contact.projectile_kind, Game::Systems::ProjectileKind::CursedArrow);
  EXPECT_EQ(target_rpg->rpg_hp, 83);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 100);
  auto* cursed = target->get_component<CursedStatusComponent>();
  ASSERT_NE(cursed, nullptr);
  EXPECT_EQ(cursed->stacks, 1);
  EXPECT_FLOAT_EQ(cursed->duration, 6.0F);
  EXPECT_FLOAT_EQ(cursed->remaining_duration, 6.0F);
  EXPECT_FLOAT_EQ(morale->morale, 62.0F);
}

TEST_F(CombatModeTest, CombatHitResolverCursedProjectileSkipsUndeadTarget) {
  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::CursedArrow;
  special_attack->use_projectile_system = true;
  special_attack->cursed_duration = 6.0F;
  special_attack->cursed_morale_penalty_per_hit = 8.0F;
  special_attack->cursed_stacks_per_hit = 1;

  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);
  target->add_component<UndeadComponent>();
  auto* morale = target->add_component<MoraleComponent>();
  morale->morale = 70.0F;

  auto result = Game::Systems::Combat::resolve_projectile_impact_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = CombatAttackFamily::Bow,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 5.0F),
                   .from_projectile = true,
                   .projectile_kind = Game::Systems::ProjectileKind::CursedArrow},
       .explicit_raw_damage = 17});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 83);
  EXPECT_EQ(target->get_component<CursedStatusComponent>(), nullptr);
  EXPECT_FLOAT_EQ(morale->morale, 70.0F);
}

TEST_F(CombatModeTest, CombatHitResolverFireballAppliesBurningStatus) {
  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->burn_duration = 1.4F;
  special_attack->burn_tick_interval = 0.25F;
  special_attack->burn_damage_per_tick = 2;
  special_attack->bonus_damage_multiplier_vs_fire_vulnerable = 1.7F;

  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);

  auto result = Game::Systems::Combat::resolve_projectile_impact_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = CombatAttackFamily::Bow,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 5.0F),
                   .from_projectile = true,
                   .projectile_kind = Game::Systems::ProjectileKind::Fireball},
       .explicit_raw_damage = 17});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 83);
  auto* burning = target->get_component<BurningStatusComponent>();
  ASSERT_NE(burning, nullptr);
  EXPECT_FLOAT_EQ(burning->duration, 1.4F);
  EXPECT_FLOAT_EQ(burning->remaining_duration, 1.4F);
  EXPECT_FLOAT_EQ(burning->ignition_elapsed, 0.08F);
  EXPECT_FLOAT_EQ(burning->tick_interval, 0.25F);
  EXPECT_EQ(burning->damage_per_tick, 2);
  EXPECT_EQ(burning->attacker_id, attacker->get_id());
  EXPECT_FLOAT_EQ(burning->fire_bonus_multiplier, 1.7F);
}

TEST_F(CombatModeTest, CombatHitResolverFireballRefreshesExistingBurningStatus) {
  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->burn_duration = 1.4F;
  special_attack->burn_tick_interval = 0.25F;
  special_attack->burn_damage_per_tick = 4;
  special_attack->bonus_damage_multiplier_vs_fire_vulnerable = 1.8F;

  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* burning = target->add_component<BurningStatusComponent>();
  burning->duration = 0.6F;
  burning->remaining_duration = 0.2F;
  burning->ignition_elapsed = 0.5F;
  burning->tick_interval = 0.5F;
  burning->damage_per_tick = 1;
  burning->fire_bonus_multiplier = 1.0F;

  auto result = Game::Systems::Combat::resolve_projectile_impact_hit(
      world.get(),
      {.contact = {.attacker_id = attacker->get_id(),
                   .target_id = target->get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = CombatAttackFamily::Bow,
                   .attack_direction = AttackDirection::Thrust,
                   .contact_point = QVector3D(0.0F, 0.0F, 5.0F),
                   .from_projectile = true,
                   .projectile_kind = Game::Systems::ProjectileKind::Fireball},
       .explicit_raw_damage = 17});

  EXPECT_TRUE(result.attempted);
  EXPECT_TRUE(result.applied);
  EXPECT_FLOAT_EQ(burning->duration, 1.4F);
  EXPECT_FLOAT_EQ(burning->remaining_duration, 1.4F);
  EXPECT_FLOAT_EQ(burning->ignition_elapsed, 0.5F);
  EXPECT_FLOAT_EQ(burning->tick_interval, 0.25F);
  EXPECT_EQ(burning->damage_per_tick, 4);
  EXPECT_EQ(burning->attacker_id, attacker->get_id());
  EXPECT_FLOAT_EQ(burning->fire_bonus_multiplier, 1.8F);
}

TEST_F(CombatModeTest, CombatHitResolverFireballAreaImpactSpawnsFirePatch) {
  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* special_attack = attacker->add_component<SpecialAttackComponent>();
  special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
  special_attack->use_projectile_system = true;
  special_attack->splash_radius = 1.5F;
  special_attack->splash_damage_multiplier = 0.5F;
  special_attack->fire_patch_duration = 2.0F;
  special_attack->fire_patch_radius = 1.4F;
  special_attack->burn_duration = 1.2F;
  special_attack->burn_tick_interval = 0.3F;
  special_attack->burn_damage_per_tick = 3;

  auto* primary = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* splash = make_enemy_soldier(*world, 0.6F, 5.2F);
  auto* ally = world->create_entity();
  ally->add_component<TransformComponent>(0.4F, 0.0F, 5.0F);
  auto* ally_unit = ally->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ASSERT_NE(ally_unit, nullptr);
  ally_unit->owner_id = 1;

  auto result = Game::Systems::Combat::resolve_projectile_area_impact_hit(
      world.get(),
      {.attacker_id = attacker->get_id(),
       .primary_target_id = primary->get_id(),
       .impact_point = QVector3D(0.0F, 0.85F, 5.0F),
       .projectile_kind = Game::Systems::ProjectileKind::Fireball,
       .explicit_raw_damage = 20,
       .radius = 1.5F,
       .splash_damage_multiplier = 0.5F,
       .friendly_fire = false});

  EXPECT_EQ(result.attempted_hits, 2);
  EXPECT_EQ(result.applied_hits, 2);
  EXPECT_TRUE(result.fire_patch_spawned);
  EXPECT_EQ(primary->get_component<UnitComponent>()->health, 80);
  EXPECT_EQ(splash->get_component<UnitComponent>()->health, 90);
  EXPECT_EQ(ally_unit->health, 100);
  EXPECT_NE(primary->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_NE(splash->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_EQ(ally->get_component<BurningStatusComponent>(), nullptr);

  auto fire_patches = world->get_entities_with<FirePatchComponent>();
  ASSERT_EQ(fire_patches.size(), 1U);
  auto* fire_patch = fire_patches.front()->get_component<FirePatchComponent>();
  auto* fire_patch_transform =
      fire_patches.front()->get_component<TransformComponent>();
  ASSERT_NE(fire_patch, nullptr);
  ASSERT_NE(fire_patch_transform, nullptr);
  EXPECT_FLOAT_EQ(fire_patch->radius, 1.4F);
  EXPECT_FLOAT_EQ(fire_patch->remaining_duration, 2.0F);
  EXPECT_EQ(fire_patch->attacker_id, attacker->get_id());
  EXPECT_EQ(fire_patch->attacker_owner_id, 1);
  EXPECT_FALSE(fire_patch->friendly_fire);
  EXPECT_FLOAT_EQ(fire_patch_transform->position.y, 0.0F);
}

TEST_F(CombatModeTest, CombatStatusEffectsBurningTickRoutesThroughHitResolver) {
  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* target_unit = target->get_component<UnitComponent>();
  ASSERT_NE(target_unit, nullptr);
  auto* target_cmd = target->add_component<CommanderComponent>();
  ASSERT_NE(target_cmd, nullptr);
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  auto* burning = target->add_component<BurningStatusComponent>();
  burning->remaining_duration = 1.0F;
  burning->tick_interval = 0.25F;
  burning->tick_accumulator = 0.0F;
  burning->damage_per_tick = 7;
  burning->attacker_id = attacker->get_id();

  auto result =
      Game::Systems::Combat::process_combat_status_effects(world.get(), 0.25F);

  EXPECT_EQ(result.burning_ticks, 1);
  EXPECT_EQ(result.expired_burning_statuses, 0);
  EXPECT_EQ(target_rpg->rpg_hp, 93);
  EXPECT_EQ(target_unit->health, 100);
  ASSERT_NE(target->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_FLOAT_EQ(target->get_component<BurningStatusComponent>()->remaining_duration,
                  0.75F);
}

TEST_F(CombatModeTest, CombatStatusEffectsExpireCursesAndFirePatches) {
  auto* cursed_target = make_enemy_soldier(*world, 10.0F, 5.0F);
  auto* cursed = cursed_target->add_component<CursedStatusComponent>();
  cursed->remaining_duration = 0.1F;

  auto* fire_patch_entity = world->create_entity();
  fire_patch_entity->add_component<TransformComponent>(1.0F, 0.0F, 5.0F);
  auto* fire_patch = fire_patch_entity->add_component<FirePatchComponent>();
  fire_patch->radius = 1.25F;
  fire_patch->duration = 0.3F;
  fire_patch->remaining_duration = 0.3F;
  fire_patch->burn_duration = 0.8F;
  fire_patch->burn_tick_interval = 0.2F;
  fire_patch->burn_damage_per_tick = 3;
  fire_patch->attacker_owner_id = 1;
  fire_patch->attacker_id = 77;
  fire_patch->friendly_fire = false;

  auto* burn_target = make_enemy_soldier(*world, 1.2F, 5.1F);
  burn_target->get_component<UnitComponent>()->owner_id = 2;

  auto first = Game::Systems::Combat::process_combat_status_effects(world.get(), 0.1F);

  EXPECT_EQ(first.expired_curses, 1);
  EXPECT_EQ(cursed_target->get_component<CursedStatusComponent>(), nullptr);
  EXPECT_EQ(first.fire_patch_contacts, 1);
  EXPECT_EQ(first.expired_fire_patches, 0);
  ASSERT_NE(burn_target->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_FLOAT_EQ(
      burn_target->get_component<BurningStatusComponent>()->remaining_duration, 0.8F);

  auto second =
      Game::Systems::Combat::process_combat_status_effects(world.get(), 0.25F);

  EXPECT_EQ(second.expired_fire_patches, 1);
  EXPECT_NE(fire_patch_entity->get_component<PendingRemovalComponent>(), nullptr);
}

TEST_F(CombatModeTest, CombatStatusEffectSystemTicksBurningDuringWorldUpdate) {
  world->add_system(std::make_unique<Game::Systems::CombatStatusEffectSystem>());

  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* target_unit = target->get_component<UnitComponent>();
  ASSERT_NE(target_unit, nullptr);
  auto* burning = target->add_component<BurningStatusComponent>();
  burning->remaining_duration = 0.5F;
  burning->tick_interval = 0.1F;
  burning->damage_per_tick = 5;

  world->update(0.1F);

  EXPECT_EQ(target_unit->health, 95);
  ASSERT_NE(target->get_component<BurningStatusComponent>(), nullptr);
  EXPECT_FLOAT_EQ(target->get_component<BurningStatusComponent>()->remaining_duration,
                  0.4F);
}

TEST_F(CombatModeTest, ProjectileSystemImpactRoutesThroughRpgResolver) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* attacker = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* target_cmd = target->add_component<CommanderComponent>();
  ASSERT_NE(target_cmd, nullptr);
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;

  auto* projectile_system = world->get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);
  projectile_system->spawn_arrow(QVector3D(0.0F, 1.15F, 0.0F),
                                 QVector3D(0.0F, 0.85F, 5.0F),
                                 QVector3D(1.0F, 1.0F, 1.0F),
                                 30.0F,
                                 false,
                                 Game::Systems::ProjectileKind::Arrow,
                                 true,
                                 11,
                                 attacker->get_id(),
                                 target->get_id());

  advance_projectiles(*world, 0.6F, 0.05F);

  EXPECT_TRUE(projectile_system->projectiles().empty());
  EXPECT_EQ(target_rpg->rpg_hp, 89);
  EXPECT_EQ(target->get_component<UnitComponent>()->health, 100);
}

TEST_F(CombatModeTest, CommanderActionSwordTraceDealsDamageWhenBladeTouches) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, 1.5F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());
  combat_state->state_duration = 0.30F;

  auto* action = start_authored_action_at(
      commander,
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft,
      0.33F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, 0U);
  EXPECT_EQ(action->hit_target_count, 0U);

  Game::Systems::Combat::process_combat_state(world.get(), 0.18F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 90);
  EXPECT_TRUE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  EXPECT_EQ(action->hit_target_count, 1U);
  EXPECT_EQ(action->hit_target_ids[0], enemy->get_id());
}

TEST_F(CombatModeTest, CommanderActionSwordTraceMissDoesNotUseImpactFallback) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, -1.2F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());
  combat_state->state_duration = 0.30F;

  auto* action = start_authored_action_at(
      commander,
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft,
      0.33F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);
  Game::Systems::Combat::process_combat_state(world.get(), 0.24F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, 0U);
  EXPECT_EQ(action->hit_target_count, 0U);
}

TEST_F(CombatModeTest, CommanderDuelTargetIsHitOnlyOncePerAuthoredSwing) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* target = make_enemy_soldier(*world, 0.0F, 1.5F);
  target->add_component<CommanderComponent>();
  auto* target_rpg = target->add_component<RpgHealthComponent>();
  target_rpg->active = true;
  target_rpg->rpg_hp = 100;
  target_rpg->rpg_max_hp = 100;
  target_rpg->crit_chance = 0.0F;
  auto* combat_state = begin_commander_strike(commander, target->get_id());
  combat_state->state_duration = 0.30F;
  auto* action = start_authored_action_at(
      commander,
      Game::Systems::CombatActions::CombatActionId::RpgSwordSlashLeft,
      0.48F);
  ASSERT_NE(action, nullptr);
  action->active_target_id = target->get_id();

  Game::Systems::Combat::process_combat_state(world.get(), 0.04F);
  int const hp_after_first_contact = target_rpg->rpg_hp;
  Game::Systems::Combat::process_combat_state(world.get(), 0.04F);

  EXPECT_LT(hp_after_first_contact, 100);
  EXPECT_EQ(target_rpg->rpg_hp, hp_after_first_contact);
  EXPECT_EQ(action->hit_target_count, 1U);
  EXPECT_EQ(action->hit_target_ids[0], target->get_id());
}

TEST_F(CombatModeTest, CommanderActionSpearTraceDealsDamageDuringActiveWindow) {
  auto* commander = make_fpv_spear_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, 2.45F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());
  combat_state->state_duration = 0.30F;
  combat_state->attack_family = CombatAttackFamily::Spear;

  auto* action = start_authored_action_at(
      commander, Game::Systems::CombatActions::CombatActionId::RpgSpearThrust, 0.28F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, 0U);
  EXPECT_EQ(action->hit_target_count, 0U);

  Game::Systems::Combat::process_combat_state(world.get(), 0.38F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 89);
  EXPECT_TRUE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  EXPECT_EQ(action->hit_target_count, 1U);
}

TEST_F(CombatModeTest, MountedSwordActionTraceDealsDamageDuringActiveWindow) {
  auto* commander = make_fpv_mounted_sword_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, -0.45F, 0.32F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());
  combat_state->state_duration = 0.30F;
  combat_state->attack_family = CombatAttackFamily::Sword;

  auto* action = start_authored_action_at(
      commander,
      Game::Systems::CombatActions::CombatActionId::MountedSwordSlash,
      0.28F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 79);
  EXPECT_TRUE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  EXPECT_EQ(action->hit_target_count, 1U);
}

TEST_F(CombatModeTest, MountedSpearActionTraceDealsDamageDuringActiveWindow) {
  auto* commander = make_fpv_mounted_spear_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, -0.10F, 2.0F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());
  combat_state->state_duration = 0.30F;
  combat_state->attack_family = CombatAttackFamily::Spear;

  auto* action = start_authored_action_at(
      commander,
      Game::Systems::CombatActions::CombatActionId::MountedSpearThrust,
      0.24F);
  ASSERT_NE(action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.06F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 78);
  EXPECT_TRUE(combat_state->damage_dealt_this_swing);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  EXPECT_EQ(action->hit_target_count, 1U);
}

TEST_F(CombatModeTest, MountedWeaponActionMissDoesNotUseBroadShapeFallback) {
  auto* sword_commander = make_fpv_mounted_sword_commander(*world, 0.0F, 0.0F);
  auto* sword_enemy = make_enemy_soldier(*world, 0.0F, 2.0F);
  auto* sword_state = begin_commander_strike(sword_commander, sword_enemy->get_id());
  sword_state->state_duration = 0.30F;
  sword_state->attack_family = CombatAttackFamily::Sword;
  auto* sword_action = start_authored_action_at(
      sword_commander,
      Game::Systems::CombatActions::CombatActionId::MountedSwordSlash,
      0.28F);
  ASSERT_NE(sword_action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.16F);

  EXPECT_EQ(sword_enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(sword_state->damage_dealt_this_swing);
  EXPECT_EQ(sword_action->last_hit_target_id, 0U);
  EXPECT_EQ(sword_action->hit_target_count, 0U);

  auto* spear_commander = make_fpv_mounted_spear_commander(*world, 4.0F, 0.0F);
  auto* spear_enemy = make_enemy_soldier(*world, 4.0F, 3.0F);
  auto* spear_state = begin_commander_strike(spear_commander, spear_enemy->get_id());
  spear_state->state_duration = 0.30F;
  spear_state->attack_family = CombatAttackFamily::Spear;
  auto* spear_action = start_authored_action_at(
      spear_commander,
      Game::Systems::CombatActions::CombatActionId::MountedSpearThrust,
      0.24F);
  ASSERT_NE(spear_action, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.16F);

  EXPECT_EQ(spear_enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(spear_state->damage_dealt_this_swing);
  EXPECT_EQ(spear_action->last_hit_target_id, 0U);
  EXPECT_EQ(spear_action->hit_target_count, 0U);
}

TEST_F(CombatModeTest, CommanderBowActionReleasesProjectileAtAuthoredEvent) {
  world->add_system(std::make_unique<Game::Systems::ProjectileSystem>());

  auto* commander = make_fpv_bow_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, 5.0F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());
  combat_state->state_duration = 0.30F;
  combat_state->attack_family = CombatAttackFamily::Bow;

  auto* action = start_authored_action_at(
      commander, Game::Systems::CombatActions::CombatActionId::RpgBowShot, 0.39F);
  ASSERT_NE(action, nullptr);

  auto* projectile_system = world->get_system<Game::Systems::ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);

  Game::Systems::Combat::process_combat_state(world.get(), 0.12F);

  EXPECT_TRUE(projectile_system->projectiles().empty());
  EXPECT_FALSE(combat_state->damage_dealt_this_swing);

  Game::Systems::Combat::process_combat_state(world.get(), 0.08F);

  ASSERT_EQ(projectile_system->projectiles().size(), 1U);
  auto const& projectile = projectile_system->projectiles().front();
  EXPECT_TRUE(projectile->should_apply_damage());
  EXPECT_EQ(projectile->get_attacker_id(), commander->get_id());
  EXPECT_EQ(projectile->get_target_id(), enemy->get_id());
  EXPECT_EQ(projectile->get_damage(), 13);
  EXPECT_EQ(action->last_hit_target_id, enemy->get_id());
  EXPECT_EQ(action->last_damage, 13);
  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 100);
}

TEST_F(CombatModeTest, CommanderStrikeWhiffsWhenTargetLeavesReach) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, 6.0F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());

  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(combat_state->damage_dealt_this_swing);
}

TEST_F(CombatModeTest, CommanderStrikeWhiffsWhenTargetBehindCommander) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* enemy = make_enemy_soldier(*world, 0.0F, -1.2F);
  auto* combat_state = begin_commander_strike(commander, enemy->get_id());

  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, 100);
  EXPECT_FALSE(combat_state->damage_dealt_this_swing);
}

TEST_F(CombatModeTest, CommanderStrikeConnectsWithEnemyInSwingArcWhenLockFled) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* fled = make_enemy_soldier(*world, 0.0F, 6.0F);
  auto* in_arc = make_enemy_soldier(*world, 0.3F, 1.4F);
  auto* combat_state = begin_commander_strike(commander, fled->get_id());

  Game::Systems::Combat::process_combat_state(world.get(), 0.05F);

  EXPECT_EQ(fled->get_component<UnitComponent>()->health, 100);
  EXPECT_EQ(in_arc->get_component<UnitComponent>()->health, 90);
  EXPECT_TRUE(combat_state->damage_dealt_this_swing);
}

TEST_F(CombatModeTest, BackRankMeleeEnemyCannotStrikeFpvCommander) {
  auto* commander = make_fpv_commander(*world, 0.0F, 0.0F);
  auto* rpg_health = commander->add_component<RpgHealthComponent>();
  rpg_health->active = true;
  rpg_health->rpg_hp = 100;
  rpg_health->rpg_max_hp = 100;

  auto* front_attacker = make_enemy_soldier(*world, 0.0F, 2.0F);

  auto* back_rank = world->create_entity();
  back_rank->add_component<TransformComponent>(1.2F, 0.0F, 0.0F);
  auto* back_rank_unit = back_rank->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  back_rank_unit->owner_id = 2;
  auto* back_rank_attack = back_rank->add_component<AttackComponent>();
  back_rank_attack->can_melee = true;
  back_rank_attack->can_ranged = false;
  back_rank_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  back_rank_attack->current_mode = AttackComponent::CombatMode::Melee;
  back_rank_attack->melee_range = 1.6F;
  back_rank_attack->melee_damage = 10;
  back_rank_attack->melee_cooldown = 0.0F;
  back_rank_attack->time_since_last = 1.0F;
  auto* attack_target = back_rank->add_component<AttackTargetComponent>();
  attack_target->target_id = commander->get_id();
  attack_target->should_chase = true;

  auto* engagement = commander->add_component<RpgEngagementComponent>();
  engagement->active_attackers = 1;
  engagement->front_attacker_id = front_attacker->get_id();

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context, 0.016F);

  EXPECT_EQ(rpg_health->rpg_hp, 100);

  engagement->front_attacker_id = back_rank->get_id();

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context, 0.016F);

  EXPECT_LT(rpg_health->rpg_hp, 100);
}
